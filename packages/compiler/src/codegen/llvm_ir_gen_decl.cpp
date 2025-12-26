// LLVM IR generator - Declaration generation
// Handles: struct, enum, function declarations

#include "tml/codegen/llvm_ir_gen.hpp"
#include "tml/types/type.hpp"

namespace tml::codegen {

void LLVMIRGen::gen_struct_decl(const parser::StructDecl& s) {
    // If struct has generic parameters, defer generation until instantiated
    if (!s.generics.empty()) {
        pending_generic_structs_[s.name] = &s;
        return;
    }

    // Non-generic struct: generate immediately
    std::string type_name = "%struct." + s.name;

    // Collect field types and register field info
    std::vector<std::string> field_types;
    std::vector<FieldInfo> fields;
    for (size_t i = 0; i < s.fields.size(); ++i) {
        std::string ft = llvm_type_ptr(s.fields[i].type);
        field_types.push_back(ft);
        fields.push_back({s.fields[i].name, static_cast<int>(i), ft});
    }

    // Emit struct type definition
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0) def += ", ";
        def += field_types[i];
    }
    def += " }";
    emit_line(def);

    // Register for later use
    struct_types_[s.name] = type_name;
    struct_fields_[s.name] = fields;
}

// Generate a specialized version of a generic struct
void LLVMIRGen::gen_struct_instantiation(
    const parser::StructDecl& decl,
    const std::vector<types::TypePtr>& type_args
) {
    // 1. Create substitution map: T -> I32, K -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < decl.generics.size() && i < type_args.size(); ++i) {
        subs[decl.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled name: Pair[I32] -> Pair__I32
    std::string mangled = mangle_struct_name(decl.name, type_args);
    std::string type_name = "%struct." + mangled;

    // 3. Collect field types with substitution and register field info
    std::vector<std::string> field_types;
    std::vector<FieldInfo> fields;
    for (size_t i = 0; i < decl.fields.size(); ++i) {
        // Resolve field type and apply substitution
        types::TypePtr field_type = resolve_parser_type_with_subs(*decl.fields[i].type, subs);
        std::string ft = llvm_type_from_semantic(field_type);
        field_types.push_back(ft);
        fields.push_back({decl.fields[i].name, static_cast<int>(i), ft});
    }

    // 4. Emit struct type definition
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0) def += ", ";
        def += field_types[i];
    }
    def += " }";
    emit_line(def);

    // 5. Register for later use
    struct_types_[mangled] = type_name;
    struct_fields_[mangled] = fields;
}

// Request instantiation of a generic struct - returns mangled name
auto LLVMIRGen::require_struct_instantiation(
    const std::string& base_name,
    const std::vector<types::TypePtr>& type_args
) -> std::string {
    // Generate mangled name
    std::string mangled = mangle_struct_name(base_name, type_args);

    // Check if already registered
    auto it = struct_instantiations_.find(mangled);
    if (it != struct_instantiations_.end()) {
        return mangled;  // Already queued or generated
    }

    // Register new instantiation
    struct_instantiations_[mangled] = GenericInstantiation{
        base_name,
        type_args,
        mangled,
        false  // Not yet generated
    };

    // Also register field info immediately so it's available during code generation
    // (struct_fields_ is used before generate_pending_instantiations runs)
    auto decl_it = pending_generic_structs_.find(base_name);
    if (decl_it != pending_generic_structs_.end()) {
        const parser::StructDecl* decl = decl_it->second;

        // Create substitution map
        std::unordered_map<std::string, types::TypePtr> subs;
        for (size_t i = 0; i < decl->generics.size() && i < type_args.size(); ++i) {
            subs[decl->generics[i].name] = type_args[i];
        }

        // Register field info
        std::vector<FieldInfo> fields;
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            types::TypePtr field_type = resolve_parser_type_with_subs(*decl->fields[i].type, subs);
            std::string ft = llvm_type_from_semantic(field_type);
            fields.push_back({decl->fields[i].name, static_cast<int>(i), ft});
        }
        struct_fields_[mangled] = fields;
    }

    return mangled;
}

