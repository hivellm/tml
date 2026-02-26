TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Dynamic Dispatch
//!
//! This file implements vtables for `dyn Behavior` types.
//!
//! ## Vtable Structure
//!
//! Each `impl Behavior for Type` generates a vtable:
//! ```llvm
//! @vtable.Point.Display = global [1 x ptr] [ptr @Point_display]
//! ```
//!
//! ## Dyn Type Layout
//!
//! `dyn Behavior` is a fat pointer: `{ data: ptr, vtable: ptr }`
//!
//! ## Key Methods
//!
//! | Method            | Purpose                              |
//! |-------------------|--------------------------------------|
//! | `register_impl`   | Register impl for vtable generation  |
//! | `emit_dyn_type`   | Emit fat pointer struct              |
//! | `get_vtable`      | Get vtable name for type+behavior    |
//! | `emit_vtables`    | Emit all registered vtables          |
//!
//! ## Method Order
//!
//! `behavior_method_order_` ensures consistent vtable slot ordering
//! across all implementations of a behavior.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// ============ Vtable Support ============

void LLVMIRGen::register_impl(const parser::ImplDecl* impl) {
    pending_impls_.push_back(impl);

    // Eagerly populate behavior_method_order_ and vtables_ for dyn dispatch
    std::string behavior_name;
    if (impl->trait_type && impl->trait_type->is<parser::NamedType>()) {
        const auto& named = impl->trait_type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            behavior_name = named.path.segments.back();
        }
    }
    if (!behavior_name.empty()) {

        // Populate behavior_method_order_ only once per behavior
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            auto behavior_def = env_.lookup_behavior(behavior_name);
            if (behavior_def) {
                std::vector<std::string> methods;
                for (const auto& m : behavior_def->methods) {
                    methods.push_back(m.name);
                }
                behavior_method_order_[behavior_name] = methods;
            }
        }

        // Register vtable for EVERY impl (not just the first per behavior)
        std::string type_name;
        if (impl->self_type->kind.index() == 0) {
            const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
            if (!named.path.segments.empty()) {
                type_name = named.path.segments.back();
            }
        }
        if (!type_name.empty()) {
            std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
            std::string key = type_name + "::" + behavior_name;
            vtables_[key] = vtable_name;
        }
    }
}

void LLVMIRGen::emit_dyn_type(const std::string& behavior_name) {
    if (emitted_dyn_types_.count(behavior_name))
        return;
    emitted_dyn_types_.insert(behavior_name);

    // Emit the dyn type as a fat pointer struct: { data_ptr, vtable_ptr }
    // Write to type_defs_buffer_ to ensure type definition appears before use
    TML_DEBUG_LN("[DYN] Emitting dyn type: %dyn." << behavior_name);
    type_defs_buffer_ << "%dyn." << behavior_name << " = type { ptr, ptr }\n";
}

auto LLVMIRGen::get_vtable(const std::string& type_name, const std::string& behavior_name)
    -> std::string {
    std::string key = type_name + "::" + behavior_name;

    // First check behavior vtables (impl blocks)
    auto it = vtables_.find(key);
    if (it != vtables_.end()) {
        return it->second;
    }

    // Check interface vtables (class implements)
    auto iface_it = interface_vtables_.find(key);
    if (iface_it != interface_vtables_.end()) {
        return iface_it->second;
    }

    return ""; // No vtable found
}

