TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Generic Class & Method Generation
//!
//! This file handles generic class instantiation helpers, generic static method
//! generation (method-level generics), and class method generation.
//!
//! Extracted from class_codegen.cpp to reduce file size.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/token.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

namespace tml::codegen {

// Helper to extract name from FuncParam pattern (duplicated from class_codegen.cpp)
static std::string get_class_param_name(const parser::FuncParam& param) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    }
    return "_anon";
}

// ============================================================================
// Generic Class Instantiation Helpers
// ============================================================================

void LLVMIRGen::gen_class_constructor_instantiation(
    [[maybe_unused]] const parser::ClassDecl& c, const parser::ConstructorDecl& ctor,
    const std::string& mangled_name,
    const std::unordered_map<std::string, types::TypePtr>& type_subs) {

    std::string class_type = "%class." + mangled_name;

    // Save current type subs and set new ones
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    // Build parameter list with type substitution
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    for (const auto& param : ctor.params) {
        auto resolved = resolve_parser_type_with_subs(*param.type, type_subs);
        param_types.push_back(llvm_type_from_semantic(resolved));
        param_names.push_back(get_class_param_name(param));
    }

    // Generate unique constructor name based on parameter types (for overloading)
    std::string func_name = "@tml_" + get_suite_prefix() + mangled_name + "_new";
    if (!param_types.empty()) {
        for (const auto& pt : param_types) {
            std::string type_suffix = pt;
            if (type_suffix == "i8")
                type_suffix = "I8";
            else if (type_suffix == "i16")
                type_suffix = "I16";
            else if (type_suffix == "i32")
                type_suffix = "I32";
            else if (type_suffix == "i64")
                type_suffix = "I64";
            else if (type_suffix == "i128")
                type_suffix = "I128";
            else if (type_suffix == "float")
                type_suffix = "F32";
            else if (type_suffix == "double")
                type_suffix = "F64";
            else if (type_suffix == "i1")
                type_suffix = "Bool";
            else if (type_suffix.find("ptr") != std::string::npos ||
                     type_suffix.find("%") != std::string::npos)
                type_suffix = "ptr";
            func_name += "_" + type_suffix;
        }
    }

    // Register constructor in functions_ map
    std::string ctor_key = mangled_name + "_new";
    if (!param_types.empty()) {
        for (const auto& pt : param_types) {
            ctor_key += "_" + pt;
        }
    }
    functions_[ctor_key] = FuncInfo{func_name, "ptr", "ptr", param_types};

    // Function signature - use ptr for opaque pointer mode
    std::string sig = "define ptr " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Allocate object
    std::string obj = fresh_reg();
    emit_line("  " + obj + " = call ptr @malloc(i64 ptrtoint (" + class_type + "* getelementptr (" +
              class_type + ", " + class_type + "* null, i32 1) to i64))");

    // Initialize vtable pointer
    std::string vtable_ptr = fresh_reg();
    emit_line("  " + vtable_ptr + " = getelementptr " + class_type + ", ptr " + obj +
              ", i32 0, i32 0");
    emit_line("  store ptr @vtable." + mangled_name + ", ptr " + vtable_ptr);

    // Generate constructor body
    if (ctor.body) {
        locals_["this"] = VarInfo{obj, class_type + "*", nullptr, std::nullopt};

        for (size_t i = 0; i < param_names.size(); ++i) {
            locals_[param_names[i]] =
                VarInfo{"%" + param_names[i], param_types[i], nullptr, std::nullopt};
        }

        for (const auto& stmt : ctor.body->stmts) {
            gen_stmt(*stmt);
        }

        if (ctor.body->expr.has_value()) {
            gen_expr(*ctor.body->expr.value());
        }

        locals_.erase("this");
    }

    emit_line("  ret ptr " + obj);
    emit_line("}");
    emit_line("");

    // Restore type subs
    current_type_subs_ = saved_subs;
}

