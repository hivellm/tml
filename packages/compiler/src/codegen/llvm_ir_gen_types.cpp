// LLVM IR generator - Types and collections
// Handles: struct expressions, fields, arrays, indexing, paths, method calls, format print

#include "tml/codegen/llvm_ir_gen.hpp"
#include <algorithm>
#include <cctype>

namespace tml::codegen {

// Generate struct expression, returning pointer to allocated struct
auto LLVMIRGen::gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string {
    std::string type_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type = "%struct." + type_name;

    // Allocate struct on stack
    std::string ptr = fresh_reg();
    emit_line("  " + ptr + " = alloca " + struct_type);

    // Initialize fields
    for (size_t i = 0; i < s.fields.size(); ++i) {
        std::string field_val;
        std::string field_type = "i32";

        // Check if field value is a nested struct
        if (s.fields[i].second->is<parser::StructExpr>()) {
            // Nested struct - allocate and copy
            const auto& nested = s.fields[i].second->as<parser::StructExpr>();
            std::string nested_ptr = gen_struct_expr_ptr(nested);
            std::string nested_type = "%struct." + nested.path.segments.back();
            std::string nested_val = fresh_reg();
            emit_line("  " + nested_val + " = load " + nested_type + ", ptr " + nested_ptr);
            field_val = nested_val;
            field_type = nested_type;
        } else {
            field_val = gen_expr(*s.fields[i].second);
        }

        std::string field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + ptr + ", i32 0, i32 " + std::to_string(i));
        emit_line("  store " + field_type + " " + field_val + ", ptr " + field_ptr);
    }

    return ptr;
}

auto LLVMIRGen::gen_struct_expr(const parser::StructExpr& s) -> std::string {
    std::string ptr = gen_struct_expr_ptr(s);
    std::string type_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type = "%struct." + type_name;

    // Load the struct value
    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + struct_type + ", ptr " + ptr);

    return result;
}

// Helper to get field index for known struct types
static int get_field_index(const std::string& struct_name, const std::string& field_name) {
    // Point fields
    if (struct_name == "Point") {
        if (field_name == "x") return 0;
        if (field_name == "y") return 1;
    }
    // Rectangle fields
    if (struct_name == "Rectangle") {
        if (field_name == "origin") return 0;
        if (field_name == "width") return 1;
        if (field_name == "height") return 2;
    }
    return 0;
}

// Helper to get field type for known struct types
static std::string get_field_type(const std::string& struct_name, const std::string& field_name) {
    if (struct_name == "Rectangle" && field_name == "origin") {
        return "%struct.Point";
    }
    return "i32";
}

auto LLVMIRGen::gen_field(const parser::FieldExpr& field) -> std::string {
    // Handle field access on struct
    std::string struct_type;
    std::string struct_ptr;

    // If the object is an identifier, look up its type
    if (field.object->is<parser::IdentExpr>()) {
        const auto& ident = field.object->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            struct_type = it->second.type;
            struct_ptr = it->second.reg;
        }
    } else if (field.object->is<parser::FieldExpr>()) {
        // Chained field access (e.g., rect.origin.x)
        // First get the nested field pointer
        const auto& nested_field = field.object->as<parser::FieldExpr>();

        // Get the outermost struct
        if (nested_field.object->is<parser::IdentExpr>()) {
            const auto& ident = nested_field.object->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                std::string outer_type = it->second.type;
                std::string outer_ptr = it->second.reg;

                // Get outer struct type name
                std::string outer_name = outer_type;
                if (outer_name.starts_with("%struct.")) {
                    outer_name = outer_name.substr(8);
                }

                // Get field index for nested field
                int nested_idx = get_field_index(outer_name, nested_field.field);
                std::string nested_type = get_field_type(outer_name, nested_field.field);

                // Get pointer to nested field
                std::string nested_ptr = fresh_reg();
                emit_line("  " + nested_ptr + " = getelementptr " + outer_type + ", ptr " + outer_ptr + ", i32 0, i32 " + std::to_string(nested_idx));

                struct_type = nested_type;
                struct_ptr = nested_ptr;
            }
        }
    }

    if (struct_type.empty() || struct_ptr.empty()) {
        report_error("Cannot resolve field access object", field.span);
        return "0";
    }

    // Get struct type name
    std::string type_name = struct_type;
    if (type_name.starts_with("%struct.")) {
        type_name = type_name.substr(8);
    }

    // Get field index and type
    int field_idx = get_field_index(type_name, field.field);
    std::string field_type = get_field_type(type_name, field.field);

    // Use getelementptr to access field, then load
    std::string field_ptr = fresh_reg();
    emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + struct_ptr + ", i32 0, i32 " + std::to_string(field_idx));

    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + field_type + ", ptr " + field_ptr);
    return result;
}

