TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Impl Method Declarations
//!
//! This file implements impl method declaration and instantiation code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/type.hpp"

#include <functional>
#include <sstream>

namespace tml::codegen {

// Helper: Parse a mangled type string back into a semantic type
// e.g., "ptr_ChannelNode__I32" -> PtrType{inner=NamedType{name="ChannelNode", type_args=[I32]}}
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Handle primitive types
    if (s == "I8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (s == "I16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (s == "I32")
        return types::make_i32();
    if (s == "I64")
        return types::make_i64();
    if (s == "I128")
        return types::make_primitive(types::PrimitiveKind::I128);
    if (s == "U8")
        return types::make_primitive(types::PrimitiveKind::U8);
    if (s == "U16")
        return types::make_primitive(types::PrimitiveKind::U16);
    if (s == "U32")
        return types::make_primitive(types::PrimitiveKind::U32);
    if (s == "U64")
        return types::make_primitive(types::PrimitiveKind::U64);
    if (s == "U128")
        return types::make_primitive(types::PrimitiveKind::U128);
    if (s == "F32")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();
    if (s == "Unit")
        return types::make_unit();
    if (s == "Usize")
        return types::make_primitive(types::PrimitiveKind::U64);
    if (s == "Isize")
        return types::make_primitive(types::PrimitiveKind::I64);

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable pointer prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for ref prefix
    if (s.size() > 4 && s.substr(0, 4) == "ref_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable ref prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutref_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for nested generic (e.g., Mutex__I32, ChannelNode__I32)
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);

        // Parse all type arguments (separated by __)
        std::vector<types::TypePtr> type_args;
        size_t pos = 0;
        while (pos < arg_str.size()) {
            // Find next __ delimiter
            auto next_delim = arg_str.find("__", pos);
            std::string arg_part;
            if (next_delim == std::string::npos) {
                arg_part = arg_str.substr(pos);
                pos = arg_str.size();
            } else {
                arg_part = arg_str.substr(pos, next_delim - pos);
                pos = next_delim + 2;
            }

            auto arg_type = parse_mangled_type_string(arg_part);
            if (arg_type) {
                type_args.push_back(arg_type);
            } else {
                // Fallback: create NamedType
                auto t = std::make_shared<types::Type>();
                t->kind = types::NamedType{arg_part, "", {}};
                type_args.push_back(t);
            }
        }

        auto t = std::make_shared<types::Type>();
        t->kind = types::NamedType{base, "", std::move(type_args)};
        return t;
    }

    // Simple struct type (no generics, no prefix)
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

// Helper to extract name from FuncParam pattern
// For tuple patterns, returns a synthetic name like __tuple_param_0
static std::string get_param_name(const parser::FuncParam& param, size_t param_index = 0) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    } else if (param.pattern && param.pattern->is<parser::TuplePattern>()) {
        return "__tuple_param_" + std::to_string(param_index);
    }
    return "_anon";
}

