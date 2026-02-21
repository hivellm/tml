//! # LLVM IR Generator - Drop/RAII Support
//!
//! This file implements automatic destructor calls at scope exit.
//!
//! ## RAII in TML
//!
//! Types implementing the `Drop` behavior have their `drop()` method
//! called automatically when they go out of scope.
//!
//! ## Drop Scope Stack
//!
//! `drop_scopes_` tracks variables needing drop per lexical scope:
//!
//! | Method            | Action                              |
//! |-------------------|-------------------------------------|
//! | `push_drop_scope` | Enter new scope (e.g., block)       |
//! | `pop_drop_scope`  | Exit scope                          |
//! | `register_for_drop`| Track variable for later drop      |
//! | `emit_scope_drops`| Emit drop calls at scope exit       |
//!
//! ## Drop Order
//!
//! Drops are emitted in LIFO order (last declared, first dropped),
//! matching Rust's drop semantics.
//!
//! ## Generated Code
//!
//! ```llvm
//! ; At scope exit:
//! call void @tml_Resource_drop(ptr %resource)
//! ```

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Helper to parse mangled type strings for drop instantiation
// Handles: primitives, ptr_X (Ptr[X]), Nested__Y (Nested[Y])
static types::TypePtr parse_mangled_type_for_drop(const std::string& s) {
    // Primitives
    if (s == "I32")
        return types::make_i32();
    if (s == "I64")
        return types::make_i64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "F32")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (s == "F64")
        return types::make_f64();
    if (s == "Str")
        return types::make_str();
    if (s == "I8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (s == "I16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (s == "U8")
        return types::make_primitive(types::PrimitiveKind::U8);
    if (s == "U16")
        return types::make_primitive(types::PrimitiveKind::U16);
    if (s == "U32")
        return types::make_primitive(types::PrimitiveKind::U32);
    if (s == "U64")
        return types::make_primitive(types::PrimitiveKind::U64);

    // Pointer prefix: ptr_X -> Ptr[X]
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        auto inner = parse_mangled_type_for_drop(s.substr(4));
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.inner = inner};
            return t;
        }
    }

    // Nested generic: Base__Arg -> Base[Arg]
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_for_drop(arg_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::NamedType{base, "", {inner}};
            return t;
        }
    }

    // Simple struct type
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

void LLVMIRGen::mark_var_consumed(const std::string& var_name) {
    consumed_vars_.insert(var_name);
}

void LLVMIRGen::mark_field_consumed(const std::string& var_name, const std::string& field_name) {
    // Mark the specific field as consumed: "var.field"
    consumed_vars_.insert(var_name + "." + field_name);
    TML_DEBUG_LN("[DROP] Marked field as consumed: " << var_name << "." << field_name);
}

bool LLVMIRGen::has_consumed_fields(const std::string& var_name) const {
    // Check if any field of this variable has been consumed
    std::string prefix = var_name + ".";
    for (const auto& consumed : consumed_vars_) {
        if (consumed.size() > prefix.size() && consumed.substr(0, prefix.size()) == prefix) {
            return true;
        }
    }
    return false;
}

void LLVMIRGen::push_drop_scope() {
    drop_scopes_.push_back({});
}

void LLVMIRGen::pop_drop_scope() {
    if (!drop_scopes_.empty()) {
        drop_scopes_.pop_back();
    }
}

