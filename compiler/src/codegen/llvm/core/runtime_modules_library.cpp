TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Referenced Library Codegen
//!
//! This file emits LLVM IR for library methods and functions that are
//! actually referenced by compiled user code (lazy/on-demand emission).
//!
//! ## Emitted Sections
//!
//! | Method                                  | Emits                                  |
//! |-----------------------------------------|----------------------------------------|
//! | `emit_referenced_library_definitions`   | Full function bodies for library uses  |
//! | `emit_referenced_library_declarations`  | Forward declarations for library uses  |
//!
//! Extracted from runtime_modules.cpp to keep individual files manageable.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <sstream>
#include <unordered_set>

namespace tml::codegen {

// Static helper to parse mangled type strings like "Mutex__I32" into proper TypePtr.
// Used by emit_referenced_library_definitions to reconstruct type substitutions
// for generic type method instantiation.
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    if (s == "I64")
        return types::make_i64();
    if (s == "I32")
        return types::make_i32();
    if (s == "I8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I8};
        return t;
    }
    if (s == "I16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I16};
        return t;
    }
    if (s == "U8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U8};
        return t;
    }
    if (s == "U16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U16};
        return t;
    }
    if (s == "U32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U32};
        return t;
    }
    if (s == "U64") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Usize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Isize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I64};
        return t;
    }
    if (s == "F32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::F32};
        return t;
    }
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();

    if (s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.inner = inner};
            return t;
        }
    }

    // Check if s is a numeric string (const generic value like "3")
    if (!s.empty() && (std::isdigit(s[0]) || (s[0] == '-' && s.size() > 1 && std::isdigit(s[1])))) {
        try {
            int64_t val = std::stoll(s);
            auto t = std::make_shared<types::Type>();
            t->kind = types::ConstGenericType{s, types::make_i64(), val};
            return t;
        } catch (...) {
            // Not a valid number, fall through
        }
    }

    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_string(arg_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::NamedType{base, "", {inner}};
            return t;
        }
    }

    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

