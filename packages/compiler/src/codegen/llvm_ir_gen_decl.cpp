// LLVM IR generator - Declaration generation
// Handles: struct, enum, function declarations

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

void LLVMIRGen::gen_struct_decl(const parser::StructDecl& s) {
    std::string type_name = "%struct." + s.name;

    // Collect field types
    std::vector<std::string> field_types;
    for (const auto& field : s.fields) {
        field_types.push_back(llvm_type_ptr(field.type));
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
}

void LLVMIRGen::gen_enum_decl(const parser::EnumDecl& e) {
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
        // Simple enum - just use i32 for the tag
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

// Helper to extract name from FuncParam pattern
static std::string get_param_name(const parser::FuncParam& param) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    }
    return "_anon";
}

void LLVMIRGen::gen_func_decl(const parser::FuncDecl& func) {
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

    // Allocate parameters as locals (so they can be mutated)
    for (const auto& param : func.params) {
        std::string param_type = llvm_type_ptr(param.type);
        std::string param_name = get_param_name(param);
        std::string alloc = fresh_reg();
        emit_line("  " + alloc + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloc);
        locals_[param_name] = {alloc, param_type};
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

} // namespace tml::codegen