void LLVMIRGen::gen_impl_method(const std::string& type_name, const parser::FuncDecl& method) {
    // Skip builtin types that have hard-coded implementations in method.cpp
    // These use lowlevel blocks in TML source but are handled directly by codegen
    if (type_name == "Ordering") {
        return;
    }

    // Skip generic methods for now (they will be instantiated when called)
    if (!method.generics.empty()) {
        return;
    }

    // Record @allocates decorator for Phase 4b Str temp tracking
    for (const auto& decorator : method.decorators) {
        if (decorator.name == "allocates") {
            allocating_functions_.insert(method.name);
            break;
        }
    }

    std::string method_name = type_name + "_" + method.name;

    // Generate mangled LLVM symbol name (includes module path for library types)
    std::string func_llvm_name = mangle_impl_method(type_name, method.name);

    // Skip if already generated (can happen with re-exports across modules)
    std::string llvm_name = "@" + func_llvm_name;
    if (generated_functions_.count(llvm_name)) {
        return;
    }
    // NOTE: Do NOT insert into generated_functions_ here. In lazy_library_defs mode,
    // this function may be deferred to pending_library_methods_ without emitting any IR.
    // If we mark it as "generated" now, gen_impl_method_instantiation() will skip it
    // later when PendingImplMethod tries to emit the actual definition.
    // The insert happens AFTER the lazy check below.
    current_func_ = method_name;
    current_impl_type_ = type_name; // Set impl type for 'this' field access
    locals_.clear();
    block_terminated_ = false;
    temp_drops_.clear();
    last_semantic_type_ = nullptr;
    pending_str_temps_.clear();
    expected_enum_type_.clear();
    expected_literal_type_.clear();

    // Build type substitutions for monomorphized generic types.
    // When type_name is e.g. "Take__Repeat__I32", we need to parse the type args
    // and map them to the struct's generic params so that parameter types like "I"
    // resolve to "%struct.Repeat__I32" instead of falling back to "i32".
    // IMPORTANT: Only derive subs from mangled name if current_type_subs_ is empty.
    // When called from gen_impl_method_instantiation, current_type_subs_ is already
    // correctly set (with proper nested generics), and parse_mangled_type_string
    // cannot handle nested generics like "Take__Repeat__I32" → Take[Repeat[I32]].
    auto saved_type_subs = current_type_subs_;
    auto saved_const_generic_values_impl = current_const_generic_values_;
    auto pos_dunder = type_name.find("__");
    if (pos_dunder != std::string::npos && current_type_subs_.empty()) {
        std::string base_name = type_name.substr(0, pos_dunder);
        auto struct_def = env_.lookup_struct(base_name);
        if (struct_def.has_value() && !struct_def->type_params.empty()) {
            auto parsed = parse_mangled_type_string(type_name);
            if (parsed && parsed->is<types::NamedType>()) {
                const auto& named = parsed->as<types::NamedType>();
                for (size_t i = 0; i < struct_def->type_params.size() && i < named.type_args.size();
                     ++i) {
                    current_type_subs_[struct_def->type_params[i]] = named.type_args[i];
                }
                // Also map const generic params (e.g., N -> ConstGenericType{3})
                for (size_t i = 0; i < struct_def->const_params.size(); ++i) {
                    size_t idx = struct_def->type_params.size() + i;
                    if (idx < named.type_args.size()) {
                        current_type_subs_[struct_def->const_params[i].name] = named.type_args[idx];
                    }
                }
            }
        }
    }
    // Extract const generic values from current type substitutions
    for (const auto& [param_name, type_ptr] : current_type_subs_) {
        if (type_ptr && type_ptr->is<types::ConstGenericType>()) {
            const auto& cgt = type_ptr->as<types::ConstGenericType>();
            if (cgt.resolved_value.has_value()) {
                current_const_generic_values_[param_name] = *cgt.resolved_value;
            }
        }
    }

    // Determine return type
    std::string ret_type = "void";
    if (method.return_type.has_value()) {
        if (!current_type_subs_.empty()) {
            auto resolved_ret =
                resolve_parser_type_with_subs(**method.return_type, current_type_subs_);
            ret_type = llvm_type_from_semantic(resolved_ret, /*for_data=*/false);
        } else {
            ret_type = llvm_type_ptr(*method.return_type);
        }
    }
    // Fix: Unit type returns "{}" (empty struct), not "void".
    // "void" causes call sites to discard the return value, but Unit is a real
    // zero-sized value that must be storable in variables and passed to functions.
    if (ret_type == "void" && method.return_type.has_value()) {
        // Check if the return type is Unit — if so, use "{}" instead of "void".
        const auto& rt = **method.return_type;
        if (rt.is<parser::NamedType>()) {
            const auto& named = rt.as<parser::NamedType>();
            if (!named.path.segments.empty() && named.path.segments.back() == "Unit") {
                ret_type = "{}";
            }
        }
        // Also handle the case where the return type resolves to Unit through
        // the semantic type system (e.g., when T=Unit in a generic impl).
        if (ret_type == "void") {
            std::string data_ret;
            if (!current_type_subs_.empty()) {
                auto resolved_ret =
                    resolve_parser_type_with_subs(**method.return_type, current_type_subs_);
                data_ret = llvm_type_from_semantic(resolved_ret, /*for_data=*/true);
            }
            if (data_ret == "{}") {
                ret_type = "{}";
            }
        }
    }
    current_ret_type_ = ret_type;
    func_ret_type_ = ret_type;

    // Build parameter list
    std::string params;
    std::string param_types;
    std::vector<std::string> param_types_vec;

    // Check if first param is 'this'/'self' or 'mut this'/'mut self' (instance method vs static)
    // Note: 'self' is an alias for 'this' (Rust compatibility)
    size_t param_start = 0;
    bool is_instance_method = false;
    bool is_mut_this = false;
    bool is_ref_this = false;
    if (!method.params.empty()) {
        const auto& first_param = method.params[0];
        std::string first_name = get_param_name(first_param);
        if (first_name == "this" || first_name == "self") {
            is_instance_method = true;
            param_start = 1; // Skip 'this'/'self' in param loop since we handle it specially
            // Check if 'mut this'/'mut self' - need to pass by pointer for mutation
            if (first_param.pattern && first_param.pattern->is<parser::IdentPattern>()) {
                is_mut_this = first_param.pattern->as<parser::IdentPattern>().is_mut;
            }
            // Check if 'this: ref This' - reference type requires pointer passing
            // Only for non-mut 'this' — 'mut this' already uses ptr with is_ptr_to_value
            if (!is_mut_this && first_param.type && first_param.type->is<parser::RefType>()) {
                is_ref_this = true;
            }
        }
    }

    // Add 'this' as first parameter only for instance methods
    // For primitive types with 'mut this', pass by pointer so mutations propagate back
    // For primitive types without 'mut this', pass by value
    // For structs/enums, always pass by pointer
    std::string this_type = "ptr";    // default for structs
    std::string this_inner_type = ""; // For mut this on primitives, the actual primitive type
    if (is_instance_method) {
        // Check if implementing on a primitive type
        std::string llvm_type = llvm_type_name(type_name);
        if (llvm_type[0] != '%') {
            // Primitive type (i32, i64, i1, float, double, etc.)
            if (is_mut_this || is_ref_this) {
                // For 'mut this' or 'this: ref This', pass by pointer
                this_type = "ptr";
                this_inner_type = llvm_type; // Remember the actual type for load/store
            } else {
                // For immutable 'this', pass by value
                this_type = llvm_type;
            }
        }
        // Skip 'this' parameter for Unit type (void is not valid in LLVM parameter lists)
        if (this_type != "void") {
            params = this_type + " %this";
            param_types = this_type;
            param_types_vec.push_back(this_type);
        } else {
            // For Unit, treat as no-arg method
            is_instance_method = false;
        }
    }

    // Add remaining parameters
    // Note: For the first param (when it's NOT this/self and is a struct type),
    // we use ptr instead of the concrete struct type. This matches method-syntax
    // calling convention where the receiver is always passed as ptr.
    // Example: ManuallyDrop::into_inner(slot: ManuallyDrop[T])
    for (size_t i = param_start; i < method.params.size(); ++i) {
        if (!params.empty()) {
            params += ", ";
            param_types += ", ";
        }
        std::string param_type;
        if (!current_type_subs_.empty() && method.params[i].type) {
            auto resolved =
                resolve_parser_type_with_subs(*method.params[i].type, current_type_subs_);
            param_type = llvm_type_from_semantic(resolved, /*for_data=*/true);
            if (resolved && resolved->is<types::FuncType>()) {
                param_type = "{ ptr, ptr }";
            }
        } else {
            param_type = llvm_type_ptr(method.params[i].type);
            if (method.params[i].type && method.params[i].type->is<parser::FuncType>()) {
                param_type = "{ ptr, ptr }";
            }
        }
        // For the first param (i == 0 when !is_instance_method), if it's a struct/enum,
        // use ptr. This is the "receiver-like" param for method-syntax calls.
        if (i == 0 && !is_instance_method &&
            (param_type.find("%struct.") == 0 || param_type.find("%enum.") == 0)) {
            param_type = "ptr";
        }
        std::string param_name = get_param_name(method.params[i], i);
        params += param_type + " %" + param_name;
        param_types += param_type;
        param_types_vec.push_back(param_type);
    }

    // Register function in functions_ map for lookup
    // This is critical for suite mode where method calls look up functions by name
    std::string func_type = ret_type + " (" + param_types + ")";
    functions_[method_name] = FuncInfo{"@" + func_llvm_name, func_type, ret_type, param_types_vec};

    // In lazy_library_defs mode, skip emitting the function entirely and store
    // it for deferred generation. The define/declare will be emitted later only if
    // the function is actually referenced by user code or other library functions.
    // This applies to BOTH library_decls_only and full-definition modes.
    if (options_.lazy_library_defs && !options_.library_ir_only &&
        !current_module_prefix_.empty()) {
        pending_library_methods_["@" + func_llvm_name] = {
            type_name, &method, current_module_prefix_, current_module_name_,
            current_submodule_name_};
        current_func_.clear();
        current_impl_type_.clear();
        return;
    }

    // Now that we've passed the lazy check, mark the function as generated.
    // This prevents duplicate generation from re-exports across modules.
    generated_functions_.insert(llvm_name);

    // In library_decls_only mode (without lazy), emit declare for library methods
    // that exist in the pre-compiled stdlib.obj. Methods NOT in the stdlib (e.g.,
    // generic instantiations with test types) get full definitions.
    if (options_.library_decls_only) {
        bool in_stdlib = options_.cached_library_state &&
                         options_.cached_library_state->generated_functions.count(llvm_name);
        if (in_stdlib) {
            emit_line("");
            emit_line("declare dso_local " + ret_type + " @" + func_llvm_name + "(" + param_types +
                      ")");
            current_func_.clear();
            current_impl_type_.clear();
            return;
        }
        // Fall through to emit full definition for methods not in stdlib.obj
    }

    emit_line("");
    // In library_ir_only mode, use external linkage so the shared library object
    // can export symbols for test objects to link against.
    std::string impl_linkage = options_.library_ir_only ? "" : "internal ";
    emit_line("define " + impl_linkage + ret_type + " @" + func_llvm_name + "(" + params +
              ") #0 {");
    TML_LOG_TRACE("codegen", "[GEN_IMPL] " << func_llvm_name << " type_name=" << type_name);
    emit_line("entry:");

    // Register 'this'/'self' in locals only for instance methods
    // Remember what the parameter was named to register both names if needed
    std::string this_param_name = "";
    if (is_instance_method && !method.params.empty()) {
        this_param_name = get_param_name(method.params[0]);
    }
    if (is_instance_method) {
        // Create semantic type for the impl type
        types::TypePtr impl_semantic_type = std::make_shared<types::Type>();

        // For primitive types, create a PrimitiveType (needed for signedness checks in codegen)
        // Otherwise, use NamedType for structs/enums
        auto create_primitive_type = [](const std::string& name) -> std::optional<types::Type> {
            types::Type t;
            if (name == "I8")
                t.kind = types::PrimitiveType{types::PrimitiveKind::I8};
            else if (name == "I16")
                t.kind = types::PrimitiveType{types::PrimitiveKind::I16};
            else if (name == "I32")
                t.kind = types::PrimitiveType{types::PrimitiveKind::I32};
            else if (name == "I64")
                t.kind = types::PrimitiveType{types::PrimitiveKind::I64};
            else if (name == "I128")
                t.kind = types::PrimitiveType{types::PrimitiveKind::I128};
            else if (name == "U8")
                t.kind = types::PrimitiveType{types::PrimitiveKind::U8};
            else if (name == "U16")
                t.kind = types::PrimitiveType{types::PrimitiveKind::U16};
            else if (name == "U32")
                t.kind = types::PrimitiveType{types::PrimitiveKind::U32};
            else if (name == "U64")
                t.kind = types::PrimitiveType{types::PrimitiveKind::U64};
            else if (name == "U128")
                t.kind = types::PrimitiveType{types::PrimitiveKind::U128};
            else if (name == "F32")
                t.kind = types::PrimitiveType{types::PrimitiveKind::F32};
            else if (name == "F64")
                t.kind = types::PrimitiveType{types::PrimitiveKind::F64};
            else if (name == "Bool")
                t.kind = types::PrimitiveType{types::PrimitiveKind::Bool};
            else if (name == "Str")
                t.kind = types::PrimitiveType{types::PrimitiveKind::Str};
            else if (name == "Char")
                t.kind = types::PrimitiveType{types::PrimitiveKind::Char};
            else
                return std::nullopt;
            return t;
        };

        auto prim = create_primitive_type(current_impl_type_);
        if (prim) {
            impl_semantic_type->kind = std::get<types::PrimitiveType>(prim->kind);
        } else {
            // Build semantic type for this impl type.
            // When current_type_subs_ is available, use the struct definition's type params
            // to build the correct NamedType. This handles nested generics like
            // Take[Repeat[I32]] correctly, where naive __ splitting would produce
            // Take[Repeat, I32].
            auto sep_pos = current_impl_type_.find("__");
            bool built_from_subs = false;
            if (sep_pos != std::string::npos && !current_type_subs_.empty()) {
                std::string base_name = current_impl_type_.substr(0, sep_pos);
                auto struct_def = env_.lookup_struct(base_name);
                if (struct_def.has_value() && !struct_def->type_params.empty()) {
                    std::vector<types::TypePtr> type_args;
                    for (const auto& param : struct_def->type_params) {
                        auto it = current_type_subs_.find(param);
                        if (it != current_type_subs_.end()) {
                            type_args.push_back(it->second);
                        }
                    }
                    if (type_args.size() == struct_def->type_params.size()) {
                        impl_semantic_type->kind =
                            types::NamedType{base_name, "", std::move(type_args)};
                        built_from_subs = true;
                    }
                }
            }
            if (!built_from_subs && sep_pos != std::string::npos) {
                std::string base_name = current_impl_type_.substr(0, sep_pos);
                std::string args_str = current_impl_type_.substr(sep_pos + 2);

                // Parse type args from mangled suffix (naive __ splitting - works for
                // simple cases like Arc__I32, Maybe__I32__Str)
                std::vector<types::TypePtr> type_args;
                size_t pos = 0;
                while (pos < args_str.size()) {
                    auto next_sep = args_str.find("__", pos);
                    std::string arg = (next_sep == std::string::npos)
                                          ? args_str.substr(pos)
                                          : args_str.substr(pos, next_sep - pos);
                    auto arg_type = parse_mangled_type_string(arg);
                    type_args.push_back(arg_type);
                    if (next_sep == std::string::npos)
                        break;
                    pos = next_sep + 2;
                }
                impl_semantic_type->kind = types::NamedType{base_name, "", type_args};
            } else {
                // Parse the mangled type name properly
                auto parsed = parse_mangled_type_string(current_impl_type_);
                if (parsed) {
                    impl_semantic_type = parsed;
                } else {
                    impl_semantic_type->kind = types::NamedType{current_impl_type_, "", {}};
                }
            }
        }

        if (is_ref_this && !this_inner_type.empty()) {
            // For 'this: ref This' on primitive types, %this is a reference (pointer).
            // Register as ptr type WITHOUT is_ptr_to_value — the deref operator (*this)
            // will do the actual load. gen_ident returns the pointer as-is.
            locals_["this"] = VarInfo{"%this", "ptr", impl_semantic_type, std::nullopt, false};
            if (this_param_name == "self") {
                locals_["self"] = VarInfo{"%this", "ptr", impl_semantic_type, std::nullopt, false};
            }
        } else if (!this_inner_type.empty()) {
            // For 'mut this'/'mut self' on primitive types, %this is a pointer to the value
            // Mark is_ptr_to_value so gen_ident will load from %this
            locals_["this"] =
                VarInfo{"%this", this_inner_type, impl_semantic_type, std::nullopt, true};
            // Also register as 'self' if that was the parameter name
            if (this_param_name == "self") {
                locals_["self"] =
                    VarInfo{"%this", this_inner_type, impl_semantic_type, std::nullopt, true};
            }
        } else {
            locals_["this"] = VarInfo{"%this", this_type, impl_semantic_type, std::nullopt};
            // Also register as 'self' if that was the parameter name
            if (this_param_name == "self") {
                locals_["self"] = VarInfo{"%this", this_type, impl_semantic_type, std::nullopt};
            }
        }
    }

    // Register other parameters in locals by creating allocas
    for (size_t i = param_start; i < method.params.size(); ++i) {
        std::string param_type;
        if (!current_type_subs_.empty() && method.params[i].type) {
            auto resolved =
                resolve_parser_type_with_subs(*method.params[i].type, current_type_subs_);
            param_type = llvm_type_from_semantic(resolved, /*for_data=*/true);
            if (resolved && resolved->is<types::FuncType>()) {
                param_type = "{ ptr, ptr }";
            }
        } else {
            param_type = llvm_type_ptr(method.params[i].type);
            if (method.params[i].type && method.params[i].type->is<parser::FuncType>()) {
                param_type = "{ ptr, ptr }";
            }
        }
        // Normalize void -> {} for Unit parameters (void is invalid in LLVM data contexts)
        if (param_type == "void")
            param_type = "{}";
        std::string param_name = get_param_name(method.params[i], i);
        // Resolve semantic type for the parameter
        types::TypePtr semantic_type =
            resolve_parser_type_with_subs(*method.params[i].type, current_type_subs_);

        // First non-this/self struct/enum param is passed as ptr (see signature loop).
        // Copy from ptr into a local alloca so field access (GEP) works correctly.
        if (i == 0 && !is_instance_method &&
            (param_type.find("%struct.") == 0 || param_type.find("%enum.") == 0)) {
            // Load the struct from the ptr, store into alloca
            std::string alloca_reg = fresh_reg();
            std::string loaded_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + param_type);
            emit_line("  " + loaded_reg + " = load " + param_type + ", ptr %" + param_name);
            emit_line("  store " + param_type + " " + loaded_reg + ", ptr " + alloca_reg);
            locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};
        } else {
            std::string alloca_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + param_type);
            emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
            locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};
        }
    }

    // Destructure tuple pattern parameters
    for (size_t i = param_start; i < method.params.size(); ++i) {
        if (method.params[i].pattern && method.params[i].pattern->is<parser::TuplePattern>()) {
            const auto& tuple_pat = method.params[i].pattern->as<parser::TuplePattern>();
            std::string param_name = get_param_name(method.params[i], i);
            std::string param_type = llvm_type_ptr(method.params[i].type);
            types::TypePtr semantic_type =
                resolve_parser_type_with_subs(*method.params[i].type, {});

            auto it = locals_.find(param_name);
            if (it == locals_.end())
                continue;
            std::string tuple_ptr = it->second.reg;

            std::vector<std::string> elem_types;
            std::vector<types::TypePtr> semantic_elem_types;
            if (semantic_type && semantic_type->is<types::TupleType>()) {
                const auto& tup = semantic_type->as<types::TupleType>();
                semantic_elem_types = tup.elements;
                for (const auto& elem : tup.elements) {
                    elem_types.push_back(llvm_type_from_semantic(elem));
                }
            }

            for (size_t j = 0; j < tuple_pat.elements.size() && j < elem_types.size(); ++j) {
                const auto& elem_pattern = *tuple_pat.elements[j];
                if (elem_pattern.is<parser::IdentPattern>()) {
                    const auto& ident = elem_pattern.as<parser::IdentPattern>();
                    std::string elem_type = elem_types[j];
                    types::TypePtr semantic_elem =
                        j < semantic_elem_types.size() ? semantic_elem_types[j] : nullptr;

                    std::string elem_ptr = fresh_reg();
                    emit_line("  " + elem_ptr + " = getelementptr inbounds " + param_type +
                              ", ptr " + tuple_ptr + ", i32 0, i32 " + std::to_string(j));

                    std::string elem_val = fresh_reg();
                    emit_line("  " + elem_val + " = load " + elem_type + ", ptr " + elem_ptr);

                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + elem_type);
                    emit_line("  store " + elem_type + " " + elem_val + ", ptr " + var_alloca);
                    locals_[ident.name] =
                        VarInfo{var_alloca, elem_type, semantic_elem, std::nullopt};
                }
            }
        }
    }

    // Coverage instrumentation - inject call at method entry
    // Uses qualified name like "TypeName::method_name" for library coverage tracking
    emit_coverage(type_name + "::" + method.name);

    // Generate method body
    if (method.body) {
        // Push drop scope for method body (enables RAII for local variables)
        push_drop_scope();

        for (const auto& stmt : method.body->stmts) {
            if (block_terminated_)
                break;
            gen_stmt(*stmt);
        }

        // Handle trailing expression
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*method.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                // Mark the returned variable as consumed (moved) so it won't be dropped.
                // This prevents double-free for types with Drop (like Buffer, List, etc.).
                // Same logic as gen_return() in return.cpp — tail expressions transfer ownership.
                if (method.body->expr.value()->is<parser::IdentExpr>()) {
                    const auto& ident = method.body->expr.value()->as<parser::IdentExpr>();
                    mark_var_consumed(ident.name);
                }

                // If the tail expression is a Str temp, remove from temp drops (ownership
                // transfers)
                if (last_expr_type_ == "ptr" && !temp_drops_.empty() &&
                    temp_drops_.back().is_heap_str) {
                    temp_drops_.pop_back();
                }
                if (last_expr_type_ == "ptr" && !pending_str_temps_.empty()) {
                    consume_last_str_temp();
                }

                // Flush remaining Str intermediates before returning
                flush_str_temps();

                // Emit drops before returning
                emit_all_drops();

                // Fix: Unit type always uses zeroinitializer (can't use bool/int values)
                if (ret_type == "{}") {
                    emit_line("  ret {} zeroinitializer");
                } else if (ret_type == "ptr" && result == "0") {
                    // Fix: if returning ptr type with "0" placeholder (from loops), use null
                    emit_line("  ret ptr null");
                } else if (result == "0" && ret_type.find("%struct.") == 0) {
                    // Fix: if returning struct type with "0" placeholder, use zeroinitializer
                    emit_line("  ret " + ret_type + " zeroinitializer");
                } else {
                    // Handle type mismatches between result and return type
                    std::string final_result = result;
                    std::string actual_type = last_expr_type_;
                    if (actual_type != ret_type) {
                        if (actual_type == "ptr" &&
                            (ret_type.starts_with("%struct.") || ret_type.starts_with("%class."))) {
                            // Returning ptr from a method that returns a struct/class by value.
                            // Load the value from the pointer.
                            std::string load_reg = fresh_reg();
                            emit_line("  " + load_reg + " = load " + ret_type + ", ptr " + result);
                            final_result = load_reg;
                        } else if (ret_type == "i64" &&
                                   (actual_type == "i32" || actual_type == "i16" ||
                                    actual_type == "i8")) {
                            std::string ext_reg = fresh_reg();
                            emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                      " to i64");
                            final_result = ext_reg;
                        } else if (ret_type == "i32" &&
                                   (actual_type == "i16" || actual_type == "i8")) {
                            std::string ext_reg = fresh_reg();
                            emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                      " to i32");
                            final_result = ext_reg;
                        }
                    }
                    emit_line("  ret " + ret_type + " " + final_result);
                }
                block_terminated_ = true;
            }
        }

        // Emit drops for variables that weren't returned via tail expression
        if (!block_terminated_) {
            emit_all_drops();
        }
        pop_drop_scope();
    }

    // Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else if (ret_type == "i1") {
            emit_line("  ret i1 false");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");
    current_func_.clear();
    current_ret_type_.clear();
    current_impl_type_.clear();
    current_type_subs_ = saved_type_subs;
    current_const_generic_values_ = saved_const_generic_values_impl;
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