/// Emits full function bodies for library methods and functions referenced by user code.
/// Uses a worklist algorithm to transitively resolve all dependencies.
void LLVMIRGen::emit_referenced_library_definitions() {
    if (pending_library_methods_.empty() && pending_library_funcs_.empty()) {
        return;
    }

    // Helper: scan IR text for @tml_ function name references
    auto collect_refs = [](const std::string& ir) -> std::unordered_set<std::string> {
        std::unordered_set<std::string> refs;
        size_t pos = 0;
        while ((pos = ir.find("@tml_", pos)) != std::string::npos) {
            size_t start = pos;
            pos += 5;
            while (pos < ir.size() && (std::isalnum(ir[pos]) || ir[pos] == '_')) {
                ++pos;
            }
            refs.insert(ir.substr(start, pos - start));
        }
        return refs;
    };

    // Scan the current IR output to find all referenced library functions.
    std::string original_ir = output_.str();
    auto referenced = collect_refs(original_ir);

    // Seed worklist with referenced functions that have pending definitions
    std::unordered_set<std::string> worklist;
    for (const auto& name : referenced) {
        if (pending_library_methods_.count(name) || pending_library_funcs_.count(name)) {
            worklist.insert(name);
        }
    }

    if (worklist.empty() && pending_impl_method_instantiations_.empty()) {
        return;
    }

    // Save codegen state
    auto saved_module_prefix = current_module_prefix_;
    auto saved_module_name = current_module_name_;
    auto saved_submodule = current_submodule_name_;
    auto saved_in_library_body = in_library_body_;
    in_library_body_ = true;

    // We generate definitions by temporarily using output_ (since gen_impl_method
    // and gen_func_decl write to it via emit_line). We save the original IR,
    // generate into a clean output_, then restore original + append new defs.
    std::string all_lazy_defs;
    std::unordered_set<std::string> generated;

    while (!worklist.empty()) {
        std::unordered_set<std::string> next_worklist;

        // Clear output_ and generate this round's definitions into it
        output_.str("");
        output_.clear();

        for (const auto& fn : worklist) {
            if (generated.count(fn))
                continue;
            generated.insert(fn);

            // If the function was already fully defined by another codegen path
            // (e.g., pending impl method instantiation flush for generics), skip it
            // to avoid emitting a duplicate `define` (LLVM redefinition error).
            // Check generated_impl_methods_output_ which tracks actually-generated
            // method instantiations.
            {
                // fn has "@" prefix, e.g. "@tml_BTreeMap_insert"
                std::string fn_no_at = fn.substr(1);
                if (generated_impl_methods_output_.count(fn_no_at) > 0)
                    continue;
            }

            auto method_it = pending_library_methods_.find(fn);
            if (method_it != pending_library_methods_.end()) {
                const auto& info = method_it->second;
                current_module_prefix_ = info.module_prefix;
                current_module_name_ = info.module_name;
                current_submodule_name_ = info.submodule_name;
                options_.lazy_library_defs = false;
                generated_functions_.erase(fn);

                // For generic type methods (e.g., LockFreeStack__I32::push),
                // set up type substitutions so the body can resolve type params.
                // Extract base type and type args from the mangled type name.
                auto saved_type_subs = current_type_subs_;
                auto saved_const_generic_values = current_const_generic_values_;
                auto dunder = info.type_name.find("__");
                if (dunder != std::string::npos) {
                    std::string base = info.type_name.substr(0, dunder);
                    std::string suffix = info.type_name.substr(dunder + 2);
                    // Look up impl generics from pending_generic_impls_
                    auto impl_it = pending_generic_impls_.find(base);
                    if (impl_it != pending_generic_impls_.end()) {
                        const auto& impl_block = *impl_it->second;
                        if (impl_block.generics.size() == 1) {
                            auto type_arg = parse_mangled_type_string(suffix);
                            if (type_arg) {
                                current_type_subs_[impl_block.generics[0].name] = type_arg;
                                // For const generic params, also set the value
                                if (impl_block.generics[0].is_const &&
                                    type_arg->is<types::ConstGenericType>()) {
                                    const auto& cgt = type_arg->as<types::ConstGenericType>();
                                    if (cgt.resolved_value.has_value()) {
                                        current_const_generic_values_[impl_block.generics[0].name] =
                                            *cgt.resolved_value;
                                    }
                                }
                            }
                        } else if (impl_block.generics.size() > 1) {
                            // Multi-param: split on "__"
                            std::vector<std::string> parts;
                            size_t pos = 0;
                            while (pos < suffix.size()) {
                                size_t next = suffix.find("__", pos);
                                if (next == std::string::npos) {
                                    parts.push_back(suffix.substr(pos));
                                    break;
                                }
                                parts.push_back(suffix.substr(pos, next - pos));
                                pos = next + 2;
                            }
                            for (size_t gi = 0;
                                 gi < impl_block.generics.size() && gi < parts.size(); ++gi) {
                                auto type_arg = parse_mangled_type_string(parts[gi]);
                                if (type_arg) {
                                    current_type_subs_[impl_block.generics[gi].name] = type_arg;
                                    // For const generic params, also set the value
                                    if (impl_block.generics[gi].is_const &&
                                        type_arg->is<types::ConstGenericType>()) {
                                        const auto& cgt = type_arg->as<types::ConstGenericType>();
                                        if (cgt.resolved_value.has_value()) {
                                            current_const_generic_values_[impl_block.generics[gi]
                                                                              .name] =
                                                *cgt.resolved_value;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                emit_line("; DEBUG LAZY type_name=" + info.type_name +
                          " method=" + info.method->name);
                gen_impl_method(info.type_name, *info.method);
                current_type_subs_ = saved_type_subs;
                current_const_generic_values_ = saved_const_generic_values;
                options_.lazy_library_defs = true;
                continue;
            }

            auto func_it = pending_library_funcs_.find(fn);
            if (func_it != pending_library_funcs_.end()) {
                const auto& finfo = func_it->second;
                current_module_prefix_ = finfo.module_prefix;
                current_module_name_ = finfo.module_name;
                current_submodule_name_ = finfo.submodule_name;
                options_.lazy_library_defs = false;
                generated_functions_.erase(fn);
                gen_func_decl(*finfo.func);
                options_.lazy_library_defs = true;
                continue;
            }
        }

        // Capture this round's output
        std::string round_ir = output_.str();
        all_lazy_defs += round_ir;

        // Check for new transitive references in the generated code
        auto new_refs = collect_refs(round_ir);
        for (const auto& name : new_refs) {
            if (!generated.count(name) &&
                (pending_library_methods_.count(name) || pending_library_funcs_.count(name))) {
                next_worklist.insert(name);
            }
        }

        worklist = next_worklist;
    }

    // After the main worklist, flush pending generic instantiations and resolve
    // any new transitive references they introduce. This loop handles chains like:
    //   user code -> Barrier::wait -> Mutex[BarrierState]::lock (instantiation)
    //             -> AtomicU32::store (pending method) -> ...
    // Each iteration flushes instantiations and resolves their transitive deps.
    for (int flush_round = 0; flush_round < 10; ++flush_round) {
        // Flush pending impl method instantiations queued during lazy generation.
        if (!pending_impl_method_instantiations_.empty()) {
            output_.str("");
            output_.clear();
            generate_pending_instantiations();
            std::string pending_inst_ir = output_.str();
            if (!pending_inst_ir.empty()) {
                all_lazy_defs += "\n; Lazy transitive instantiations (round " +
                                 std::to_string(flush_round) + ")\n";
                all_lazy_defs += pending_inst_ir;

                // Scan flushed instantiations for new transitive references
                auto inst_refs = collect_refs(pending_inst_ir);
                for (const auto& name : inst_refs) {
                    if (!generated.count(name) && (pending_library_methods_.count(name) ||
                                                   pending_library_funcs_.count(name))) {
                        worklist.insert(name);
                    }
                }
            }
        }

        // If we found new pending references, generate them
        if (worklist.empty())
            break;

        while (!worklist.empty()) {
            std::unordered_set<std::string> next_worklist;
            output_.str("");
            output_.clear();

            for (const auto& fn : worklist) {
                if (generated.count(fn))
                    continue;
                generated.insert(fn);

                {
                    std::string fn_no_at = fn.substr(1);
                    if (generated_impl_methods_output_.count(fn_no_at) > 0)
                        continue;
                }

                auto method_it = pending_library_methods_.find(fn);
                if (method_it != pending_library_methods_.end()) {
                    const auto& info = method_it->second;
                    current_module_prefix_ = info.module_prefix;
                    current_module_name_ = info.module_name;
                    current_submodule_name_ = info.submodule_name;
                    options_.lazy_library_defs = false;
                    generated_functions_.erase(fn);

                    // Set up type subs for generic type methods (same as main worklist)
                    auto saved_type_subs = current_type_subs_;
                    auto dunder = info.type_name.find("__");
                    if (dunder != std::string::npos) {
                        std::string base = info.type_name.substr(0, dunder);
                        std::string suffix = info.type_name.substr(dunder + 2);
                        auto impl_it = pending_generic_impls_.find(base);
                        if (impl_it != pending_generic_impls_.end()) {
                            const auto& impl_block = *impl_it->second;
                            if (impl_block.generics.size() == 1) {
                                auto type_arg = parse_mangled_type_string(suffix);
                                if (type_arg) {
                                    current_type_subs_[impl_block.generics[0].name] = type_arg;
                                }
                            } else if (impl_block.generics.size() > 1) {
                                std::vector<std::string> parts;
                                size_t pos = 0;
                                while (pos < suffix.size()) {
                                    size_t next = suffix.find("__", pos);
                                    if (next == std::string::npos) {
                                        parts.push_back(suffix.substr(pos));
                                        break;
                                    }
                                    parts.push_back(suffix.substr(pos, next - pos));
                                    pos = next + 2;
                                }
                                for (size_t gi = 0;
                                     gi < impl_block.generics.size() && gi < parts.size(); ++gi) {
                                    auto type_arg = parse_mangled_type_string(parts[gi]);
                                    if (type_arg) {
                                        current_type_subs_[impl_block.generics[gi].name] = type_arg;
                                    }
                                }
                            }
                        }
                    }

                    gen_impl_method(info.type_name, *info.method);
                    current_type_subs_ = saved_type_subs;
                    options_.lazy_library_defs = true;
                    continue;
                }

                auto func_it = pending_library_funcs_.find(fn);
                if (func_it != pending_library_funcs_.end()) {
                    const auto& finfo = func_it->second;
                    current_module_prefix_ = finfo.module_prefix;
                    current_module_name_ = finfo.module_name;
                    current_submodule_name_ = finfo.submodule_name;
                    options_.lazy_library_defs = false;
                    generated_functions_.erase(fn);
                    gen_func_decl(*finfo.func);
                    options_.lazy_library_defs = true;
                    continue;
                }
            }

            std::string round_ir = output_.str();
            all_lazy_defs += round_ir;

            auto new_refs = collect_refs(round_ir);
            for (const auto& name : new_refs) {
                if (!generated.count(name) &&
                    (pending_library_methods_.count(name) || pending_library_funcs_.count(name))) {
                    next_worklist.insert(name);
                }
            }
            worklist = next_worklist;
        }
        // Loop back to check if the new definitions queued more instantiations
    }

    // Restore codegen state
    current_module_prefix_ = saved_module_prefix;
    current_module_name_ = saved_module_name;
    current_submodule_name_ = saved_submodule;
    in_library_body_ = saved_in_library_body;

    // Collect any new type definitions generated during lazy pass
    // (e.g., MutexGuard__BarrierState from generic struct instantiation).
    // Type defs MUST appear before functions that use them.
    std::string lazy_type_defs = type_defs_buffer_.str();
    type_defs_buffer_.str("");
    type_defs_buffer_.clear();

    // Restore output_: original IR + lazy type defs + lazy function definitions.
    output_.str("");
    output_.clear();
    output_ << original_ir;
    if (!lazy_type_defs.empty()) {
        output_ << "\n; Lazy library type instantiations\n";
        output_ << lazy_type_defs;
    }
    output_ << "\n; Lazy library definitions (only functions actually used)\n";
    output_ << all_lazy_defs;

    // Scan lazily-generated definitions for runtime function references
    // (e.g., @f32_to_string from lowlevel blocks) so finalize_runtime_decls()
    // emits the necessary declarations. finalize_runtime_decls() deduplicates
    // against already-declared/defined functions in the output.
    scan_for_runtime_refs(all_lazy_defs);

    TML_DEBUG_LN("[LAZY_LIB] Generated "
                 << generated.size() << " of "
                 << (pending_library_methods_.size() + pending_library_funcs_.size())
                 << " library functions");

    // Verify and recover: scan final IR for any @tml_ calls that lack a define or declare.
    // For unresolved generic type methods (e.g., @tml_StackNode__I32_new), attempt to
    // queue them as pending impl method instantiations and flush. This handles cases where
    // generate_pending_instantiations() generated a method body that calls internal generic
    // type methods which weren't queued through the normal path.
    in_library_body_ = true; // Recovery needs library body mode
    for (int verify_round = 0; verify_round < 3; ++verify_round) {
        std::string final_ir = output_.str();
        auto all_refs = collect_refs(final_ir);

        // Collect all defined/declared functions
        std::unordered_set<std::string> defined_or_declared;
        std::istringstream stream(final_ir);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("define ") != std::string::npos ||
                line.find("declare ") != std::string::npos) {
                auto at_pos = line.find("@tml_");
                if (at_pos != std::string::npos) {
                    size_t start = at_pos;
                    size_t pos = at_pos + 5;
                    while (pos < line.size() && (std::isalnum(line[pos]) || line[pos] == '_'))
                        ++pos;
                    defined_or_declared.insert(line.substr(start, pos - start));
                }
            }
        }

        bool queued_recovery = false;
        for (const auto& ref : all_refs) {
            if (defined_or_declared.count(ref) == 0) {
                std::string name_no_at = ref.size() > 1 ? ref.substr(1) : ref;
                if (runtime_catalog_index_.count(name_no_at) > 0) {
                    require_runtime_decl(name_no_at);
                    continue;
                }

                bool is_enum_drop = (ref.find("_drop") != std::string::npos &&
                                     name_no_at.find("_s") != std::string::npos);
                if (is_enum_drop) {
                    TML_LOG_DEBUG(
                        "codegen",
                        "[LAZY_LIB] Auto-declaring unreferenced enum drop function: " << ref);
                    deferred_runtime_decls_ += "declare dso_local void " + ref + "(ptr) #0\n";
                    continue;
                }

                // Recovery: try to queue unresolved generic type methods for instantiation.
                // Pattern: @tml_<Type>__<TypeArg>_<method> (e.g., @tml_StackNode__I32_new)
                // Strip the "tml_" prefix and look for a double-underscore type separator.
                std::string func_name = name_no_at;
                if (func_name.substr(0, 4) == "tml_") {
                    func_name = func_name.substr(4);
                }
                // Also strip mangled module prefix like "N3std4sync5stack"
                // by looking for a leading "N" followed by length-prefixed segments.
                if (!func_name.empty() && func_name[0] == 'N') {
                    // Skip over N<len><name> segments until we hit an uppercase letter
                    // that starts a type name (not part of a module segment).
                    // Actually, simpler: look for the pattern Type__TypeArg_method directly.
                    // The function names are like: StackNode__I32_new (no module prefix for
                    // internal) or N3std4sync5stack22StackNode__I32_new for module-prefixed ones.
                    // For now, just handle the simple non-prefixed case.
                }

                auto dunder = func_name.find("__");
                if (dunder != std::string::npos) {
                    std::string base_type = func_name.substr(0, dunder);
                    std::string rest = func_name.substr(dunder + 2);
                    // rest = "I32_new" or "I32_free" — split on last underscore for method name
                    auto last_underscore = rest.rfind('_');
                    if (last_underscore != std::string::npos && last_underscore > 0) {
                        std::string type_arg_str = rest.substr(0, last_underscore);
                        std::string method_name = rest.substr(last_underscore + 1);
                        std::string mangled_type = base_type + "__" + type_arg_str;

                        // Check if this base type has a generic impl
                        auto impl_it = pending_generic_impls_.find(base_type);
                        if (impl_it != pending_generic_impls_.end()) {
                            const auto& impl_block = *impl_it->second;
                            // Verify the impl has this method
                            bool has_method = false;
                            std::string available_methods;
                            for (const auto& m : impl_block.methods) {
                                if (!available_methods.empty())
                                    available_methods += ", ";
                                available_methods += m.name;
                                if (m.name == method_name) {
                                    has_method = true;
                                }
                            }
                            if (has_method) {
                                std::string mangled_method =
                                    mangle_impl_method(mangled_type, method_name);
                                if (generated_impl_methods_output_.count(mangled_method) == 0) {
                                    // Build type_subs from the type arg
                                    std::unordered_map<std::string, types::TypePtr> type_subs;
                                    if (impl_block.generics.size() == 1) {
                                        auto type_arg = parse_mangled_type_string(type_arg_str);
                                        if (type_arg) {
                                            type_subs[impl_block.generics[0].name] = type_arg;
                                        }
                                    }
                                    pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                        mangled_type, method_name, type_subs, base_type, "",
                                        /*is_library_type=*/true});
                                    generated_impl_methods_.insert(mangled_method);
                                    queued_recovery = true;
                                    TML_LOG_DEBUG("codegen", "[LAZY_LIB] Recovery: queued "
                                                                 << base_type << "::" << method_name
                                                                 << " for instantiation (type_arg="
                                                                 << type_arg_str << ")");
                                }
                            }
                        }
                    }
                }

                // Try to resolve from pending library methods/funcs.
                // These are deferred library functions that were registered but
                // not yet emitted because lazy mode only generates on-demand.
                if (pending_library_methods_.count(ref) || pending_library_funcs_.count(ref)) {
                    // Found in pending — queue for generation
                    if (!generated.count(ref)) {
                        auto method_it = pending_library_methods_.find(ref);
                        if (method_it != pending_library_methods_.end()) {
                            auto saved_prefix = current_module_prefix_;
                            auto saved_name = current_module_name_;
                            auto saved_sub = current_submodule_name_;
                            current_module_prefix_ = method_it->second.module_prefix;
                            current_module_name_ = method_it->second.module_name;
                            current_submodule_name_ = method_it->second.submodule_name;
                            in_library_body_ = true;
                            current_impl_type_ = method_it->second.type_name;
                            gen_impl_method(method_it->second.type_name, *method_it->second.method);
                            current_impl_type_.clear();
                            current_module_prefix_ = saved_prefix;
                            current_module_name_ = saved_name;
                            current_submodule_name_ = saved_sub;
                            generated.insert(ref);
                            queued_recovery = true;
                            TML_LOG_DEBUG("codegen",
                                          "[LAZY_LIB] Recovery: emitted pending method " << ref);
                        }
                        auto func_it = pending_library_funcs_.find(ref);
                        if (func_it != pending_library_funcs_.end()) {
                            auto saved_prefix = current_module_prefix_;
                            auto saved_name = current_module_name_;
                            auto saved_sub = current_submodule_name_;
                            current_module_prefix_ = func_it->second.module_prefix;
                            current_module_name_ = func_it->second.module_name;
                            current_submodule_name_ = func_it->second.submodule_name;
                            in_library_body_ = true;
                            gen_func_decl(*func_it->second.func);
                            current_module_prefix_ = saved_prefix;
                            current_module_name_ = saved_name;
                            current_submodule_name_ = saved_sub;
                            generated.insert(ref);
                            queued_recovery = true;
                            TML_LOG_DEBUG("codegen",
                                          "[LAZY_LIB] Recovery: emitted pending func " << ref);
                        }
                    }
                } else if (!queued_recovery && verify_round == 2) {
                    // Final round: genuinely unresolved reference
                    TML_LOG_WARN("codegen", "[LAZY_LIB] UNRESOLVED: " << ref);
                }
            }
        }

        // If we queued recovery instantiations, flush them and re-verify
        if (queued_recovery && !pending_impl_method_instantiations_.empty()) {
            output_.str("");
            output_.clear();
            // Restore the full IR first
            output_ << final_ir;

            // Generate the recovered instantiations
            std::stringstream recovery_output;
            auto saved_output = output_.str();
            output_.str("");
            output_.clear();
            generate_pending_instantiations();
            std::string recovery_ir = output_.str();
            if (!recovery_ir.empty()) {
                output_.str("");
                output_.clear();
                output_ << saved_output;
                output_ << "\n; Recovery instantiations (round " << verify_round << ")\n";
                output_ << recovery_ir;

                // Also resolve any transitive pending library methods
                auto recovery_refs = collect_refs(recovery_ir);
                std::unordered_set<std::string> recovery_worklist;
                for (const auto& name : recovery_refs) {
                    if (!generated.count(name) && (pending_library_methods_.count(name) ||
                                                   pending_library_funcs_.count(name))) {
                        recovery_worklist.insert(name);
                    }
                }
                if (!recovery_worklist.empty()) {
                    // Generate transitive dependencies
                    while (!recovery_worklist.empty()) {
                        std::unordered_set<std::string> next;
                        std::string rnd_ir_saved = output_.str();
                        output_.str("");
                        output_.clear();
                        for (const auto& fn : recovery_worklist) {
                            if (generated.count(fn))
                                continue;
                            generated.insert(fn);
                            auto method_it = pending_library_methods_.find(fn);
                            if (method_it != pending_library_methods_.end()) {
                                const auto& info = method_it->second;
                                current_module_prefix_ = info.module_prefix;
                                current_module_name_ = info.module_name;
                                current_submodule_name_ = info.submodule_name;
                                options_.lazy_library_defs = false;
                                generated_functions_.erase(fn);
                                gen_impl_method(info.type_name, *info.method);
                                options_.lazy_library_defs = true;
                            }
                            auto func_it = pending_library_funcs_.find(fn);
                            if (func_it != pending_library_funcs_.end()) {
                                const auto& finfo = func_it->second;
                                current_module_prefix_ = finfo.module_prefix;
                                current_module_name_ = finfo.module_name;
                                current_submodule_name_ = finfo.submodule_name;
                                options_.lazy_library_defs = false;
                                generated_functions_.erase(fn);
                                gen_func_decl(*finfo.func);
                                options_.lazy_library_defs = true;
                            }
                        }
                        std::string extra_ir = output_.str();
                        output_.str("");
                        output_.clear();
                        output_ << rnd_ir_saved << extra_ir;
                        auto extra_refs = collect_refs(extra_ir);
                        for (const auto& name : extra_refs) {
                            if (!generated.count(name) && (pending_library_methods_.count(name) ||
                                                           pending_library_funcs_.count(name))) {
                                next.insert(name);
                            }
                        }
                        recovery_worklist = next;
                    }
                }
            } else {
                // No recovery IR generated, restore original
                output_.str("");
                output_.clear();
                output_ << saved_output;
            }
            continue; // Re-verify
        }
        break; // No recovery needed, exit verification loop
    }

    // Restore codegen state
    current_module_prefix_ = saved_module_prefix;
    current_module_name_ = saved_module_name;
    current_submodule_name_ = saved_submodule;
    in_library_body_ = saved_in_library_body;
}

/// Emits forward declarations for library methods and functions referenced by user code.
/// Used in library declaration mode where implementations come from a shared library object.
void LLVMIRGen::emit_referenced_library_declarations() {
    if (pending_library_methods_.empty() && pending_library_funcs_.empty()) {
        return;
    }

    // Scan current IR for referenced @tml_ function names
    std::string current_ir = output_.str();
    size_t pos = 0;
    std::unordered_set<std::string> referenced;
    while ((pos = current_ir.find("@tml_", pos)) != std::string::npos) {
        size_t start = pos;
        pos += 5;
        while (pos < current_ir.size() &&
               (std::isalnum(current_ir[pos]) || current_ir[pos] == '_')) {
            ++pos;
        }
        referenced.insert(current_ir.substr(start, pos - start));
    }

    // Helper to emit a declare from FuncInfo
    auto emit_declare = [&](const FuncInfo& fi, std::ostringstream& out) -> bool {
        auto paren_pos = fi.llvm_func_type.find('(');
        if (paren_pos == std::string::npos)
            return false;
        // llvm_func_type is "ret_type (param1, param2)" — extract "(param1, param2)"
        std::string params = fi.llvm_func_type.substr(paren_pos + 1);
        // params includes closing ")", e.g. "ptr, i32)"
        out << "declare dso_local " << fi.ret_type << " " << fi.llvm_name << "(" << params << "\n";
        return true;
    };

    // Emit declare for each referenced function.
    // Search order: pending_library_methods_ → pending_library_funcs_ → functions_ (fallback).
    // The fallback is critical when cached_library_state is used: pending maps are empty
    // but functions_ is populated from the restored state.
    std::ostringstream decls;
    std::unordered_set<std::string> already_declared;
    int count = 0;
    for (const auto& name : referenced) {
        // Skip if already declared or defined in the current IR
        if (already_declared.count(name))
            continue;

        bool found = false;

        // 1. Check pending_library_methods_
        auto method_it = pending_library_methods_.find(name);
        if (method_it != pending_library_methods_.end()) {
            const auto& info = method_it->second;
            std::string method_key = info.type_name + "_" + info.method->name;
            auto fn_it = functions_.find(method_key);
            if (fn_it != functions_.end() && emit_declare(fn_it->second, decls)) {
                count++;
                already_declared.insert(name);
                found = true;
            }
        }

        // 2. Check pending_library_funcs_
        if (!found) {
            auto func_it = pending_library_funcs_.find(name);
            if (func_it != pending_library_funcs_.end()) {
                for (const auto& [key, fi] : functions_) {
                    if (fi.llvm_name == name) {
                        if (emit_declare(fi, decls)) {
                            count++;
                            already_declared.insert(name);
                            found = true;
                        }
                        break;
                    }
                }
            }
        }

        // 3. Fallback: search functions_ directly by llvm_name.
        // This handles the case where cached_library_state is used and
        // pending maps are empty but functions_ was restored from state.
        if (!found) {
            for (const auto& [key, fi] : functions_) {
                if (fi.llvm_name == name) {
                    if (emit_declare(fi, decls)) {
                        count++;
                        already_declared.insert(name);
                    }
                    break;
                }
            }
        }
    }

    if (count > 0) {
        output_ << "\n; Lazy library declarations (only functions actually used)\n";
        output_ << decls.str();
        TML_DEBUG_LN("[LAZY_LIB_DECL] Declared "
                     << count << " of "
                     << (pending_library_methods_.size() + pending_library_funcs_.size())
                     << " library functions");
    }

    // Diagnostic: check for unresolved @tml_ references in final IR
    {
        std::string final_ir = output_.str();
        // Collect all defined/declared function names
        std::unordered_set<std::string> defined_or_declared;
        std::istringstream stream(final_ir);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("define ") != std::string::npos ||
                line.find("declare ") != std::string::npos) {
                auto at_pos = line.find("@tml_");
                if (at_pos != std::string::npos) {
                    size_t s = at_pos;
                    size_t p = at_pos + 5;
                    while (p < line.size() && (std::isalnum(line[p]) || line[p] == '_'))
                        ++p;
                    defined_or_declared.insert(line.substr(s, p - s));
                }
            }
        }
        // Now scan ALL @tml_ references
        size_t scan_pos = 0;
        while ((scan_pos = final_ir.find("@tml_", scan_pos)) != std::string::npos) {
            size_t s = scan_pos;
            scan_pos += 5;
            while (scan_pos < final_ir.size() &&
                   (std::isalnum(final_ir[scan_pos]) || final_ir[scan_pos] == '_'))
                ++scan_pos;
            std::string ref = final_ir.substr(s, scan_pos - s);
            if (defined_or_declared.count(ref) == 0) {
                TML_LOG_WARN("codegen", "[LAZY_LIB_DECL] UNRESOLVED: " << ref);
            }
        }
    }
}

} // namespace tml::codegen