// Generate formatted print: "hello {} world {}" with args
// Supports {}, {:.N} for floats with N decimal places
auto LLVMIRGen::gen_format_print(const std::string& format,
                                   const std::vector<parser::ExprPtr>& args,
                                   size_t start_idx,
                                   bool with_newline) -> std::string {
    // Parse format string and print segments with arguments
    size_t arg_idx = start_idx;
    size_t pos = 0;
    std::string result = "0";

    while (pos < format.size()) {
        // Find next { placeholder
        size_t placeholder = format.find('{', pos);

        if (placeholder == std::string::npos) {
            // No more placeholders, print remaining text
            if (pos < format.size()) {
                std::string remaining = format.substr(pos);
                std::string str_const = add_string_literal(remaining);
                result = fresh_reg();
                emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + str_const + ")");
            }
            break;
        }

        // Print text before placeholder
        if (placeholder > pos) {
            std::string segment = format.substr(pos, placeholder - pos);
            std::string str_const = add_string_literal(segment);
            result = fresh_reg();
            emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + str_const + ")");
        }

        // Parse placeholder: {} or {:.N}
        size_t end_brace = format.find('}', placeholder);
        if (end_brace == std::string::npos) {
            pos = placeholder + 1;
            continue;
        }

        std::string placeholder_content = format.substr(placeholder + 1, end_brace - placeholder - 1);
        int precision = -1; // -1 means no precision specified

        // Parse {:.N} format
        if (placeholder_content.starts_with(":.")) {
            std::string prec_str = placeholder_content.substr(2);
            if (!prec_str.empty() && std::all_of(prec_str.begin(), prec_str.end(), ::isdigit)) {
                precision = std::stoi(prec_str);
            }
        }

        // Print argument
        if (arg_idx < args.size()) {
            const auto& arg_expr = *args[arg_idx];
            std::string arg_val = gen_expr(arg_expr);
            auto arg_type = infer_print_type(arg_expr);

            // For identifiers, check variable type
            if (arg_type == PrintArgType::Unknown && arg_expr.is<parser::IdentExpr>()) {
                const auto& ident = arg_expr.as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    if (it->second.type == "i1") arg_type = PrintArgType::Bool;
                    else if (it->second.type == "i32") arg_type = PrintArgType::Int;
                    else if (it->second.type == "i64") arg_type = PrintArgType::I64;
                    else if (it->second.type == "float" || it->second.type == "double") arg_type = PrintArgType::Float;
                    else if (it->second.type == "ptr") arg_type = PrintArgType::Str;
                }
            }

            // For string constants
            if (arg_val.starts_with("@.str.")) {
                arg_type = PrintArgType::Str;
            }

            result = fresh_reg();
            switch (arg_type) {
                case PrintArgType::Str:
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + arg_val + ")");
                    break;
                case PrintArgType::Bool: {
                    std::string label_true = fresh_label("fmt.true");
                    std::string label_false = fresh_label("fmt.false");
                    std::string label_end = fresh_label("fmt.end");

                    emit_line("  br i1 " + arg_val + ", label %" + label_true + ", label %" + label_false);

                    emit_line(label_true + ":");
                    std::string r1 = fresh_reg();
                    emit_line("  " + r1 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.true)");
                    emit_line("  br label %" + label_end);

                    emit_line(label_false + ":");
                    std::string r2 = fresh_reg();
                    emit_line("  " + r2 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.false)");
                    emit_line("  br label %" + label_end);

                    emit_line(label_end + ":");
                    block_terminated_ = false;
                    break;
                }
                case PrintArgType::I64:
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.i64.no_nl, i64 " + arg_val + ")");
                    break;
                case PrintArgType::Float: {
                    // Check if already double (from variable type or last_expr_type_)
                    bool is_double = (last_expr_type_ == "double");
                    if (!is_double && arg_expr.is<parser::IdentExpr>()) {
                        const auto& ident = arg_expr.as<parser::IdentExpr>();
                        auto it = locals_.find(ident.name);
                        if (it != locals_.end() && it->second.type == "double") {
                            is_double = true;
                        }
                    }

                    std::string double_val;
                    if (is_double) {
                        // Already a double, no conversion needed
                        double_val = arg_val;
                    } else {
                        // For printf, floats are promoted to double
                        double_val = fresh_reg();
                        emit_line("  " + double_val + " = fpext float " + arg_val + " to double");
                    }
                    if (precision >= 0) {
                        // Create custom format string for precision
                        std::string fmt_str = "%." + std::to_string(precision) + "f";
                        std::string fmt_const = add_string_literal(fmt_str);
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr " + fmt_const + ", double " + double_val + ")");
                    } else {
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.float.no_nl, double " + double_val + ")");
                    }
                    break;
                }
                case PrintArgType::Int:
                case PrintArgType::Unknown:
                default:
                    // If precision is specified for int, treat as float for fractional display
                    if (precision >= 0) {
                        // Convert i32 to double for fractional display (e.g., us to ms)
                        std::string double_val = fresh_reg();
                        emit_line("  " + double_val + " = sitofp i32 " + arg_val + " to double");
                        std::string fmt_str = "%." + std::to_string(precision) + "f";
                        std::string fmt_const = add_string_literal(fmt_str);
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr " + fmt_const + ", double " + double_val + ")");
                    } else {
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.int.no_nl, i32 " + arg_val + ")");
                    }
                    break;
            }
            ++arg_idx;
        }

        pos = end_brace + 1; // Skip past }
    }

    // Print newline if println
    if (with_newline) {
        result = fresh_reg();
        emit_line("  " + result + " = call i32 @putchar(i32 10)");
    }

    return result;
}