void LLVMIRGen::gen_enum_decl(const parser::EnumDecl& e) {
    // If enum has generic parameters, defer generation until instantiated
    if (!e.generics.empty()) {
        pending_generic_enums_[e.name] = &e;
        return;
    }

    // TML enums are represented as tagged unions
    // For now, simple enums are just integers (tag only)
    // Complex enums with data would need { i32, union_data }

    bool has_data = false;
    for (const auto& variant : e.variants) {
        if (variant.tuple_fields.has_value() || variant.struct_fields.has_value()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        // Simple enum - represented as struct with single i32 tag field
        std::string type_name = "%struct." + e.name;
        emit_line(type_name + " = type { i32 }");
        struct_types_[e.name] = type_name;

        // Register variant values
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    } else {
        // Complex enum with data - create tagged union
        // Find the largest variant
        size_t max_size = 0;
        for (const auto& variant : e.variants) {
            size_t size = 0;
            if (variant.tuple_fields.has_value()) {
                for (const auto& field_type : *variant.tuple_fields) {
                    // Approximate size
                    std::string ty = llvm_type_ptr(field_type);
                    if (ty == "i8") size += 1;
                    else if (ty == "i16") size += 2;
                    else if (ty == "i32" || ty == "float") size += 4;
                    else if (ty == "i64" || ty == "double" || ty == "ptr") size += 8;
                    else size += 8; // Default
                }
            }
            if (variant.struct_fields.has_value()) {
                for (const auto& field : *variant.struct_fields) {
                    std::string ty = llvm_type_ptr(field.type);
                    if (ty == "i8") size += 1;
                    else if (ty == "i16") size += 2;
                    else if (ty == "i32" || ty == "float") size += 4;
                    else if (ty == "i64" || ty == "double" || ty == "ptr") size += 8;
                    else size += 8; // Default
                }
            }
            max_size = std::max(max_size, size);
        }

        // Emit the enum type
        std::string type_name = "%struct." + e.name;
        // { i32 tag, [N x i8] data }
        emit_line(type_name + " = type { i32, [" + std::to_string(max_size) + " x i8] }");
        struct_types_[e.name] = type_name;

        // Register variant values
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }
}

// Generate a specialized version of a generic enum
void LLVMIRGen::gen_enum_instantiation(
    const parser::EnumDecl& decl,
    const std::vector<types::TypePtr>& type_args
) {
    // 1. Create substitution map: T -> I32, K -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < decl.generics.size() && i < type_args.size(); ++i) {
        subs[decl.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled name: Maybe[I32] -> Maybe__I32
    std::string mangled = mangle_struct_name(decl.name, type_args);
    std::string type_name = "%struct." + mangled;

    // Check if any variant has data
    bool has_data = false;
    for (const auto& variant : decl.variants) {
        if (variant.tuple_fields.has_value() || variant.struct_fields.has_value()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        // Simple enum - just a tag
        emit_line(type_name + " = type { i32 }");
        struct_types_[mangled] = type_name;

        int tag = 0;
        for (const auto& variant : decl.variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    } else {
        // Complex enum with data
        // Calculate max size with substituted types
        size_t max_size = 0;
        for (const auto& variant : decl.variants) {
            size_t size = 0;
            if (variant.tuple_fields.has_value()) {
                for (const auto& field_type : *variant.tuple_fields) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*field_type, subs);
                    std::string ty = llvm_type_from_semantic(resolved);
                    if (ty == "i8") size += 1;
                    else if (ty == "i16") size += 2;
                    else if (ty == "i32" || ty == "float" || ty == "i1") size += 4;
                    else if (ty == "i64" || ty == "double" || ty == "ptr") size += 8;
                    else size += 8;
                }
            }
            if (variant.struct_fields.has_value()) {
                for (const auto& field : *variant.struct_fields) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*field.type, subs);
                    std::string ty = llvm_type_from_semantic(resolved);
                    if (ty == "i8") size += 1;
                    else if (ty == "i16") size += 2;
                    else if (ty == "i32" || ty == "float" || ty == "i1") size += 4;
                    else if (ty == "i64" || ty == "double" || ty == "ptr") size += 8;
                    else size += 8;
                }
            }
            max_size = std::max(max_size, size);
        }

        // Ensure at least 8 bytes for data
        if (max_size == 0) max_size = 8;

        emit_line(type_name + " = type { i32, [" + std::to_string(max_size) + " x i8] }");
        struct_types_[mangled] = type_name;

        int tag = 0;
        for (const auto& variant : decl.variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }
}

// Helper to extract name from FuncParam pattern
static std::string get_param_name(const parser::FuncParam& param) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    }
    return "_anon";
}