void LLVMIRGen::register_for_drop(const std::string& var_name, const std::string& var_reg,
                                  const std::string& type_name, const std::string& llvm_type) {
    if (type_name.empty()) {
        return;
    }

    // Only register for drop if the type directly implements Drop.
    // Types that are non-trivially-destructible only because their fields
    // have Drop impls need recursive field-level drops (not yet supported).
    // For now, we only call drop on types that explicitly implement Drop.
    bool has_drop = env_.type_implements(type_name, "Drop");

    // For generic types (MutexGuard__I32), check the base type too
    if (!has_drop) {
        auto sep_pos = type_name.find("__");
        if (sep_pos != std::string::npos) {
            std::string base_type = type_name.substr(0, sep_pos);
            has_drop = env_.type_implements(base_type, "Drop");

            // Also check source code for generic Drop impls
            if (!has_drop && env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    if (!mod.source_code.empty()) {
                        std::string pattern = "Drop for " + base_type + "[";
                        if (mod.source_code.find(pattern) != std::string::npos) {
                            has_drop = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (!has_drop) {
        return;
    }

    TML_DEBUG_LN("[DROP] Registering " << var_name << " for drop, type=" << type_name);

    if (!drop_scopes_.empty()) {
        drop_scopes_.back().push_back(DropInfo{var_name, var_reg, type_name, llvm_type});

        // For generic imported types, request Drop method instantiation
        // This handles types like MutexGuard__I32 from std::sync
        auto sep_pos = type_name.find("__");
        if (sep_pos != std::string::npos) {
            std::string base_type = type_name.substr(0, sep_pos);

            // Check if Drop impl method already generated
            std::string drop_key = "tml_" + type_name + "_drop";
            TML_DEBUG_LN("[DROP]   base_type=" << base_type << ", drop_key=" << drop_key
                                               << ", already_generated="
                                               << (generated_impl_methods_.find(drop_key) !=
                                                   generated_impl_methods_.end()));
            if (generated_impl_methods_.find(drop_key) == generated_impl_methods_.end()) {
                // Build type_subs from mangled name
                std::unordered_map<std::string, types::TypePtr> type_subs;
                std::string remaining = type_name.substr(sep_pos + 2);

                // Parse mangled type parameter (handles ptr_X, Nested__Y, etc.)
                types::TypePtr type_arg = parse_mangled_type_for_drop(remaining);

                if (type_arg) {
                    type_subs["T"] = type_arg;
                    pending_impl_method_instantiations_.push_back(
                        PendingImplMethod{type_name, "drop", type_subs, base_type, "",
                                          /*is_library_type=*/true});
                    generated_impl_methods_.insert(drop_key);

                    // Pre-register in functions_ so emit_drop_call can find it
                    // Library types don't use suite prefix
                    std::string method_name = type_name + "_drop";
                    std::string func_llvm_name = "tml_" + type_name + "_drop";
                    functions_[method_name] =
                        FuncInfo{"@" + func_llvm_name, "void (ptr)", "void", {"ptr"}};
                }
            }
        } else {
            // Non-generic type with Drop impl (e.g., Condvar, DroppableResource)
            // Need to ensure the drop function exists and is findable
            std::string method_name = type_name + "_drop";
            if (functions_.find(method_name) == functions_.end()) {
                // Determine if this is a library type or a local (test-defined) type
                // Library types are from imported modules (std::*, core::*)
                // Local types are defined in the current compilation unit
                bool is_library = false;
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (mod_name.starts_with("std::") || mod_name.starts_with("core::")) {
                            if (mod.structs.count(type_name) || mod.classes.count(type_name)) {
                                is_library = true;
                                break;
                            }
                        }
                    }
                }

                std::string prefix = is_library ? "" : get_suite_prefix();
                std::string func_llvm_name = "tml_" + prefix + type_name + "_drop";
                functions_[method_name] =
                    FuncInfo{"@" + func_llvm_name, "void (ptr)", "void", {"ptr"}};
                // Request instantiation
                std::unordered_map<std::string, types::TypePtr> empty_subs;
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{type_name, "drop", empty_subs, type_name, "",
                                      /*is_library_type=*/is_library});
                generated_impl_methods_.insert(func_llvm_name);
            }
        }
    }
}

void LLVMIRGen::register_heap_str_for_drop(const std::string& var_name,
                                           const std::string& var_reg) {
    if (!drop_scopes_.empty()) {
        DropInfo di;
        di.var_name = var_name;
        di.var_reg = var_reg;
        di.type_name = "Str";
        di.llvm_type = "ptr";
        di.is_heap_str = true;
        drop_scopes_.back().push_back(di);
    }
}

void LLVMIRGen::emit_drop_call(const DropInfo& info) {
    // Heap-allocated Str: safely free via tml_str_free.
    // tml_str_free validates the pointer is a genuine heap allocation
    // before calling free(), so it's safe for global string constants too.
    if (info.is_heap_str) {
        std::string ptr_val = fresh_reg();
        emit_line("  " + ptr_val + " = load ptr, ptr " + info.var_reg);
        emit_line("  call void @tml_str_free(ptr " + ptr_val + ")");
        return;
    }

    // Load the value from the variable's alloca
    std::string value_reg = fresh_reg();
    emit_line("  " + value_reg + " = load " + info.llvm_type + ", ptr " + info.var_reg);

    // Create a pointer to pass to drop (drop takes mut this)
    // Actually, for drop we pass the pointer directly since it's `mut this`
    // The drop function signature is: void @tml_TypeName_drop(ptr %this)
    // Look up in functions_ to get the correct LLVM name
    std::string drop_lookup_key = info.type_name + "_drop";
    auto drop_it = functions_.find(drop_lookup_key);
    std::string drop_func;
    if (drop_it != functions_.end()) {
        drop_func = drop_it->second.llvm_name;
    } else {
        // Fallback: use suite prefix for test-local types
        drop_func = "@tml_" + get_suite_prefix() + info.type_name + "_drop";
    }
    emit_line("  call void " + drop_func + "(ptr " + info.var_reg + ")");

    // For @pool(thread_local: true) classes, release to thread-local pool
    if (tls_pool_classes_.count(info.type_name) > 0) {
        // Get class type for size calculation
        std::string class_type = "%class." + info.type_name;
        emit_line("  call void @tls_pool_release(ptr @pool.name." + info.type_name + ", ptr " +
                  info.var_reg + ", i64 ptrtoint (" + class_type + "* getelementptr (" +
                  class_type + ", " + class_type + "* null, i32 1) to i64))");
    }
    // For @pool classes (non-thread-local), release to global pool
    else if (pool_classes_.count(info.type_name) > 0) {
        emit_line("  call void @pool_release(ptr @pool." + info.type_name + ", ptr " +
                  info.var_reg + ")");
    }
}

void LLVMIRGen::emit_scope_drops() {
    if (drop_scopes_.empty()) {
        return;
    }

    // Emit drops in reverse order (LIFO - last declared is dropped first)
    // Skip variables that have been consumed (moved into struct fields, etc.)
    // Also skip variables that have partial moves (some fields consumed)
    const auto& current_scope = drop_scopes_.back();
    for (auto it = current_scope.rbegin(); it != current_scope.rend(); ++it) {
        // Skip if the whole variable was consumed
        if (consumed_vars_.find(it->var_name) != consumed_vars_.end()) {
            continue;
        }
        // Skip if any field of this variable was consumed (partial move)
        // TODO: In the future, we could emit drop calls for individual non-moved fields
        if (has_consumed_fields(it->var_name)) {
            TML_DEBUG_LN("[DROP] Skipping drop for " << it->var_name << " due to partial move");
            continue;
        }
        emit_drop_call(*it);
    }
}

void LLVMIRGen::emit_all_drops() {
    // Emit drops for all scopes in reverse order (innermost to outermost)
    // Skip variables that have been consumed (moved into struct fields, etc.)
    for (auto scope_it = drop_scopes_.rbegin(); scope_it != drop_scopes_.rend(); ++scope_it) {
        // Within each scope, drop in reverse declaration order
        for (auto it = scope_it->rbegin(); it != scope_it->rend(); ++it) {
            // Skip if the whole variable was consumed
            if (consumed_vars_.find(it->var_name) != consumed_vars_.end()) {
                continue;
            }
            // Skip if any field of this variable was consumed (partial move)
            if (has_consumed_fields(it->var_name)) {
                TML_DEBUG_LN("[DROP] Skipping drop for "
                             << it->var_name << " in emit_all_drops due to partial move");
                continue;
            }
            emit_drop_call(*it);
        }
    }
}

} // namespace tml::codegen