void LLVMIRGen::gen_class_method_instantiation(
    [[maybe_unused]] const parser::ClassDecl& c, const parser::ClassMethod& method,
    const std::string& mangled_name,
    const std::unordered_map<std::string, types::TypePtr>& type_subs) {

    if (method.is_abstract) {
        return;
    }

    // Save and set type substitutions
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    std::string func_name = "@tml_" + get_suite_prefix() + mangled_name + "_" + method.name;
    std::string class_type = "%class." + mangled_name;

    // Build parameter list with type substitution
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    if (!method.is_static) {
        param_types.push_back("ptr");
        param_names.push_back("this");
    }

    for (const auto& param : method.params) {
        std::string pname = get_class_param_name(param);
        if (pname == "this")
            continue;

        auto resolved = resolve_parser_type_with_subs(*param.type, type_subs);
        param_types.push_back(llvm_type_from_semantic(resolved));
        param_names.push_back(pname);
    }

    // Return type with substitution
    std::string ret_type = "void";
    if (method.return_type) {
        auto resolved = resolve_parser_type_with_subs(**method.return_type, type_subs);
        ret_type = llvm_type_from_semantic(resolved);
    }

    // Function signature
    std::string sig = "define " + ret_type + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Save and set current return type for gen_return() to use
    std::string saved_ret_type = current_ret_type_;
    current_ret_type_ = ret_type;
    block_terminated_ = false;

    // Set up locals - mark as direct parameters (not allocas)
    for (size_t i = 0; i < param_names.size(); ++i) {
        auto sem_type = std::make_shared<types::Type>();
        if (param_names[i] == "this") {
            sem_type->kind = types::ClassType{mangled_name, "", {}};
        }
        VarInfo var_info;
        var_info.reg = "%" + param_names[i];
        var_info.type = param_types[i];
        var_info.semantic_type = sem_type;
        var_info.is_direct_param = true; // Mark as direct parameter
        locals_[param_names[i]] = var_info;
    }

    // Generate body
    if (method.body) {
        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }

        // Handle trailing expression if not already terminated by a return statement
        if (!block_terminated_ && method.body->expr.has_value()) {
            std::string result = gen_expr(*method.body->expr.value());
            // Only emit return if gen_expr didn't already terminate the block
            // (e.g., if the trailing expression was itself a return)
            if (!block_terminated_ && ret_type != "void") {
                emit_line("  ret " + ret_type + " " + result);
                block_terminated_ = true;
            }
        }
    }

    // Add implicit return if block wasn't terminated
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i64" || ret_type == "i32" || ret_type == "i1") {
            emit_line("  ret " + ret_type + " 0");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }
    emit_line("}");
    emit_line("");

    // Restore return type and type substitutions
    current_ret_type_ = saved_ret_type;
    current_type_subs_ = saved_subs;

    // Clean up locals
    for (const auto& name : param_names) {
        locals_.erase(name);
    }

    // Register method in functions_ map
    functions_[mangled_name + "_" + method.name] =
        FuncInfo{func_name, ret_type, ret_type, param_types};
}

// ============================================================================
// Generic Static Method Generation (Method-Level Generics)
// ============================================================================

void LLVMIRGen::gen_generic_class_static_method(
    const parser::ClassDecl& c, const parser::ClassMethod& method, const std::string& method_suffix,
    const std::unordered_map<std::string, types::TypePtr>& type_subs) {

    if (method.is_abstract || !method.is_static) {
        return;
    }

    // Save and set type substitutions
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    // Function name: @tml_ClassName_methodName_TypeSuffix
    // e.g., @tml_Utils_identity_I32
    std::string func_name =
        "@tml_" + get_suite_prefix() + c.name + "_" + method.name + method_suffix;

    // Build parameter list with type substitution
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    for (const auto& param : method.params) {
        std::string pname = get_class_param_name(param);
        auto resolved = resolve_parser_type_with_subs(*param.type, type_subs);
        param_types.push_back(llvm_type_from_semantic(resolved));
        param_names.push_back(pname);
    }

    // Return type with substitution
    std::string ret_type = "void";
    if (method.return_type) {
        auto resolved = resolve_parser_type_with_subs(*method.return_type.value(), type_subs);
        ret_type = llvm_type_from_semantic(resolved);
    }

    // Function signature
    std::string sig = "define " + ret_type + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Set up locals for parameters
    for (size_t i = 0; i < param_names.size(); ++i) {
        types::TypePtr semantic = nullptr;
        if (i < method.params.size() && method.params[i].type) {
            semantic = resolve_parser_type_with_subs(*method.params[i].type, type_subs);
        }
        locals_[param_names[i]] =
            VarInfo{"%" + param_names[i], param_types[i], semantic, std::nullopt};
    }

    // Generate body
    if (method.body) {
        current_func_ = func_name;
        current_ret_type_ = ret_type;
        block_terminated_ = false;

        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
            if (block_terminated_) {
                break;
            }
        }

        // Generate trailing expression (if any)
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string expr_val = gen_expr(*method.body->expr.value());
            // Return the expression value for non-void methods
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + expr_val);
                block_terminated_ = true;
            }
        }

        // Default return if no explicit return
        if (!block_terminated_) {
            if (ret_type == "void") {
                emit_line("  ret void");
            } else {
                emit_line("  ret " + ret_type + " zeroinitializer");
            }
        }
    }

    emit_line("}");
    emit_line("");

    // Restore type substitutions
    current_type_subs_ = saved_subs;

    // Clean up locals
    for (const auto& name : param_names) {
        locals_.erase(name);
    }

    // Register method in functions_ map
    functions_[c.name + "_" + method.name + method_suffix] =
        FuncInfo{func_name, ret_type, ret_type, param_types};
}

// ============================================================================
// Method Generation
// ============================================================================

