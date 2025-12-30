// LLVM IR generator - Collections and paths
// Handles: gen_array, gen_index, gen_path

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_array(const parser::ArrayExpr& arr) -> std::string {
    // Array literals create dynamic lists: [1, 2, 3] -> list_create + list_push for each element

    if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
        // [elem1, elem2, elem3, ...]
        const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
        size_t count = elements.size();

        // Create list with initial capacity
        size_t capacity = count > 0 ? count : 4;
        std::string list_ptr = fresh_reg();
        emit_line("  " + list_ptr + " = call ptr @list_create(i32 " + std::to_string(capacity) +
                  ")");

        // Push each element
        for (const auto& elem : elements) {
            std::string val = gen_expr(*elem);
            std::string call_result = fresh_reg();
            emit_line("  " + call_result + " = call i32 @list_push(ptr " + list_ptr + ", i32 " +
                      val + ")");
        }

        return list_ptr;
    } else {
        // [expr; count] - repeat expression count times
        const auto& pair = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);
        std::string init_val = gen_expr(*pair.first);
        std::string count_val = gen_expr(*pair.second);

        // Create list with capacity from count
        std::string list_ptr = fresh_reg();
        emit_line("  " + list_ptr + " = call ptr @list_create(i32 " + count_val + ")");

        // Loop to push init_val count times
        std::string label_cond = fresh_label("arr.cond");
        std::string label_body = fresh_label("arr.body");
        std::string label_end = fresh_label("arr.end");

        // Counter alloca
        std::string counter_ptr = fresh_reg();
        emit_line("  " + counter_ptr + " = alloca i32");
        emit_line("  store i32 0, ptr " + counter_ptr);

        emit_line("  br label %" + label_cond);
        emit_line(label_cond + ":");

        std::string counter_val = fresh_reg();
        emit_line("  " + counter_val + " = load i32, ptr " + counter_ptr);
        std::string cmp = fresh_reg();
        emit_line("  " + cmp + " = icmp slt i32 " + counter_val + ", " + count_val);
        emit_line("  br i1 " + cmp + ", label %" + label_body + ", label %" + label_end);

        emit_line(label_body + ":");
        std::string push_result = fresh_reg();
        emit_line("  " + push_result + " = call i32 @list_push(ptr " + list_ptr + ", i32 " +
                  init_val + ")");

        std::string next_counter = fresh_reg();
        emit_line("  " + next_counter + " = add nsw i32 " + counter_val + ", 1");
        emit_line("  store i32 " + next_counter + ", ptr " + counter_ptr);
        emit_line("  br label %" + label_cond);

        emit_line(label_end + ":");
        block_terminated_ = false;

        return list_ptr;
    }
}

auto LLVMIRGen::gen_index(const parser::IndexExpr& idx) -> std::string {
    // arr[i] -> list_get(arr, i)
    std::string arr_ptr = gen_expr(*idx.object);
    std::string index_val = gen_expr(*idx.index);

    std::string result = fresh_reg();
    emit_line("  " + result + " = call i32 @list_get(ptr " + arr_ptr + ", i32 " + index_val + ")");

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
