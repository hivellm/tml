// LLVM IR generator - Collections and paths
// Handles: gen_array, gen_index, gen_path

#include "codegen/llvm_ir_gen.hpp"
#include "common.hpp"

#include <iostream>

namespace tml::codegen {

auto LLVMIRGen::gen_array(const parser::ArrayExpr& arr) -> std::string {
    // Array literals create fixed-size arrays on the stack: [1, 2, 3] -> [3 x i64]
    // This matches Rust's array semantics where [T; N] is a fixed-size array

    if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
        // [elem1, elem2, elem3, ...]
        const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
        size_t count = elements.size();

        if (count == 0) {
            // Empty array - create empty struct as placeholder
            last_expr_type_ = "[0 x i64]";
            return "zeroinitializer";
        }

        // Infer element type from first element
        types::TypePtr elem_type = infer_expr_type(*elements[0]);
        std::string llvm_elem_type = llvm_type_from_semantic(elem_type, true);

        // Generate array type [N x elem_type]
        std::string array_type = "[" + std::to_string(count) + " x " + llvm_elem_type + "]";

        // Allocate array on stack
        std::string arr_ptr = fresh_reg();
        emit_line("  " + arr_ptr + " = alloca " + array_type);

        // Store each element using GEP
        for (size_t i = 0; i < elements.size(); ++i) {
            std::string val = gen_expr(*elements[i]);
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_type + ", ptr " + arr_ptr +
                      ", i32 0, i32 " + std::to_string(i));
            emit_line("  store " + llvm_elem_type + " " + val + ", ptr " + elem_ptr);
        }

        // Load and return the entire array value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + array_type + ", ptr " + arr_ptr);

        // Track the array type for later use
        last_expr_type_ = array_type;

        return result;
    } else {
        // [expr; count] - repeat expression count times
        const auto& pair = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);

        // Get the count - must be a compile-time constant
        size_t count = 0;
        if (pair.second->is<parser::LiteralExpr>()) {
            const auto& lit = pair.second->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                const auto& val = lit.token.int_value();
                count = static_cast<size_t>(val.value);
            }
        }

        if (count == 0) {
            // Runtime count or zero - fall back to empty array
            last_expr_type_ = "[0 x i64]";
            return "zeroinitializer";
        }

        // Infer element type
        types::TypePtr elem_type = infer_expr_type(*pair.first);
        std::string llvm_elem_type = llvm_type_from_semantic(elem_type, true);
        std::string array_type = "[" + std::to_string(count) + " x " + llvm_elem_type + "]";

        // Generate initial value once
        std::string init_val = gen_expr(*pair.first);

        // Allocate array on stack
        std::string arr_ptr = fresh_reg();
        emit_line("  " + arr_ptr + " = alloca " + array_type);

        // Store the same value in each element
        for (size_t i = 0; i < count; ++i) {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_type + ", ptr " + arr_ptr +
                      ", i32 0, i32 " + std::to_string(i));
            emit_line("  store " + llvm_elem_type + " " + init_val + ", ptr " + elem_ptr);
        }

        // Load and return the entire array value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + array_type + ", ptr " + arr_ptr);

        last_expr_type_ = array_type;

        return result;
    }
}

auto LLVMIRGen::gen_index(const parser::IndexExpr& idx) -> std::string {
    // For fixed-size arrays: arr[i] -> GEP + load
    // For dynamic lists: arr[i] -> list_get(arr, i)

    // First, infer the type of the object to determine if it's an array or list
    types::TypePtr obj_type = infer_expr_type(*idx.object);

    // Check if it's an ArrayType
    if (obj_type && obj_type->is<types::ArrayType>()) {
        const auto& arr_type = obj_type->as<types::ArrayType>();
        std::string elem_llvm_type = llvm_type_from_semantic(arr_type.element, true);
        std::string array_llvm_type =
            "[" + std::to_string(arr_type.size) + " x " + elem_llvm_type + "]";

        // Generate the array - need to get a pointer to it
        // First allocate storage for the array value
        std::string arr_val = gen_expr(*idx.object);
        std::string arr_ptr = fresh_reg();
        emit_line("  " + arr_ptr + " = alloca " + array_llvm_type);
        emit_line("  store " + array_llvm_type + " " + arr_val + ", ptr " + arr_ptr);

        // Generate the index
        std::string index_val = gen_expr(*idx.index);

        // GEP to get element pointer
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr " + array_llvm_type + ", ptr " + arr_ptr +
                  ", i32 0, i32 " + index_val);

        // Load and return the element
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + elem_llvm_type + ", ptr " + elem_ptr);

        last_expr_type_ = elem_llvm_type;
        return result;
    }

    // Check if the last expression type indicates an array (from gen_array)
    if (last_expr_type_.starts_with("[") && last_expr_type_.find(" x ") != std::string::npos) {
        // Parse array type: [N x type]
        size_t x_pos = last_expr_type_.find(" x ");
        size_t end_bracket = last_expr_type_.rfind("]");
        if (x_pos != std::string::npos && end_bracket != std::string::npos) {
            std::string elem_type = last_expr_type_.substr(x_pos + 3, end_bracket - (x_pos + 3));
            std::string array_type = last_expr_type_;

            // Generate the array value and store it
            std::string arr_val = gen_expr(*idx.object);
            std::string arr_ptr = fresh_reg();
            emit_line("  " + arr_ptr + " = alloca " + array_type);
            emit_line("  store " + array_type + " " + arr_val + ", ptr " + arr_ptr);

            // Generate the index
            std::string index_val = gen_expr(*idx.index);

            // GEP to get element pointer
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_type + ", ptr " + arr_ptr +
                      ", i32 0, i32 " + index_val);

            // Load and return the element
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_type + ", ptr " + elem_ptr);

            last_expr_type_ = elem_type;
            return result;
        }
    }

    // Fall back to list_get for dynamic lists
    std::string arr_ptr = gen_expr(*idx.object);
    std::string index_val = gen_expr(*idx.index);

    std::string result = fresh_reg();
    emit_line("  " + result + " = call i32 @list_get(ptr " + arr_ptr + ", i32 " + index_val + ")");

    last_expr_type_ = "i32";
    return result;
}

auto LLVMIRGen::gen_path(const parser::PathExpr& path) -> std::string {
    // Path expressions like Color::Red resolve to enum variant values
    // Join path segments with ::
    std::string full_path;
    for (size_t i = 0; i < path.path.segments.size(); ++i) {
        if (i > 0)
            full_path += "::";
        full_path += path.path.segments[i];
    }

    // Look up in enum variants
    auto it = enum_variants_.find(full_path);
    if (it != enum_variants_.end()) {
        // For enum variants, we need to create a struct { i32 } value
        // Extract the enum type name (first segment)
        std::string enum_name = path.path.segments[0];
        std::string struct_type = "%struct." + enum_name;

        // Allocate the enum struct on stack
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + struct_type);

        // Get pointer to the tag field (GEP with indices 0, 0)
        std::string tag_ptr = fresh_reg();
        emit_line("  " + tag_ptr + " = getelementptr " + struct_type + ", ptr " + alloca_reg +
                  ", i32 0, i32 0");

        // Store the tag value
        emit_line("  store i32 " + std::to_string(it->second) + ", ptr " + tag_ptr);

        // Load the entire struct value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + struct_type + ", ptr " + alloca_reg);

        // Mark last expr type
        last_expr_type_ = struct_type;

        return result;
    }

    // Not found - might be a function or module path
    report_error("Unknown path: " + full_path, path.span);
    return "0";
}

} // namespace tml::codegen