void LLVMIRGen::emit_vtables() {
    // For each registered impl block, generate a vtable
    for (const auto* impl : pending_impls_) {
        if (!impl->trait_type)
            continue; // Skip inherent impls

        // Get the type name and behavior name
        std::string type_name;
        if (impl->self_type->kind.index() == 0) { // NamedType
            const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
            if (!named.path.segments.empty()) {
                type_name = named.path.segments.back();
            }
        }

        std::string behavior_name;
        if (impl->trait_type->is<parser::NamedType>()) {
            const auto& named = impl->trait_type->as<parser::NamedType>();
            if (!named.path.segments.empty()) {
                behavior_name = named.path.segments.back();
            }
        }

        if (type_name.empty() || behavior_name.empty())
            continue;

        // Build vtable name and check if already emitted
        std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
        if (emitted_vtables_.count(vtable_name)) {
            TML_DEBUG_LN("[DYN] Skipping duplicate vtable: " << vtable_name);
            continue;
        }
        emitted_vtables_.insert(vtable_name);

        // Emit the dyn type for this behavior
        emit_dyn_type(behavior_name);

        // Get behavior method order
        auto behavior_def = env_.lookup_behavior(behavior_name);
        if (!behavior_def)
            continue;

        // Collect non-generic method indices (methods with own generics can't be dispatched)
        // Check both FuncSig.type_params AND the parser TraitDecl.generics as source of truth,
        // since cached modules may not have type_params populated for older cache files.
        std::set<std::string> generic_method_names;
        auto trait_it_check = trait_decls_.find(behavior_name);
        if (trait_it_check != trait_decls_.end()) {
            for (const auto& tm : trait_it_check->second->methods) {
                if (!tm.generics.empty()) {
                    generic_method_names.insert(tm.name);
                }
            }
        }

        std::vector<size_t> dispatchable_method_indices;
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            const auto& m = behavior_def->methods[i];
            if (m.type_params.empty() &&
                generic_method_names.find(m.name) == generic_method_names.end()) {
                dispatchable_method_indices.push_back(i);
            }
        }

        // Build vtable type: array of function pointers (only dispatchable methods)
        std::string vtable_type = "{ ";
        for (size_t i = 0; i < dispatchable_method_indices.size(); ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";

        // Collect method names that the impl explicitly provides
        std::set<std::string> impl_method_names;
        for (const auto& m : impl->methods) {
            impl_method_names.insert(m.name);
        }

        // First pass: collect all function references and check which need declarations
        std::vector<std::string> vtable_entries;
        std::vector<std::string> missing_decls;
        bool all_methods_available = true;

        for (size_t di = 0; di < dispatchable_method_indices.size(); ++di) {
            size_t i = dispatchable_method_indices[di];
            const auto& method = behavior_def->methods[i];

            // Look up in functions_ to get the correct LLVM name
            // Try both with and without suite prefix, since impl methods for test-local
            // types are registered with suite prefix in suite mode
            std::string method_lookup_key = type_name + "_" + method.name;
            auto method_it = functions_.find(method_lookup_key);

            // If not found, try with suite prefix (for test-local types in suite mode)
            if (method_it == functions_.end() && !get_suite_prefix().empty()) {
                std::string prefixed_key = get_suite_prefix() + method_lookup_key;
                method_it = functions_.find(prefixed_key);
            }
            std::string func_name;

            if (method_it != functions_.end()) {
                // Method was compiled in this compilation unit, use its registered name
                func_name = method_it->second.llvm_name;
            } else {
                // Method not found in local functions_
                // Check if it's explicitly provided by the impl block
                bool explicitly_provided = (impl_method_names.count(method.name) > 0);

                // If explicitly provided but not in functions_, it means the impl comes
                // from an imported module and wasn't compiled in this unit - skip this vtable
                if (explicitly_provided) {
                    TML_DEBUG_LN("[DYN] Skipping vtable " << vtable_name << " - method "
                                                          << method.name
                                                          << " from imported module");
                    all_methods_available = false;
                    break;
                }

                // Not explicitly provided - check if it has a default implementation
                // AND if we have the trait definition to generate it
                bool has_default = (behavior_def->methods_with_defaults.count(method.name) > 0);
                bool can_generate_default =
                    has_default && (trait_decls_.find(behavior_name) != trait_decls_.end());

                if (!has_default) {
                    // Required method with no default and not provided by impl - can't generate
                    // vtable
                    TML_DEBUG_LN("[DYN] Skipping vtable "
                                 << vtable_name << " - missing required method " << method.name);
                    all_methods_available = false;
                    break;
                }

                if (!can_generate_default) {
                    // Has default but we don't have the trait definition to generate it
                    // This happens when impl comes from imported module
                    TML_DEBUG_LN("[DYN] Skipping vtable "
                                 << vtable_name << " - cannot generate default for " << method.name
                                 << " (trait " << behavior_name << " not in trait_decls_)");
                    all_methods_available = false;
                    break;
                }

                // Use non-prefixed name for default methods
                func_name = "@tml_" + type_name + "_" + method.name;
                // Mark as needing an external declaration only if not already generated
                if (functions_.find(type_name + "_" + method.name) == functions_.end()) {
                    missing_decls.push_back(func_name);
                }
            }
            vtable_entries.push_back(func_name);
        }

        // Skip this vtable if not all methods are available
        if (!all_methods_available) {
            continue;
        }

        // Generate missing default implementations
        // Instead of just declaring them (which causes link errors), actually generate the
        // function bodies using the trait's default method implementation
        if (!missing_decls.empty()) {
            auto trait_it = trait_decls_.find(behavior_name);
            if (trait_it != trait_decls_.end()) {
                const auto* trait_decl = trait_it->second;

                // Set up type substitutions for resolving return types
                auto saved_type_subs = current_type_subs_;
                {
                    auto this_type = std::make_shared<types::Type>();
                    this_type->kind = types::NamedType{type_name, "", {}};
                    current_type_subs_["This"] = this_type;
                    current_type_subs_["Self"] = this_type;
                }
                if (impl) {
                    for (const auto& binding : impl->type_bindings) {
                        if (binding.type) {
                            auto resolved =
                                resolve_parser_type_with_subs(*binding.type, current_type_subs_);
                            if (resolved) {
                                current_type_subs_["This::" + binding.name] = resolved;
                                current_type_subs_[binding.name] = resolved;
                            }
                        }
                    }
                }

                for (const auto& decl : missing_decls) {
                    // Extract method name from "@tml_TypeName_method"
                    std::string prefix = "@tml_" + type_name + "_";
                    if (decl.find(prefix) != 0)
                        continue;
                    std::string method_name_only = decl.substr(prefix.size());

                    // Find the trait method declaration
                    bool generated = false;
                    for (const auto& trait_method : trait_decl->methods) {
                        if (trait_method.name == method_name_only) {
                            generated =
                                generate_default_method(type_name, trait_decl, trait_method, impl);
                            break;
                        }
                    }

                    // If generation failed (e.g. method has func ptr params, closures, etc.),
                    // emit a minimal stub that returns a default value to satisfy the vtable
                    // reference
                    if (!generated) {
                        // Find the behavior method definition to determine the signature
                        for (size_t mi = 0; mi < behavior_def->methods.size(); ++mi) {
                            if (behavior_def->methods[mi].name == method_name_only) {
                                // Build a stub with the right number of ptr params
                                // The vtable stores function pointers - first arg is always ptr
                                // (self)
                                std::string stub_params = "ptr %this";
                                // Add any additional parameters as ptr (opaque)
                                if (trait_it != trait_decls_.end()) {
                                    for (const auto& tm : trait_decl->methods) {
                                        if (tm.name == method_name_only) {
                                            for (size_t pi = 1; pi < tm.params.size(); ++pi) {
                                                stub_params += ", ptr %_p" + std::to_string(pi);
                                            }
                                            break;
                                        }
                                    }
                                }

                                // Determine return type from the trait method
                                std::string stub_ret = "void";
                                for (const auto& tm : trait_decl->methods) {
                                    if (tm.name == method_name_only && tm.return_type.has_value()) {
                                        auto resolved_ret = resolve_parser_type_with_subs(
                                            **tm.return_type, current_type_subs_);
                                        if (resolved_ret) {
                                            stub_ret = llvm_type_from_semantic(resolved_ret);
                                        } else {
                                            stub_ret = llvm_type_ptr(*tm.return_type);
                                        }
                                        if (stub_ret.find("This") != std::string::npos) {
                                            stub_ret = "%struct." + type_name;
                                        }
                                        break;
                                    }
                                }

                                emit_line("");
                                emit_line("; Stub for unimplemented default method " +
                                          method_name_only);
                                emit_line("define internal " + stub_ret + " " + decl + "(" +
                                          stub_params + ") #0 {");
                                emit_line("entry:");
                                // Call panic with a message about the unimplemented method
                                std::string panic_msg =
                                    "called unimplemented method: " + type_name + "." +
                                    method_name_only;
                                std::string str_name =
                                    "@.str.stub_" + type_name + "_" + method_name_only;
                                string_literals_.emplace_back(str_name, panic_msg);
                                emit_line("  call void @panic(ptr " + str_name + ")");
                                emit_line("  unreachable");
                                emit_line("}");
                                break;
                            }
                        }
                    }
                }
                current_type_subs_ = saved_type_subs;
            }
        }

        // Build vtable value with function pointers
        std::string vtable_value = "{ ";
        for (size_t i = 0; i < vtable_entries.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";
            vtable_value += "ptr " + vtable_entries[i];
        }
        vtable_value += " }";

        // Emit vtable global constant
        emit_line(vtable_name + " = internal constant " + vtable_type + " " + vtable_value);

        // Register vtable
        std::string key = type_name + "::" + behavior_name;
        vtables_[key] = vtable_name;

        // Store method order for this behavior (only dispatchable methods)
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            std::vector<std::string> methods;
            for (size_t idx : dispatchable_method_indices) {
                methods.push_back(behavior_def->methods[idx].name);
            }
            behavior_method_order_[behavior_name] = methods;
        }
    }
}

