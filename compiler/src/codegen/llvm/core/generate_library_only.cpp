TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Library-Only IR Path
//!
//! Implements `generate_library_only_ir()`, which is called from `generate()`
//! when `options_.library_ir_only` is true. This path flushes all pending
//! lazy library methods/functions, runs generic instantiations, and returns
//! the complete library IR without any user code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "version_generated.hpp"

namespace tml::codegen {

// Helper: Parse a mangled type string back into a semantic type.
// Handles nested generics correctly by treating the entire suffix after
// the first "__" as a single (possibly nested) type argument.
// e.g., "Shared__PromiseState__I32" -> Shared[PromiseState[I32]]
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
    if (s == "U128") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U128};
        return t;
    }
    if (s == "I128") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I128};
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
    if (s == "Unit")
        return types::make_unit();

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

    // Nested generic: treat the entire suffix after the first "__" as a single
    // (possibly nested) type argument.  This is the KEY difference from the naive
    // splitting approach that breaks nested generics.
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

/// Library-only IR path: flush all pending lazy library methods/functions,
/// emit instantiations, and return the complete library IR.
/// Called from generate() when options_.library_ir_only is true.
auto LLVMIRGen::generate_library_only_ir(const parser::Module& module)
    -> Result<std::string, std::vector<LLVMGenError>> {
    (void)module;

    // Save the output position before generating instantiations.
    // We need to capture the instantiation code for cached_imported_func_code_
    // so that workers using library_decls_only=false get the complete library IR.
    std::string pre_instantiation_output = output_.str();

    // Flush ALL pending lazy library methods/functions so their `define` blocks
    // appear in the library IR. Without this, capture_library_state() cannot
    // extract `declare` stubs for worker threads (library_decls_only mode).
    // In library_ir_only mode there is no user code to scan for references,
    // so we emit everything unconditionally.
    if (options_.lazy_library_defs) {
        auto saved_module_prefix = current_module_prefix_;
        auto saved_module_name = current_module_name_;
        auto saved_submodule = current_submodule_name_;

        for (auto& [fn, info] : pending_library_methods_) {
            if (generated_functions_.count(fn))
                continue;
            current_module_prefix_ = info.module_prefix;
            current_module_name_ = info.module_name;
            current_submodule_name_ = info.submodule_name;
            options_.lazy_library_defs = false;
            generated_functions_.erase(fn);

            // For generic type methods (e.g., Shared__PromiseState__I32::get),
            // set up type substitutions so the body can resolve type params.
            auto saved_type_subs = current_type_subs_;
            auto saved_const_generic_values = current_const_generic_values_;
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
                        for (size_t gi = 0; gi < impl_block.generics.size() && gi < parts.size();
                             ++gi) {
                            auto type_arg = parse_mangled_type_string(parts[gi]);
                            if (type_arg) {
                                current_type_subs_[impl_block.generics[gi].name] = type_arg;
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

            gen_impl_method(info.type_name, *info.method);
            current_type_subs_ = saved_type_subs;
            current_const_generic_values_ = saved_const_generic_values;
            options_.lazy_library_defs = true;
        }

        for (auto& [fn, finfo] : pending_library_funcs_) {
            if (generated_functions_.count(fn))
                continue;
            current_module_prefix_ = finfo.module_prefix;
            current_module_name_ = finfo.module_name;
            current_submodule_name_ = finfo.submodule_name;
            options_.lazy_library_defs = false;
            generated_functions_.erase(fn);
            gen_func_decl(*finfo.func);
            options_.lazy_library_defs = true;
        }

        current_module_prefix_ = saved_module_prefix;
        current_module_name_ = saved_module_name;
        current_submodule_name_ = saved_submodule;
    }

    // Generate pending generic instantiations triggered by library functions.
    // Set in_library_body_ to disable Phase 4b Str temp tracking — library
    // generic instantiations (List[Str], HashMap[Str,X], etc.) manage their own
    // allocations and must not have temps auto-freed.
    {
        auto saved_lib = in_library_body_;
        in_library_body_ = true;
        generate_pending_instantiations();
        in_library_body_ = saved_lib;
    }

    // Update cached_imported_func_code_ to include instantiation-generated code.
    // Without this, workers using library_decls_only=false would miss instantiations
    // that were only generated by generate_pending_instantiations().
    {
        std::string post_output = output_.str();
        // The new code is everything after the pre-instantiation position
        if (post_output.size() > pre_instantiation_output.size()) {
            cached_imported_func_code_ += post_output.substr(pre_instantiation_output.size());
        }
        // Also update type defs (instantiations may generate new struct types)
        std::string new_type_defs = type_defs_buffer_.str();
        if (!new_type_defs.empty()) {
            cached_imported_type_defs_ += new_type_defs;
        }
    }

    // Emit string constants collected during library codegen
    emit_string_constants();

    // Emit attributes section (needed for function definitions)
    // Include target-features so LLVM can select AVX2/FMA/SSE4.2 intrinsics
    emit_line("");
    emit_line("attributes #0 = { nounwind "
              "\"target-features\"=\"+sse2,+sse4.2,+avx,+avx2,+fma\" }");

    // Emit loop metadata (generic instantiations may contain loops)
    emit_loop_metadata();

    // Emit module identification metadata
    {
        int ident_id = fresh_debug_id();
        emit_line("");
        emit_line("!llvm.ident = !{!" + std::to_string(ident_id) + "}");
        emit_line("!" + std::to_string(ident_id) + " = !{!\"tml version " +
                  std::string(tml::VERSION) + "\"}");
    }

    // Final sweep: scan the complete library IR for runtime function references
    scan_for_runtime_refs(output_.str());

    // Finalize runtime declarations and splice into output
    finalize_runtime_decls();
    std::string result = output_.str();
    {
        const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
        auto pos = result.find(placeholder);
        if (pos != std::string::npos) {
            result.replace(pos, placeholder.size(), deferred_runtime_decls_);
        }
    }

    // Update cached_preamble_headers_ with spliced declarations
    // so capture_library_state() gets the finalized preamble
    {
        const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
        auto pos = cached_preamble_headers_.find(placeholder);
        if (pos != std::string::npos) {
            cached_preamble_headers_.replace(pos, placeholder.size(), deferred_runtime_decls_);
        }
    }

    if (!errors_.empty()) {
        return errors_;
    }
    return result;
}

} // namespace tml::codegen