// Generate a specialized version of a generic impl method
// e.g., impl[T] Container[T] { func get() -> T } instantiated for Container[I32]
void LLVMIRGen::gen_impl_method_instantiation(
    const std::string& mangled_type_name, const parser::FuncDecl& method,
    const std::unordered_map<std::string, types::TypePtr>& type_subs,
    const std::vector<parser::GenericParam>& impl_generics, const std::string& method_type_suffix,
    bool is_library_type, const std::string& base_type_name, const parser::Type* impl_self_type) {
    TML_LOG_TRACE("codegen", "[IMPL_INST_DBG] ENTER gen_impl_method_instantiation: "
                                 << base_type_name << "::" << method.name
                                 << " (mangled=" << mangled_type_name << ")");
    // Skip method-level generic methods without a concrete type suffix.
    // Without a suffix, type params (e.g. T) remain unresolved and produce
    // `alloca %struct.T` in the IR — an unsized type that LLVM rejects.
    if (!method.generics.empty() && method_type_suffix.empty()) {
        return;
    }

    // Build full method name and check if already generated
    std::string method_name_for_key = method.name;
    if (!method_type_suffix.empty()) {
        method_name_for_key += "__" + method_type_suffix;
    }
    std::string generated_key = mangle_impl_method(mangled_type_name, method_name_for_key);
    std::string llvm_name = "@" + generated_key;

    // Prevent duplicate function generation - this can happen when the same method
    // is requested from multiple code paths or when processing nested method calls
    // Check both tracking sets since gen_impl_method and gen_impl_method_instantiation
    // can generate the same function
    if (generated_impl_methods_output_.count(generated_key) > 0 ||
        generated_functions_.count(llvm_name) > 0) {
        return;
    }
    generated_impl_methods_output_.insert(generated_key);
    generated_functions_.insert(llvm_name);

    // Save current context
    std::string saved_func = current_func_;
    std::string saved_ret_type = current_ret_type_;
    std::string saved_func_ret = func_ret_type_;
    std::string saved_impl_type = current_impl_type_;
    bool saved_terminated = block_terminated_;
    auto saved_locals = locals_;
    auto saved_type_subs = current_type_subs_;
    auto saved_where_constraints = current_where_constraints_;
    auto saved_temp_drops = temp_drops_;

    // Extract where constraints from impl-level generic bounds (e.g., T: PartialOrd)
    current_where_constraints_.clear();
    for (const auto& generic : impl_generics) {
        if (!generic.bounds.empty()) {
            types::WhereConstraint constraint;
            constraint.type_param = generic.name;

            for (const auto& bound : generic.bounds) {
                if (bound->is<parser::NamedType>()) {
                    const auto& named = bound->as<parser::NamedType>();
                    std::string behavior_name;
                    if (!named.path.segments.empty()) {
                        behavior_name = named.path.segments.back();
                    }

                    if (!named.generics.has_value() || named.generics->args.empty()) {
                        // Simple bound like T: PartialOrd
                        constraint.required_behaviors.push_back(behavior_name);
                    } else {
                        // Parameterized bound like C: Container[T]
                        types::BoundConstraint bc;
                        bc.behavior_name = behavior_name;
                        for (const auto& arg : named.generics->args) {
                            if (arg.is_type()) {
                                bc.type_args.push_back(
                                    resolve_parser_type_with_subs(*arg.as_type(), type_subs));
                            }
                        }
                        constraint.parameterized_bounds.push_back(bc);
                    }
                }
            }

            current_where_constraints_.push_back(constraint);
        }
    }

    // Build method name, including method-level type suffix if present
    std::string full_method_name = method.name;
    if (!method_type_suffix.empty()) {
        full_method_name += "__" + method_type_suffix;
    }
    std::string method_name = mangled_type_name + "_" + full_method_name;
    current_func_ = method_name;
    current_impl_type_ = mangled_type_name;
    locals_.clear();
    block_terminated_ = false;
    last_semantic_type_ = nullptr;
    temp_drops_.clear();
    pending_str_temps_.clear();
    expected_enum_type_.clear();
    expected_literal_type_.clear();

    // Build full type_subs including method-level type parameters
    auto full_type_subs = type_subs;

    // Remap type_subs keys to match impl_generics names.
    // The caller may have built type_subs with different param names (e.g., "I" from a
    // blanket impl) but the method body uses the impl block's own param names (e.g., "Fut").
    if (!impl_generics.empty() && !full_type_subs.empty()) {
        std::vector<types::TypePtr> concrete_types;
        for (const auto& [k, v] : full_type_subs) {
            if (k != "Self" && k != "This" && v) {
                concrete_types.push_back(v);
            }
        }
        for (size_t i = 0; i < impl_generics.size(); ++i) {
            const auto& gname = impl_generics[i].name;
            if (full_type_subs.find(gname) == full_type_subs.end() && i < concrete_types.size()) {
                full_type_subs[gname] = concrete_types[i];
            }
        }
    }

    // Add method-level type parameters from method_type_suffix
    // method_type_suffix contains mangled types like "Str" or "I32__Str" for multi-param methods
    // IMPORTANT: For a single type parameter, the entire suffix is the mangled type.
    // Do NOT split on "__" as it's also used within mangled type names
    // (e.g., "ptr_ChannelNode__I32" is a single type: Ptr[ChannelNode[I32]])
    if (!method_type_suffix.empty() && !method.generics.empty()) {
        if (method.generics.size() == 1) {
            // Single type parameter - use entire suffix as the type
            types::TypePtr param_type = parse_mangled_type_string(method_type_suffix);
            if (param_type) {
                full_type_subs[method.generics[0].name] = param_type;
            }
        } else {
            // Multiple type parameters - need to split, but be careful about nested types
            // For now, use simple splitting (works when params are primitives or simple types)
            // TODO: Implement smarter parsing for complex nested types
            std::vector<std::string> suffix_parts;
            size_t pos = 0;
            std::string suffix = method_type_suffix;
            while (pos < suffix.size()) {
                size_t next = suffix.find("__", pos);
                if (next == std::string::npos) {
                    suffix_parts.push_back(suffix.substr(pos));
                    break;
                }
                suffix_parts.push_back(suffix.substr(pos, next - pos));
                pos = next + 2;
            }

            // Map suffix parts to method type params
            for (size_t i = 0; i < method.generics.size() && i < suffix_parts.size(); ++i) {
                types::TypePtr param_type = parse_mangled_type_string(suffix_parts[i]);
                if (param_type) {
                    full_type_subs[method.generics[i].name] = param_type;
                }
            }
        }
    }

    current_type_subs_ = full_type_subs; // Set type substitutions for the method body

    // Extract const generic values from type substitutions
    // e.g., N -> ConstGenericType{resolved_value=3} means current_const_generic_values_["N"] = 3
    auto saved_const_generic_values = current_const_generic_values_;
    for (const auto& [param_name, type_ptr] : full_type_subs) {
        if (type_ptr && type_ptr->is<types::ConstGenericType>()) {
            const auto& cgt = type_ptr->as<types::ConstGenericType>();
            if (cgt.resolved_value.has_value()) {
                current_const_generic_values_[param_name] = *cgt.resolved_value;
            }
        }
    }

    locals_.clear();
    block_terminated_ = false;
    last_semantic_type_ = nullptr;

    // Determine return type with substitution
    std::string ret_type = "void";
    if (method.return_type.has_value()) {
        auto resolved_ret = resolve_parser_type_with_subs(**method.return_type, full_type_subs);
        // Use for_data=true: return types in data context — Unit should be "{}" not "void"
        ret_type = llvm_type_from_semantic(resolved_ret, /*for_data=*/true);
    }
    current_ret_type_ = ret_type;
    func_ret_type_ = ret_type;

    // Build parameter list
    std::string params;
    std::string param_types;

    // Check if first param is 'this'
    size_t param_start = 0;
    bool is_instance_method = false;
    if (!method.params.empty()) {
        const auto& first_param = method.params[0];
        std::string first_name = get_param_name(first_param);
        if (first_name == "this") {
            is_instance_method = true;
            param_start = 1;
        }
    }

    // Add 'this' as first parameter for instance methods
    // For primitive types, pass by value; for structs/enums, pass by pointer
    std::string this_type = "ptr"; // default for structs
    if (is_instance_method) {
        // Check if implementing on a primitive type - pass by value if so
        std::string llvm_type = llvm_type_name(mangled_type_name);
        if (llvm_type[0] != '%') {
            // Primitive type (i32, i64, i1, float, double, etc.) - pass by value
            this_type = llvm_type;
        }
        // Skip 'this' parameter for Unit type (void/{} are not useful in LLVM parameter lists)
        if (this_type == "void" || this_type == "{}") {
            is_instance_method = false;
        } else {
            params = this_type + " %this";
            param_types = this_type;
        }
    }

    // Add remaining parameters with type substitution
    // IMPORTANT: Use full_type_subs here to properly substitute all type parameters
    // including impl-level (T from impl[T] Range[T]) and method-level generics
    for (size_t i = param_start; i < method.params.size(); ++i) {
        if (!params.empty()) {
            params += ", ";
            param_types += ", ";
        }
        auto resolved_param = resolve_parser_type_with_subs(*method.params[i].type, full_type_subs);
        // Use for_data=true: param types are data context — Unit should be "{}" not "void"
        std::string param_type = llvm_type_from_semantic(resolved_param, /*for_data=*/true);
        // Function-typed parameters use fat pointer { ptr, ptr } to support closures
        if (resolved_param && resolved_param->is<types::FuncType>()) {
            param_type = "{ ptr, ptr }";
        }
        // For non-instance methods called via method syntax (e.g., ManuallyDrop::into_inner(slot)),
        // the first param is passed as ptr from call sites. However, method-level generic
        // instantiations (method_type_suffix non-empty, e.g., sum__AccCounter called as
        // I32::sum(iter)) are called with the value directly, not via ptr.
        if (i == param_start && !is_instance_method && method_type_suffix.empty() &&
            (param_type.find("%struct.") == 0 || param_type.find("%enum.") == 0)) {
            param_type = "ptr";
        }
        std::string param_name = get_param_name(method.params[i], i);
        params += param_type + " %" + param_name;
        param_types += param_type;
    }

    // Function signature - use mangled name for both library and local types
    std::string func_llvm_name = mangle_impl_method(mangled_type_name, full_method_name);

    // Register the function in functions_ so call sites can find it
    // This is crucial for suite mode where multiple test files may call this method
    std::string func_type = ret_type + " (" + param_types + ")";
    std::vector<std::string> param_types_vec;
    if (is_instance_method) {
        param_types_vec.push_back(this_type);
    }
    for (size_t i = param_start; i < method.params.size(); ++i) {
        auto resolved_param = resolve_parser_type_with_subs(*method.params[i].type, full_type_subs);
        std::string pt = llvm_type_from_semantic(resolved_param, /*for_data=*/true);
        if (resolved_param && resolved_param->is<types::FuncType>()) {
            pt = "{ ptr, ptr }";
        }
        // Match signature: non-instance first struct/enum param → ptr (only for non-generic
        // methods)
        if (i == param_start && !is_instance_method && method_type_suffix.empty() &&
            (pt.find("%struct.") == 0 || pt.find("%enum.") == 0)) {
            pt = "ptr";
        }
        param_types_vec.push_back(pt);
    }
    functions_[method_name] = FuncInfo{"@" + func_llvm_name, func_type, ret_type, param_types_vec};

    emit_line("");
    // Use internal linkage for all methods to avoid duplicate symbol warnings
    // Each object file gets its own copy of library methods - slight code bloat
    // but avoids complex COMDAT merging issues with LLD on Windows.
    // In library_ir_only mode, use external linkage for the shared library object.
    std::string inst_linkage = options_.library_ir_only ? "" : "internal ";
    emit_line("define " + inst_linkage + ret_type + " @" + func_llvm_name + "(" + params +
              ") #0 {");
    emit_line("entry:");

    // Register 'this' in locals with proper semantic type for method resolution
    if (is_instance_method) {
        // Create semantic type for 'this' with correct module_path
        // This is crucial for nested method calls in library code (e.g., add() calling offset())
        std::string module_path = "";
        // Use base_type_name if provided, otherwise fall back to mangled_type_name
        std::string type_name_for_lookup =
            base_type_name.empty() ? mangled_type_name : base_type_name;
        if (env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                // Check both base type name and mangled type name
                if (mod.structs.find(type_name_for_lookup) != mod.structs.end() ||
                    mod.enums.find(type_name_for_lookup) != mod.enums.end() ||
                    mod.structs.find(mangled_type_name) != mod.structs.end() ||
                    mod.enums.find(mangled_type_name) != mod.enums.end()) {
                    module_path = mod_name;
                    break;
                }
            }
        }

        auto this_semantic_type = std::make_shared<types::Type>();

        // For specialized impls (e.g., impl[T,E] Outcome[Outcome[T,E], E]), the
        // impl_self_type carries the full structural pattern. Resolve it with type_subs
        // to get the correct self type (e.g., Outcome[Outcome[I32,Str], Str]).
        // Without this, building from impl_generics flat would yield Outcome[I32,Str].
        if (impl_self_type != nullptr) {
            auto resolved = resolve_parser_type_with_subs(*impl_self_type, type_subs);
            if (resolved) {
                this_semantic_type = resolved;
            }
        } else if (!base_type_name.empty()) {
            // Use base_type_name with type_args so method lookup finds "RawPtr::offset" not
            // "RawPtr__I64::offset"
            // Build type_args from type_subs based on impl_generics order
            std::vector<types::TypePtr> type_args;
            for (const auto& gp : impl_generics) {
                auto it = type_subs.find(gp.name);
                if (it != type_subs.end()) {
                    type_args.push_back(it->second);
                }
            }
            this_semantic_type->kind = types::NamedType{base_type_name, module_path, type_args};
        } else {
            // Fallback: parse the mangled name properly
            auto parsed = parse_mangled_type_string(mangled_type_name);
            if (parsed) {
                this_semantic_type = parsed;
            } else {
                this_semantic_type->kind = types::NamedType{mangled_type_name, module_path, {}};
            }
        }

        locals_["this"] = VarInfo{"%this", this_type, this_semantic_type, std::nullopt};
    }

    // Register other parameters in locals by creating allocas
    // Use full_type_subs to properly substitute type parameters
    for (size_t i = param_start; i < method.params.size(); ++i) {
        std::string param_name = get_param_name(method.params[i], i);
        auto resolved_param = resolve_parser_type_with_subs(*method.params[i].type, full_type_subs);
        std::string param_type = llvm_type_from_semantic(resolved_param);
        // Normalize void -> {} for Unit parameters (void is invalid in LLVM data contexts)
        if (param_type == "void")
            param_type = "{}";
        // Function-typed parameters use fat pointer { ptr, ptr } to support closures
        if (resolved_param && resolved_param->is<types::FuncType>()) {
            param_type = "{ ptr, ptr }";
        }
        // For non-instance methods called via method syntax, first struct/enum param arrives
        // as ptr (from call site). Load the struct value from the ptr and store into a local
        // alloca. Method-level generic instantiations (method_type_suffix non-empty) receive the
        // value directly (not via ptr), so use the normal by-value path.
        if (i == param_start && !is_instance_method && method_type_suffix.empty() &&
            (param_type.find("%struct.") == 0 || param_type.find("%enum.") == 0)) {
            std::string alloca_reg = fresh_reg();
            std::string loaded_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + param_type);
            emit_line("  " + loaded_reg + " = load " + param_type + ", ptr %" + param_name);
            emit_line("  store " + param_type + " " + loaded_reg + ", ptr " + alloca_reg);
            locals_[param_name] = VarInfo{alloca_reg, param_type, resolved_param, std::nullopt};
        } else {
            std::string alloca_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + param_type);
            emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
            locals_[param_name] = VarInfo{alloca_reg, param_type, resolved_param, std::nullopt};
        }
    }

    // Destructure tuple pattern parameters (generic method instantiation)
    for (size_t i = param_start; i < method.params.size(); ++i) {
        if (method.params[i].pattern && method.params[i].pattern->is<parser::TuplePattern>()) {
            const auto& tuple_pat = method.params[i].pattern->as<parser::TuplePattern>();
            std::string param_name = get_param_name(method.params[i], i);
            auto resolved_param =
                resolve_parser_type_with_subs(*method.params[i].type, full_type_subs);
            std::string param_type = llvm_type_from_semantic(resolved_param);

            auto it = locals_.find(param_name);
            if (it == locals_.end())
                continue;
            std::string tuple_ptr = it->second.reg;

            std::vector<std::string> elem_types;
            std::vector<types::TypePtr> semantic_elem_types;
            if (resolved_param && resolved_param->is<types::TupleType>()) {
                const auto& tup = resolved_param->as<types::TupleType>();
                semantic_elem_types = tup.elements;
                for (const auto& elem : tup.elements) {
                    elem_types.push_back(llvm_type_from_semantic(elem));
                }
            }

            for (size_t j = 0; j < tuple_pat.elements.size() && j < elem_types.size(); ++j) {
                const auto& elem_pattern = *tuple_pat.elements[j];
                if (elem_pattern.is<parser::IdentPattern>()) {
                    const auto& ident = elem_pattern.as<parser::IdentPattern>();
                    std::string elem_type = elem_types[j];
                    types::TypePtr semantic_elem =
                        j < semantic_elem_types.size() ? semantic_elem_types[j] : nullptr;

                    std::string elem_ptr = fresh_reg();
                    emit_line("  " + elem_ptr + " = getelementptr inbounds " + param_type +
                              ", ptr " + tuple_ptr + ", i32 0, i32 " + std::to_string(j));

                    std::string elem_val = fresh_reg();
                    emit_line("  " + elem_val + " = load " + elem_type + ", ptr " + elem_ptr);

                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + elem_type);
                    emit_line("  store " + elem_type + " " + elem_val + ", ptr " + var_alloca);
                    locals_[ident.name] =
                        VarInfo{var_alloca, elem_type, semantic_elem, std::nullopt};
                }
            }
        }
    }

    // Coverage instrumentation - inject call at method entry
    // Uses base type name for better readability (e.g., "Arc::new" instead of "Arc__I32::new")
    {
        std::string type_for_coverage = base_type_name.empty() ? mangled_type_name : base_type_name;
        emit_coverage(type_for_coverage + "::" + method.name);
    }

    // Generate method body
    TML_LOG_TRACE("codegen", "[IMPL_INST_DBG] BODY_START "
                                 << mangled_type_name << "::" << method.name
                                 << " body=" << (method.body.has_value() ? "yes" : "no"));
    if (method.body.has_value()) {
        // Push drop scope for method body (enables RAII for local variables)
        push_drop_scope();

        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }
        if (method.body->expr.has_value()) {
            std::string result = gen_expr(*method.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                // Mark the returned variable as consumed (moved) so it won't be dropped.
                // This prevents double-free for types with Drop (like Buffer, List, etc.).
                if (method.body->expr.value()->is<parser::IdentExpr>()) {
                    const auto& ident = method.body->expr.value()->as<parser::IdentExpr>();
                    mark_var_consumed(ident.name);
                }

                // If the tail expression is a Str temp, remove from temp drops
                if (last_expr_type_ == "ptr" && !temp_drops_.empty() &&
                    temp_drops_.back().is_heap_str) {
                    temp_drops_.pop_back();
                }
                if (last_expr_type_ == "ptr" && !pending_str_temps_.empty()) {
                    consume_last_str_temp();
                }

                // Flush remaining Str intermediates before returning
                flush_str_temps();

                // Emit drops before returning
                emit_all_drops();
                // Fix: Unit type always uses zeroinitializer (can't use bool/int values)
                if (ret_type == "{}") {
                    emit_line("  ret {} zeroinitializer");
                } else if (ret_type == "ptr" && result == "0") {
                    // Fix: if returning ptr type with "0" placeholder (from loops), use null
                    emit_line("  ret ptr null");
                } else if (result == "0" && ret_type.find("%struct.") == 0) {
                    // Fix: if returning struct type with "0" placeholder, use zeroinitializer
                    emit_line("  ret " + ret_type + " zeroinitializer");
                } else {
                    // Handle type mismatches between result and return type
                    std::string final_result = result;
                    std::string actual_type = last_expr_type_;
                    if (actual_type != ret_type) {
                        if (actual_type == "ptr" &&
                            (ret_type.starts_with("%struct.") || ret_type.starts_with("%class."))) {
                            // Returning ptr from a method that returns a struct/class by value.
                            // Load the value from the pointer.
                            std::string load_reg = fresh_reg();
                            emit_line("  " + load_reg + " = load " + ret_type + ", ptr " + result);
                            final_result = load_reg;
                        } else if (ret_type == "i64" &&
                                   (actual_type == "i32" || actual_type == "i16" ||
                                    actual_type == "i8")) {
                            std::string ext_reg = fresh_reg();
                            emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                      " to i64");
                            final_result = ext_reg;
                        } else if (ret_type == "i32" &&
                                   (actual_type == "i16" || actual_type == "i8")) {
                            std::string ext_reg = fresh_reg();
                            emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                      " to i32");
                            final_result = ext_reg;
                        }
                    }
                    emit_line("  ret " + ret_type + " " + final_result);
                }
                block_terminated_ = true;
            }
        }

        // Emit drops for variables that weren't returned via tail expression
        if (!block_terminated_) {
            emit_all_drops();
        }
        pop_drop_scope();
    }

    // Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else if (ret_type == "i1") {
            emit_line("  ret i1 false");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");

    // NOTE: GlobalLibraryIRCache storage is DISABLED.
    // Each suite needs its own complete implementation.
    // See generic.cpp for explanation.
    (void)is_library_type;
    (void)generated_key;

    // Restore context
    current_func_ = saved_func;
    current_ret_type_ = saved_ret_type;
    func_ret_type_ = saved_func_ret;
    current_impl_type_ = saved_impl_type;
    current_type_subs_ = saved_type_subs;
    current_const_generic_values_ = saved_const_generic_values;
    current_where_constraints_ = saved_where_constraints;
    block_terminated_ = saved_terminated;
    locals_ = saved_locals;
    temp_drops_ = saved_temp_drops;
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

} // namespace tml::codegen
