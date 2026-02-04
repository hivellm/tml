//! # LLVM IR Generator - Impl Method Declarations
//!
//! This file implements impl method declaration and instantiation code generation.

#include "codegen/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/type.hpp"

#include <functional>
#include <iostream>
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
static std::string get_param_name(const parser::FuncParam& param) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    }
    return "_anon";
}

void LLVMIRGen::gen_impl_method(const std::string& type_name, const parser::FuncDecl& method) {
    // Skip builtin types that have hard-coded implementations in method.cpp
    // These use lowlevel blocks in TML source but are handled directly by codegen
    if (type_name == "File" || type_name == "Path" || type_name == "List" ||
        type_name == "HashMap" || type_name == "Buffer" || type_name == "Ordering") {
        return;
    }

    // Skip generic methods for now (they will be instantiated when called)
    if (!method.generics.empty()) {
        return;
    }

    std::string method_name = type_name + "_" + method.name;

    // Skip if already generated (can happen with re-exports across modules)
    std::string llvm_name = "@tml_" + method_name;
    if (generated_functions_.count(llvm_name)) {
        return;
    }
    generated_functions_.insert(llvm_name);
    current_func_ = method_name;
    current_impl_type_ = type_name; // Set impl type for 'this' field access
    locals_.clear();
    block_terminated_ = false;

    // Determine return type
    std::string ret_type = "void";
    if (method.return_type.has_value()) {
        ret_type = llvm_type_ptr(*method.return_type);
    }
    current_ret_type_ = ret_type;

    // Build parameter list
    std::string params;
    std::string param_types;
    std::vector<std::string> param_types_vec;

    // Check if first param is 'this'/'self' or 'mut this'/'mut self' (instance method vs static)
    // Note: 'self' is an alias for 'this' (Rust compatibility)
    size_t param_start = 0;
    bool is_instance_method = false;
    bool is_mut_this = false;
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
            if (is_mut_this) {
                // For 'mut this', pass by pointer so changes propagate back to caller
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
    for (size_t i = param_start; i < method.params.size(); ++i) {
        if (!params.empty()) {
            params += ", ";
            param_types += ", ";
        }
        std::string param_type = llvm_type_ptr(method.params[i].type);
        std::string param_name = get_param_name(method.params[i]);
        params += param_type + " %" + param_name;
        param_types += param_type;
        param_types_vec.push_back(param_type);
    }

    // Function signature
    std::string func_llvm_name = "tml_" + type_name + "_" + method.name;

    // Register function in functions_ map for lookup
    // This is critical for suite mode where method calls look up functions by name
    std::string func_type = ret_type + " (" + param_types + ")";
    functions_[method_name] = FuncInfo{"@" + func_llvm_name, func_type, ret_type, param_types_vec};

    emit_line("");
    emit_line("define internal " + ret_type + " @" + func_llvm_name + "(" + params + ") #0 {");
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
            // Parse mangled name like "Arc__I32" into NamedType{name="Arc", type_args=[I32]}
            auto sep_pos = current_impl_type_.find("__");
            if (sep_pos != std::string::npos) {
                std::string base_name = current_impl_type_.substr(0, sep_pos);
                std::string args_str = current_impl_type_.substr(sep_pos + 2);

                // Parse type args from mangled suffix
                std::vector<types::TypePtr> type_args;
                size_t pos = 0;
                while (pos < args_str.size()) {
                    auto next_sep = args_str.find("__", pos);
                    std::string arg = (next_sep == std::string::npos)
                                          ? args_str.substr(pos)
                                          : args_str.substr(pos, next_sep - pos);
                    types::TypePtr arg_type;
                    if (arg == "I32")
                        arg_type = types::make_i32();
                    else if (arg == "I64")
                        arg_type = types::make_i64();
                    else if (arg == "I8")
                        arg_type = types::make_primitive(types::PrimitiveKind::I8);
                    else if (arg == "I16")
                        arg_type = types::make_primitive(types::PrimitiveKind::I16);
                    else if (arg == "U8")
                        arg_type = types::make_primitive(types::PrimitiveKind::U8);
                    else if (arg == "U16")
                        arg_type = types::make_primitive(types::PrimitiveKind::U16);
                    else if (arg == "U32")
                        arg_type = types::make_primitive(types::PrimitiveKind::U32);
                    else if (arg == "U64")
                        arg_type = types::make_primitive(types::PrimitiveKind::U64);
                    else if (arg == "Usize")
                        arg_type = types::make_primitive(types::PrimitiveKind::U64);
                    else if (arg == "Isize")
                        arg_type = types::make_primitive(types::PrimitiveKind::I64);
                    else if (arg == "F32")
                        arg_type = types::make_primitive(types::PrimitiveKind::F32);
                    else if (arg == "F64")
                        arg_type = types::make_f64();
                    else if (arg == "Bool")
                        arg_type = types::make_bool();
                    else if (arg == "Str")
                        arg_type = types::make_str();
                    else {
                        // Parse mangled type string properly (handles ptr_, nested generics, etc.)
                        arg_type = parse_mangled_type_string(arg);
                    }
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

        if (!this_inner_type.empty()) {
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
        std::string param_type = llvm_type_ptr(method.params[i].type);
        std::string param_name = get_param_name(method.params[i]);
        // Resolve semantic type for the parameter
        types::TypePtr semantic_type = resolve_parser_type_with_subs(*method.params[i].type, {});
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};
    }

    // Coverage instrumentation - inject call at method entry
    // Uses qualified name like "TypeName::method_name" for library coverage tracking
    emit_coverage(type_name + "::" + method.name);

    // Generate method body
    if (method.body) {
        for (const auto& stmt : method.body->stmts) {
            if (block_terminated_)
                break;
            gen_stmt(*stmt);
        }

        // Handle trailing expression
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*method.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                // Fix: if returning ptr type with "0" placeholder (from loops), use null
                if (ret_type == "ptr" && result == "0") {
                    emit_line("  ret ptr null");
                } else if (result == "0" && ret_type.find("%struct.") == 0) {
                    // Fix: if returning struct type with "0" placeholder (from loops), use
                    // zeroinitializer
                    emit_line("  ret " + ret_type + " zeroinitializer");
                } else {
                    // Handle integer type extension when actual differs from expected
                    std::string final_result = result;
                    std::string actual_type = last_expr_type_;
                    if (actual_type != ret_type) {
                        if (ret_type == "i64" &&
                            (actual_type == "i32" || actual_type == "i16" || actual_type == "i8")) {
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
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

// Generate a specialized version of a generic impl method
// e.g., impl[T] Container[T] { func get() -> T } instantiated for Container[I32]
void LLVMIRGen::gen_impl_method_instantiation(
    const std::string& mangled_type_name, const parser::FuncDecl& method,
    const std::unordered_map<std::string, types::TypePtr>& type_subs,
    const std::vector<parser::GenericParam>& impl_generics, const std::string& method_type_suffix,
    bool is_library_type, const std::string& base_type_name) {
    // Build full method name and check if already generated
    std::string method_name_for_key = method.name;
    if (!method_type_suffix.empty()) {
        method_name_for_key += "__" + method_type_suffix;
    }
    std::string generated_key = "tml_" + mangled_type_name + "_" + method_name_for_key;
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
    std::string saved_impl_type = current_impl_type_;
    bool saved_terminated = block_terminated_;
    auto saved_locals = locals_;
    auto saved_type_subs = current_type_subs_;
    auto saved_where_constraints = current_where_constraints_;

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

    // Build full type_subs including method-level type parameters
    auto full_type_subs = type_subs;

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
    locals_.clear();
    block_terminated_ = false;

    // Determine return type with substitution
    std::string ret_type = "void";
    if (method.return_type.has_value()) {
        auto resolved_ret = resolve_parser_type_with_subs(**method.return_type, full_type_subs);
        ret_type = llvm_type_from_semantic(resolved_ret);
    }
    current_ret_type_ = ret_type;

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
        params = this_type + " %this";
        param_types = this_type;
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
        std::string param_type = llvm_type_from_semantic(resolved_param);
        std::string param_name = get_param_name(method.params[i]);
        params += param_type + " %" + param_name;
        param_types += param_type;
    }

    // Function signature - only use suite prefix for test-local types
    // Library types (from imported modules) don't use suite prefix since they're shared
    std::string suite_prefix = "";
    if (!is_library_type) {
        // Test-local type - use suite prefix for isolation
        suite_prefix = get_suite_prefix();
    }
    std::string func_llvm_name = "tml_" + suite_prefix + mangled_type_name + "_" + full_method_name;

    // Register the function in functions_ so call sites can find it
    // This is crucial for suite mode where multiple test files may call this method
    std::string func_type = ret_type + " (" + param_types + ")";
    std::vector<std::string> param_types_vec;
    if (is_instance_method) {
        param_types_vec.push_back(this_type);
    }
    for (size_t i = param_start; i < method.params.size(); ++i) {
        auto resolved_param = resolve_parser_type_with_subs(*method.params[i].type, full_type_subs);
        param_types_vec.push_back(llvm_type_from_semantic(resolved_param));
    }
    functions_[method_name] = FuncInfo{"@" + func_llvm_name, func_type, ret_type, param_types_vec};

    emit_line("");
    // Use internal linkage for all methods to avoid duplicate symbol warnings
    // Each object file gets its own copy of library methods - slight code bloat
    // but avoids complex COMDAT merging issues with LLD on Windows
    emit_line("define internal " + ret_type + " @" + func_llvm_name + "(" + params + ") #0 {");
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

        // Use base_type_name with type_args so method lookup finds "RawPtr::offset" not
        // "RawPtr__I64::offset"
        if (!base_type_name.empty()) {
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
        std::string param_name = get_param_name(method.params[i]);
        auto resolved_param = resolve_parser_type_with_subs(*method.params[i].type, full_type_subs);
        std::string param_type = llvm_type_from_semantic(resolved_param);
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, resolved_param, std::nullopt};
    }

    // Coverage instrumentation - inject call at method entry
    // Uses base type name for better readability (e.g., "Arc::new" instead of "Arc__I32::new")
    {
        std::string type_for_coverage = base_type_name.empty() ? mangled_type_name : base_type_name;
        emit_coverage(type_for_coverage + "::" + method.name);
    }

    // Generate method body
    if (method.body.has_value()) {
        // Push drop scope for method body (enables RAII for local variables)
        push_drop_scope();

        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }
        if (method.body->expr.has_value()) {
            std::string result = gen_expr(*method.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                // Emit drops before returning
                emit_all_drops();
                // Fix: if returning ptr type with "0" placeholder (from loops), use null
                if (ret_type == "ptr" && result == "0") {
                    emit_line("  ret ptr null");
                } else if (result == "0" && ret_type.find("%struct.") == 0) {
                    // Fix: if returning struct type with "0" placeholder (from loops), use
                    // zeroinitializer
                    emit_line("  ret " + ret_type + " zeroinitializer");
                } else {
                    // Handle integer type extension when actual differs from expected
                    std::string final_result = result;
                    std::string actual_type = last_expr_type_;
                    if (actual_type != ret_type) {
                        if (ret_type == "i64" &&
                            (actual_type == "i32" || actual_type == "i16" || actual_type == "i8")) {
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
    current_impl_type_ = saved_impl_type;
    current_type_subs_ = saved_type_subs;
    current_where_constraints_ = saved_where_constraints;
    block_terminated_ = saved_terminated;
    locals_ = saved_locals;
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

} // namespace tml::codegen
