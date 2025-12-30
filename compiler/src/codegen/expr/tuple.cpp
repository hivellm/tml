// LLVM IR generator - Tuple expression generation
// Handles: tuple expressions like (a, b, c)

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_tuple(const parser::TupleExpr& tuple) -> std::string {
    // Empty tuple is unit type
    if (tuple.elements.empty()) {
        last_expr_type_ = "{}";
        return "zeroinitializer";
    }

    // Generate each element and collect their values and types
    std::vector<std::string> element_values;
    std::vector<std::string> element_types;

    for (const auto& elem : tuple.elements) {
        std::string val = gen_expr(*elem);
        element_values.push_back(val);
        element_types.push_back(last_expr_type_);
    }

    // Build the tuple type string: { i32, i64, ptr } etc.
    std::string tuple_type = "{ ";
    for (size_t i = 0; i < element_types.size(); ++i) {
        if (i > 0)
            tuple_type += ", ";
        tuple_type += element_types[i];
    }
    tuple_type += " }";

    // Allocate tuple on stack
    std::string ptr = fresh_reg();
    emit_line("  " + ptr + " = alloca " + tuple_type);

    // Store each element
    for (size_t i = 0; i < element_values.size(); ++i) {
        std::string field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr inbounds " + tuple_type + ", ptr " + ptr +
                  ", i32 0, i32 " + std::to_string(i));
        emit_line("  store " + element_types[i] + " " + element_values[i] + ", ptr " + field_ptr);
    }

    // Load the tuple value
    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + tuple_type + ", ptr " + ptr);

    last_expr_type_ = tuple_type;
    return result;
}

} // namespace tml::codegen