void LLVMIRGen::gen_func_decl(const parser::FuncDecl& func) {
    // Defer generic functions - they will be instantiated when called
    if (!func.generics.empty()) {
        pending_generic_funcs_[func.name] = &func;
        return;
    }

    current_func_ = func.name;
    locals_.clear();
    block_terminated_ = false;

    // Determine return type
    std::string ret_type = "void";
    if (func.return_type.has_value()) {
        ret_type = llvm_type_ptr(*func.return_type);
    }

    // Store the return type for use in gen_return
    current_ret_type_ = ret_type;

    // Build parameter list and type list for function signature
    std::string params;
    std::string param_types;
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
            param_types += ", ";
        }
        std::string param_type = llvm_type_ptr(func.params[i].type);
        std::string param_name = get_param_name(func.params[i]);
        params += param_type + " %" + param_name;
        param_types += param_type;
    }

    // Register function for first-class function support
    std::string func_type = ret_type + " (" + param_types + ")";
    functions_[func.name] = FuncInfo{
        "@tml_" + func.name,
        func_type,
        ret_type
    };

    // Function signature with optimization attributes
    // All user-defined functions get tml_ prefix (main becomes tml_main, wrapper @main calls it)
    std::string func_llvm_name = "tml_" + func.name;
    std::string linkage = (func.name == "main") ? "" : "internal ";
    // Optimization attributes:
    // - nounwind: function doesn't throw exceptions
    // - mustprogress: function will eventually return (enables loop optimizations)
    // - willreturn: function will return (helps with dead code elimination)
    std::string attrs = " #0";
    emit_line("");
    emit_line("define " + linkage + ret_type + " @" + func_llvm_name + "(" + params + ")" + attrs + " {");
    emit_line("entry:");

    // Register function parameters in locals_ by creating allocas
    for (size_t i = 0; i < func.params.size(); ++i) {
        std::string param_type = llvm_type_ptr(func.params[i].type);
        std::string param_name = get_param_name(func.params[i]);
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, nullptr};
    }

    // Coverage instrumentation - inject call at function entry
    if (options_.coverage_enabled) {
        std::string func_name_str = add_string_literal(func.name);
        emit_line("  call void @tml_cover_func(ptr " + func_name_str + ")");
    }

    // Generate function body
    if (func.body) {
        for (const auto& stmt : func.body->stmts) {
            if (block_terminated_) {
                // Block already terminated, skip remaining statements
                break;
            }
            gen_stmt(*stmt);
        }

        // Handle trailing expression (return value)
        if (func.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*func.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + result);
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
        } else {
            // For other types, return zeroinitializer
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");
    current_func_.clear();
    current_ret_type_.clear();
}