bool LLVMIRGen::generate_default_method(const std::string& type_name,
                                        const parser::TraitDecl* trait_decl,
                                        const parser::FuncDecl& trait_method,
                                        const parser::ImplDecl* impl) {
    // Skip if trait method has no default implementation
    if (!trait_method.body.has_value())
        return false;

    // Skip methods with their own generic parameters
    if (!trait_method.generics.empty())
        return false;

    // Skip methods with where clauses
    if (trait_method.where_clause.has_value() &&
        (!trait_method.where_clause->constraints.empty() ||
         !trait_method.where_clause->type_equalities.empty()))
        return false;

    // Skip methods with function pointer parameters
    bool has_func_ptr_param = false;
    for (const auto& param : trait_method.params) {
        if (param.type && param.type->is<parser::FuncType>()) {
            has_func_ptr_param = true;
            break;
        }
    }
    if (has_func_ptr_param)
        return false;

    std::string method_name = type_name + "_" + trait_method.name;

    // Skip if already generated
    if (functions_.find(method_name) != functions_.end())
        return false;

    current_func_ = method_name;
    current_impl_type_ = type_name;
    locals_.clear();
    block_terminated_ = false;

    // Set up type substitutions for associated types
    auto saved_type_subs = current_type_subs_;
    auto saved_associated_types = current_associated_types_;
    {
        auto this_type = std::make_shared<types::Type>();
        this_type->kind = types::NamedType{type_name, "", {}};
        current_type_subs_["This"] = this_type;
        current_type_subs_["Self"] = this_type;
    }
    if (impl) {
        for (const auto& binding : impl->type_bindings) {
            if (binding.type) {
                auto resolved = resolve_parser_type_with_subs(*binding.type, current_type_subs_);
                if (resolved) {
                    current_type_subs_["This::" + binding.name] = resolved;
                    current_type_subs_[binding.name] = resolved;
                    // Also set current_associated_types_ so that resolve_parser_type_with_subs
                    // can resolve This::Item paths (e.g. in Maybe[This::Item] return types)
                    current_associated_types_[binding.name] = resolved;
                }
            }
        }
    }

    // Determine return type
    std::string ret_type = "void";
    if (trait_method.return_type.has_value()) {
        auto resolved_ret =
            resolve_parser_type_with_subs(**trait_method.return_type, current_type_subs_);
        if (resolved_ret) {
            ret_type = llvm_type_from_semantic(resolved_ret);
            // Ensure generic types in return position are instantiated
            ensure_generic_types_instantiated(resolved_ret);
        } else {
            ret_type = llvm_type_ptr(*trait_method.return_type);
            // For unresolved types, try to ensure instantiation from the string form
            // e.g. %struct.Outcome__Unit__I64 needs the type definition
            if (ret_type.find("%struct.") == 0 && ret_type.find("__") != std::string::npos) {
                // Extract base name and type args from the mangled name
                std::string mangled = ret_type.substr(8); // strip %struct.
                auto sep = mangled.find("__");
                if (sep != std::string::npos) {
                    std::string base = mangled.substr(0, sep);
                    // Try to instantiate this type based on the full mangled name
                    if (env_.all_enums().find(base) != env_.all_enums().end()) {
                        // Parse type args from mangled name (e.g., "Unit__I64" -> [Unit, I64])
                        std::string args_str = mangled.substr(sep + 2);
                        std::vector<types::TypePtr> type_args;
                        size_t pos = 0;
                        while (pos < args_str.size()) {
                            auto next_sep = args_str.find("__", pos);
                            std::string arg_name = (next_sep != std::string::npos)
                                                       ? args_str.substr(pos, next_sep - pos)
                                                       : args_str.substr(pos);
                            // Map arg_name to a TypePtr
                            types::TypePtr arg_type;
                            if (arg_name == "I32")
                                arg_type = types::make_i32();
                            else if (arg_name == "I64")
                                arg_type = types::make_i64();
                            else if (arg_name == "Bool")
                                arg_type = types::make_bool();
                            else if (arg_name == "Str")
                                arg_type = types::make_str();
                            else if (arg_name == "Unit")
                                arg_type = types::make_unit();
                            else if (arg_name == "F32")
                                arg_type = types::make_primitive(types::PrimitiveKind::F32);
                            else if (arg_name == "F64")
                                arg_type = types::make_f64();
                            else {
                                // Named type (struct/enum)
                                auto t = std::make_shared<types::Type>();
                                t->kind = types::NamedType{arg_name, "", {}};
                                arg_type = t;
                            }
                            type_args.push_back(arg_type);
                            if (next_sep == std::string::npos)
                                break;
                            pos = next_sep + 2;
                        }
                        if (!type_args.empty()) {
                            require_enum_instantiation(base, type_args);
                        }
                    }
                }
            }
        }
        if (ret_type.find("This") != std::string::npos) {
            ret_type = "%struct." + type_name;
        }
    }
    current_ret_type_ = ret_type;

    // Build parameter list
    std::string params;
    std::string param_types;
    std::vector<std::string> param_types_vec;

    std::string trait_impl_llvm_type = llvm_type_name(type_name);
    bool trait_is_primitive_impl = (trait_impl_llvm_type[0] != '%');

    for (size_t i = 0; i < trait_method.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
            param_types += ", ";
        }
        std::string param_type;
        auto resolved_param =
            resolve_parser_type_with_subs(*trait_method.params[i].type, current_type_subs_);
        if (resolved_param) {
            param_type = llvm_type_from_semantic(resolved_param);
        } else {
            param_type = llvm_type_ptr(trait_method.params[i].type);
        }
        std::string param_name;
        if (trait_method.params[i].pattern &&
            trait_method.params[i].pattern->is<parser::IdentPattern>()) {
            param_name = trait_method.params[i].pattern->as<parser::IdentPattern>().name;
        } else {
            param_name = "_anon";
        }
        if ((param_name == "this" || param_name == "self") &&
            (param_type.find("This") != std::string::npos ||
             param_type.find(type_name) != std::string::npos)) {
            param_type = trait_is_primitive_impl ? trait_impl_llvm_type : "ptr";
        }
        params += param_type + " %" + param_name;
        param_types += param_type;
        param_types_vec.push_back(param_type);
    }

    // Register function
    std::string func_type = ret_type + " (" + param_types + ")";
    functions_[method_name] = FuncInfo{"@tml_" + method_name, func_type, ret_type, param_types_vec};

    // Generate function
    emit_line("");
    emit_line("; Default implementation from behavior " + trait_decl->name);
    emit_line("define internal " + ret_type + " @tml_" + method_name + "(" + params + ") #0 {");
    emit_line("entry:");

    // Register params in locals
    for (size_t i = 0; i < trait_method.params.size(); ++i) {
        std::string param_type;
        auto resolved_local =
            resolve_parser_type_with_subs(*trait_method.params[i].type, current_type_subs_);
        if (resolved_local) {
            param_type = llvm_type_from_semantic(resolved_local);
        } else {
            param_type = llvm_type_ptr(trait_method.params[i].type);
        }
        std::string param_name;
        if (trait_method.params[i].pattern &&
            trait_method.params[i].pattern->is<parser::IdentPattern>()) {
            param_name = trait_method.params[i].pattern->as<parser::IdentPattern>().name;
        } else {
            param_name = "_anon";
        }

        types::TypePtr semantic_type = resolved_local;
        if ((param_name == "this" || param_name == "self") &&
            (param_type.find("This") != std::string::npos ||
             param_type.find(type_name) != std::string::npos || param_type == "ptr")) {
            param_type = trait_is_primitive_impl ? trait_impl_llvm_type : "ptr";
            semantic_type = std::make_shared<types::Type>();
            semantic_type->kind = types::NamedType{type_name, "", {}};
        }

        if (param_name == "this" || param_name == "self") {
            locals_["this"] = VarInfo{"%" + param_name, param_type, semantic_type, std::nullopt};
            locals_["self"] = VarInfo{"%" + param_name, param_type, semantic_type, std::nullopt};
        } else {
            std::string alloca_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + param_type);
            emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
            locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};
        }
    }

    // Check if the method body might call cross-module methods that aren't available.
    // For Error::description() which calls this.to_string() (from Display),
    // the Display impl may be in a different module. Fall back to returning a string literal.
    bool use_stub_body = false;
    if (trait_method.name == "description" && trait_decl->name == "Error") {
        // Check if the type's to_string function is available
        std::string to_string_key = type_name + "_to_string";
        if (functions_.find(to_string_key) == functions_.end()) {
            // to_string not available - generate a simple string return
            std::string desc_str = add_string_literal(type_name);
            emit_line("  ret ptr " + desc_str);
            use_stub_body = true;
        }
    }
    if (!use_stub_body) {
        // Generate body normally
        gen_block(*trait_method.body);
    }
    if (!use_stub_body && !block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "ptr") {
            emit_line("  ret ptr null");
        } else if (ret_type.find("%struct.") == 0 || ret_type == "{}") {
            emit_line("  ret " + ret_type + " zeroinitializer");
        } else {
            emit_line("  ret " + ret_type + " 0");
        }
    }
    emit_line("}");
    current_impl_type_.clear();
    current_type_subs_ = saved_type_subs;
    current_associated_types_ = saved_associated_types;
    return true;
}

} // namespace tml::codegen