auto LLVMIRGen::gen_array(const parser::ArrayExpr& arr) -> std::string {
    // Array literals create dynamic lists: [1, 2, 3] -> list_create + list_push for each element

    if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
        // [elem1, elem2, elem3, ...]
        const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
        size_t count = elements.size();

        // Create list with initial capacity
        size_t capacity = count > 0 ? count : 4;
        std::string list_ptr = fresh_reg();
        emit_line("  " + list_ptr + " = call ptr @tml_list_create(i32 " + std::to_string(capacity) + ")");

        // Push each element
        for (const auto& elem : elements) {
            std::string val = gen_expr(*elem);
            std::string call_result = fresh_reg();
            emit_line("  " + call_result + " = call i32 @tml_list_push(ptr " + list_ptr + ", i32 " + val + ")");
        }

        return list_ptr;
    } else {
        // [expr; count] - repeat expression count times
        const auto& pair = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);
        std::string init_val = gen_expr(*pair.first);
        std::string count_val = gen_expr(*pair.second);

        // Create list with capacity from count
        std::string list_ptr = fresh_reg();
        emit_line("  " + list_ptr + " = call ptr @tml_list_create(i32 " + count_val + ")");

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
        emit_line("  " + push_result + " = call i32 @tml_list_push(ptr " + list_ptr + ", i32 " + init_val + ")");

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
    emit_line("  " + result + " = call i32 @tml_list_get(ptr " + arr_ptr + ", i32 " + index_val + ")");

    return result;
}