void LLVMIRGen::gen_impl_method(const std::string& type_name, const parser::FuncDecl& method) {
    // Skip generic methods for now (they will be instantiated when called)
    if (!method.generics.empty()) {
        return;
    }

    std::string method_name = type_name + "_" + method.name;
    current_func_ = method_name;
    current_impl_type_ = type_name;  // Set impl type for 'this' field access
    locals_.clear();
    block_terminated_ = false;

    // Determine return type
    std::string ret_type = "void";
    if (method.return_type.has_value()) {
        ret_type = llvm_type_ptr(*method.return_type);
    }
    current_ret_type_ = ret_type;

    // Build parameter list - methods have an implicit 'this' parameter
    std::string params;
    std::string param_types = "ptr";  // 'this' is always a pointer

    // Check if first param is 'this' or 'mut this'
    bool has_explicit_this = false;
    size_t param_start = 0;
    if (!method.params.empty()) {
        const auto& first_param = method.params[0];
        std::string first_name = get_param_name(first_param);
        if (first_name == "this") {
            has_explicit_this = true;
            param_start = 1;  // Skip 'this' in param loop since we handle it specially
        }
    }

    // Add 'this' pointer as first parameter
    params = "ptr %this";

    // Add remaining parameters
    for (size_t i = param_start; i < method.params.size(); ++i) {
        params += ", ";
        param_types += ", ";
        std::string param_type = llvm_type_ptr(method.params[i].type);
        std::string param_name = get_param_name(method.params[i]);
        params += param_type + " %" + param_name;
        param_types += param_type;
    }

    // Function signature
    std::string func_llvm_name = "tml_" + type_name + "_" + method.name;
    emit_line("");
    emit_line("define internal " + ret_type + " @" + func_llvm_name + "(" + params + ") #0 {");
    emit_line("entry:");

    // Register 'this' in locals - it's already a pointer, don't store it
    locals_["this"] = VarInfo{"%this", "ptr", nullptr};

    // Register other parameters in locals by creating allocas
    for (size_t i = param_start; i < method.params.size(); ++i) {
        std::string param_type = llvm_type_ptr(method.params[i].type);
        std::string param_name = get_param_name(method.params[i]);
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, nullptr};
    }

    // Generate method body
    if (method.body) {
        for (const auto& stmt : method.body->stmts) {
            if (block_terminated_) break;
            gen_stmt(*stmt);
        }

        // Handle trailing expression
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*method.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + result);
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
}

// Generate a specialized version of a generic function
void LLVMIRGen::gen_func_instantiation(
    const parser::FuncDecl& func,
    const std::vector<types::TypePtr>& type_args
) {
    // 1. Create substitution map: T -> I32, U -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < func.generics.size() && i < type_args.size(); ++i) {
        subs[func.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled function name: identity[I32] -> identity__I32
    std::string mangled = mangle_func_name(func.name, type_args);

    // Save current context
    std::string saved_func = current_func_;
    std::string saved_ret_type = current_ret_type_;
    bool saved_terminated = block_terminated_;
    auto saved_locals = locals_;

    current_func_ = mangled;
    locals_.clear();
    block_terminated_ = false;

    // 3. Determine return type with substitution
    std::string ret_type = "void";
    if (func.return_type.has_value()) {
        types::TypePtr resolved_ret = resolve_parser_type_with_subs(**func.return_type, subs);
        ret_type = llvm_type_from_semantic(resolved_ret);
    }
    current_ret_type_ = ret_type;

    // 4. Build parameter list with substituted types
    std::string params;
    std::string param_types;
    std::vector<std::pair<std::string, std::string>> param_info;  // name -> type

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
            param_types += ", ";
        }
        // Resolve param type with substitution
        types::TypePtr resolved_param = resolve_parser_type_with_subs(*func.params[i].type, subs);
        std::string param_type = llvm_type_from_semantic(resolved_param);
        std::string param_name = get_param_name(func.params[i]);

        params += param_type + " %" + param_name;
        param_types += param_type;
        param_info.push_back({param_name, param_type});
    }

    // 5. Register function for first-class function support
    std::string func_type = ret_type + " (" + param_types + ")";
    functions_[mangled] = FuncInfo{
        "@tml_" + mangled,
        func_type,
        ret_type
    };

    // 6. Emit function definition
    std::string attrs = " #0";
    emit_line("");
    emit_line("define internal " + ret_type + " @tml_" + mangled + "(" + params + ")" + attrs + " {");
    emit_line("entry:");

    // 7. Register parameters in locals_
    for (const auto& [param_name, param_type] : param_info) {
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, nullptr};
    }

    // 8. Generate function body
    if (func.body) {
        for (const auto& stmt : func.body->stmts) {
            if (block_terminated_) break;
            gen_stmt(*stmt);
        }

        // Handle trailing expression (return value)
        if (func.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*func.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + result);
                block_terminated_ = true;
            }
        }
    }

    // 9. Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");

    // Restore context
    current_func_ = saved_func;
    current_ret_type_ = saved_ret_type;
    block_terminated_ = saved_terminated;
    locals_ = saved_locals;
}
} // namespace tml::codegen