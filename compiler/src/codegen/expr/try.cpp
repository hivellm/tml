// LLVM IR generator - Try operator (!) for error propagation
// Handles unwrapping Outcome[T, E] and Maybe[T] with early return on error

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_try(const parser::TryExpr& try_expr) -> std::string {
    // Generate the expression that returns Outcome[T, E] or Maybe[T]
    std::string expr_val = gen_expr(*try_expr.expr);
    std::string expr_type = last_expr_type_;

    // We need to determine if this is Outcome or Maybe from the type
    // The type should be something like %struct.Outcome__I32__Str or %struct.Maybe__I32
    bool is_outcome = expr_type.find("Outcome") != std::string::npos;
    bool is_maybe = expr_type.find("Maybe") != std::string::npos;

    if (!is_outcome && !is_maybe) {
        // Not an Outcome or Maybe type - just return the value as-is
        // This shouldn't happen if type checking is working correctly
        return expr_val;
    }

    // Create basic blocks for control flow
    std::string ok_block = fresh_label();
    std::string err_block = fresh_label();
    std::string continue_block = fresh_label();

    // Store the value to access its fields
    std::string alloca_reg = fresh_reg();
    emit_line("  " + alloca_reg + " = alloca " + expr_type);
    emit_line("  store " + expr_type + " " + expr_val + ", ptr " + alloca_reg);

    // Extract the tag (discriminant) - always at index 0
    std::string tag_ptr = fresh_reg();
    emit_line("  " + tag_ptr + " = getelementptr inbounds " + expr_type + ", ptr " + alloca_reg +
              ", i32 0, i32 0");
    std::string tag_val = fresh_reg();
    emit_line("  " + tag_val + " = load i32, ptr " + tag_ptr);

    // Branch based on tag:
    // For Outcome: 0 = Ok, 1 = Err
    // For Maybe: 0 = Just, 1 = Nothing
    std::string is_ok = fresh_reg();
    emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
    emit_line("  br i1 " + is_ok + ", label %" + ok_block + ", label %" + err_block);

    // Error/Nothing block - early return
    emit_line(err_block + ":");

    if (is_outcome) {
        // For Outcome, extract the error value and wrap it in Err
        // Get pointer to the data field (union)
        std::string err_data_ptr = fresh_reg();
        emit_line("  " + err_data_ptr + " = getelementptr inbounds " + expr_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");

        // For now, we propagate by returning the entire Outcome as-is
        // This works because the error type should be compatible with the function's return type
        // In a more complete implementation, we'd need to convert between error types

        // Generate return with the original value (it's already an Err)
        // Note: This assumes the function's return type is compatible
        emit_line("  ret " + expr_type + " " + expr_val);
    } else {
        // For Maybe, we return Nothing
        // Create a Nothing value (tag = 1, data = undef)
        // For now, just return the Nothing as-is
        emit_line("  ret " + expr_type + " " + expr_val);
    }

    // Ok/Just block - extract the value and continue
    emit_line(ok_block + ":");

    // Extract the data from the Ok/Just variant
    std::string data_ptr = fresh_reg();
    emit_line("  " + data_ptr + " = getelementptr inbounds " + expr_type + ", ptr " + alloca_reg +
              ", i32 0, i32 1");

    // Determine the inner type
    // For Outcome[T, E], we want T
    // For Maybe[T], we want T
    // We need to figure out the LLVM type for T
    // For now, use a conservative approach - load as the largest reasonable type
    // TODO: Get the actual inner type from type inference

    // Try to determine inner type from the mangled name
    // e.g., Outcome__I32__Str means T=I32, E=Str
    std::string inner_type = "i64"; // Default fallback

    // Find the type arguments in the mangled name
    size_t pos = expr_type.find("__");
    if (pos != std::string::npos) {
        std::string after_name = expr_type.substr(pos + 2);
        // First type arg is the success type
        size_t end_pos = after_name.find("__");
        if (end_pos == std::string::npos) {
            end_pos = after_name.length();
        }
        std::string type_name = after_name.substr(0, end_pos);

        // Convert type name to LLVM type
        if (type_name == "I8" || type_name == "U8")
            inner_type = "i8";
        else if (type_name == "I16" || type_name == "U16")
            inner_type = "i16";
        else if (type_name == "I32" || type_name == "U32")
            inner_type = "i32";
        else if (type_name == "I64" || type_name == "U64")
            inner_type = "i64";
        else if (type_name == "I128" || type_name == "U128")
            inner_type = "i128";
        else if (type_name == "F32")
            inner_type = "float";
        else if (type_name == "F64")
            inner_type = "double";
        else if (type_name == "Bool")
            inner_type = "i1";
        else if (type_name == "Str")
            inner_type = "ptr";
        else
            inner_type = "%struct." + type_name; // User-defined type
    }

    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + inner_type + ", ptr " + data_ptr);

    last_expr_type_ = inner_type;
    return result;
}

} // namespace tml::codegen