auto LLVMIRGen::gen_path(const parser::PathExpr& path) -> std::string {
    // Path expressions like Color::Red resolve to enum variant values
    // Join path segments with ::
    std::string full_path;
    for (size_t i = 0; i < path.path.segments.size(); ++i) {
        if (i > 0) full_path += "::";
        full_path += path.path.segments[i];
    }

    // Look up in enum variants
    auto it = enum_variants_.find(full_path);
    if (it != enum_variants_.end()) {
        return std::to_string(it->second);
    }

    // Not found - might be a function or module path
    report_error("Unknown path: " + full_path, path.span);
    return "0";
}

auto LLVMIRGen::gen_method_call(const parser::MethodCallExpr& call) -> std::string {
    // Generate receiver (the object the method is called on)
    std::string receiver = gen_expr(*call.receiver);
    const std::string& method = call.method;

    // Map method names to runtime functions
    // List methods
    if (method == "len" || method == "length") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_list_len(ptr " + receiver + ")");
        return result;
    }
    if (method == "push") {
        if (call.args.empty()) {
            report_error("push requires an argument", call.span);
            return "0";
        }
        std::string val = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_list_push(ptr " + receiver + ", i32 " + val + ")");
        return result;
    }
    if (method == "pop") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_list_pop(ptr " + receiver + ")");
        return result;
    }
    if (method == "get") {
        if (call.args.empty()) {
            report_error("get requires an index argument", call.span);
            return "0";
        }
        std::string idx = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_list_get(ptr " + receiver + ", i32 " + idx + ")");
        return result;
    }
    if (method == "set") {
        if (call.args.size() < 2) {
            report_error("set requires index and value arguments", call.span);
            return "0";
        }
        std::string idx = gen_expr(*call.args[0]);
        std::string val = gen_expr(*call.args[1]);
        emit_line("  call void @tml_list_set(ptr " + receiver + ", i32 " + idx + ", i32 " + val + ")");
        return "void";
    }
    if (method == "clear") {
        emit_line("  call void @tml_list_clear(ptr " + receiver + ")");
        return "void";
    }
    if (method == "is_empty" || method == "isEmpty") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @tml_list_is_empty(ptr " + receiver + ")");
        return result;
    }
    if (method == "capacity") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_list_capacity(ptr " + receiver + ")");
        return result;
    }

    // HashMap methods
    if (method == "has" || method == "contains") {
        if (call.args.empty()) {
            report_error("has requires a key argument", call.span);
            return "0";
        }
        std::string key = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @tml_hashmap_has(ptr " + receiver + ", i32 " + key + ")");
        return result;
    }
    if (method == "remove") {
        if (call.args.empty()) {
            report_error("remove requires a key argument", call.span);
            return "0";
        }
        std::string key = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @tml_hashmap_remove(ptr " + receiver + ", i32 " + key + ")");
        return result;
    }

    // Buffer methods
    if (method == "write_byte") {
        if (call.args.empty()) {
            report_error("write_byte requires a value argument", call.span);
            return "0";
        }
        std::string val = gen_expr(*call.args[0]);
        emit_line("  call void @tml_buffer_write_byte(ptr " + receiver + ", i32 " + val + ")");
        return "void";
    }
    if (method == "read_byte") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_buffer_read_byte(ptr " + receiver + ")");
        return result;
    }
    if (method == "remaining") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_buffer_remaining(ptr " + receiver + ")");
        return result;
    }

    report_error("Unknown method: " + method, call.span);
    return "0";
}

} // namespace tml::codegen