void LLVMIRGen::gen_class_method(const parser::ClassDecl& c, const parser::ClassMethod& method) {
    if (method.is_abstract) {
        // Abstract methods have no body
        return;
    }

    // In library_decls_only mode, use no suite prefix (library methods are shared)
    std::string prefix =
        (options_.library_decls_only && !current_module_prefix_.empty()) ? "" : get_suite_prefix();
    std::string func_name = "@tml_" + prefix + c.name + "_" + method.name;
    std::string class_type = "%class." + c.name;

    // Build parameter list - first param is always 'this' for instance methods
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    if (!method.is_static) {
        param_types.push_back("ptr"); // this pointer
        param_names.push_back("this");
    }

    for (const auto& param : method.params) {
        // Skip 'this' parameter - it's already added above for non-static methods
        std::string pname = get_class_param_name(param);
        if (pname == "this") {
            continue;
        }
        param_types.push_back(llvm_type_ptr(param.type));
        param_names.push_back(pname);
    }

    // Return type
    std::string ret_type = "void";
    bool return_value_class_by_value = false;
    std::string value_class_struct_type;
    if (method.return_type) {
        ret_type = llvm_type_ptr(*method.return_type);

        // Check if return type is a value class - return by value instead of ptr
        // This fixes dangling pointer bug for stack-allocated value class objects
        if (ret_type == "ptr" && (*method.return_type)->is<parser::NamedType>()) {
            const auto& named = (*method.return_type)->as<parser::NamedType>();
            std::string return_class_name =
                named.path.segments.empty() ? "" : named.path.segments.back();
            if (!return_class_name.empty() && env_.is_value_class_candidate(return_class_name)) {
                // Return value class by value (struct type) instead of ptr
                value_class_struct_type = "%class." + return_class_name;
                ret_type = value_class_struct_type;
                return_value_class_by_value = true;
            }
        }
    }

    // Build parameter types string for function registration
    std::string param_types_str;
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            param_types_str += ", ";
        param_types_str += param_types[i];
    }

    // Register function in functions_ map for lookup
    std::string method_key = c.name + "_" + method.name;
    std::string func_type_str = ret_type + " (" + param_types_str + ")";
    functions_[method_key] = FuncInfo{func_name, func_type_str, ret_type, param_types};

    // In library_decls_only mode, emit a declare statement for library class methods
    // instead of the full definition. The implementations come from the shared library object.
    if (options_.library_decls_only && !current_module_prefix_.empty()) {
        emit_line("");
        emit_line("declare " + ret_type + " " + func_name + "(" + param_types_str + ")");
        return;
    }

    // Function signature - use internal linkage in suite mode to prevent duplicates
    std::string class_linkage =
        (options_.suite_test_index >= 0 && options_.force_internal_linkage) ? "internal " : "";
    std::string sig = "define " + class_linkage + ret_type + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Set up locals for parameters
    if (!method.is_static) {
        // Create semantic type for 'this' so field access can infer the correct class type
        auto this_type = std::make_shared<types::Type>();
        this_type->kind = types::ClassType{c.name, "", {}};
        // Mark 'this' as direct param - it's a pointer parameter, not an alloca
        locals_["this"] =
            VarInfo{"%this", "ptr", this_type, std::nullopt, false, true /*is_direct_param*/};
    }

    // Set up locals for other parameters (non-this)
    size_t param_idx = method.is_static ? 0 : 1; // Skip 'this' index
    for (const auto& param : method.params) {
        std::string pname = get_class_param_name(param);
        if (pname == "this")
            continue;
        if (param_idx >= param_names.size())
            break;

        // Resolve semantic type for the parameter
        types::TypePtr semantic = nullptr;
        if (param.type) {
            semantic = resolve_parser_type_with_subs(*param.type, {});
        }

        // Mark all class method params as direct - they're pointer parameters, not allocas
        locals_[param_names[param_idx]] = VarInfo{"%" + param_names[param_idx],
                                                  param_types[param_idx],
                                                  semantic,
                                                  std::nullopt,
                                                  false,
                                                  true /*is_direct_param*/};
        ++param_idx;
    }

    // Generate body
    if (method.body) {
        current_func_ = func_name;
        current_ret_type_ = ret_type;
        block_terminated_ = false; // Reset for new method body

        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }

        // Generate trailing expression (if any)
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string expr_val = gen_expr(*method.body->expr.value());
            // Return the expression value for non-void methods
            // Note: If the expression was a ReturnExpr, gen_expr already emitted ret
            // and set block_terminated_, so we check again here
            if (ret_type != "void" && !block_terminated_) {
                // For value classes returned by value, load the struct from pointer
                if (return_value_class_by_value && last_expr_type_ == "ptr") {
                    std::string loaded_struct = fresh_reg();
                    emit_line("  " + loaded_struct + " = load " + value_class_struct_type +
                              ", ptr " + expr_val);
                    emit_line("  ret " + ret_type + " " + loaded_struct);
                } else {
                    emit_line("  ret " + ret_type + " " + expr_val);
                }
                block_terminated_ = true;
            }
        }

        // Add implicit return for void functions
        if (ret_type == "void" && !block_terminated_) {
            emit_line("  ret void");
        }
    }

    emit_line("}");
    emit_line("");

    // Clear locals
    locals_.clear();

    // Register function
    functions_[c.name + "_" + method.name] = FuncInfo{
        func_name, ret_type + " (" + (method.is_static ? "" : "ptr") + ")", ret_type, param_types};
}

} // namespace tml::codegen
