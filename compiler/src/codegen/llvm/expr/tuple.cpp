TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Tuples
//!
//! This file implements tuple expression code generation.
//!
//! ## Tuple Construction
//!
//! `(a, b, c)` creates an anonymous struct:
//! ```llvm
//! %tuple = alloca { i32, i32, i32 }
//! ; store each element
//! ```
//!
//! ## Unit Type
//!
//! Empty tuple `()` is the Unit type, represented as `{}`.
//!
//! ## Tuple Access
//!
//! `tuple.0`, `tuple.1` access elements by index via GEP.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Defined in control/return.cpp — parses "{ i64, %struct.Maybe__I64 }"
// into ["i64", "%struct.Maybe__I64"].
std::vector<std::string> parse_tuple_types_for_coercion(const std::string& tuple_type);

auto LLVMIRGen::gen_tuple(const parser::TupleExpr& tuple) -> std::string {
    // Empty tuple is unit type
    if (tuple.elements.empty()) {
        last_expr_type_ = "{}";
        return "zeroinitializer";
    }

    // When the surrounding context expects a tuple of a known LLVM type
    // (for example, inside `return (1, Just(1))` where the function returns
    // `(I64, Maybe[I64])`), propagate the per-element expected type down
    // into each element's codegen. This lets:
    //   - integer literals pick up the expected width (I64 vs I32)
    //   - bare enum-variant constructors like `Just(1)` monomorphize their
    //     generic payload against the correct instantiation (Maybe[I64]
    //     instead of the fallback Maybe[I32]).
    //
    // We only read `current_ret_type_` if it actually looks like a tuple
    // type of the right arity; otherwise we leave the per-element
    // expected-type slots alone so nested/unrelated tuple expressions keep
    // their existing inference behavior.
    std::vector<std::string> expected_elem_types;
    if (!current_ret_type_.empty() && current_ret_type_.front() == '{') {
        auto parsed = parse_tuple_types_for_coercion(current_ret_type_);
        if (parsed.size() == tuple.elements.size()) {
            expected_elem_types = std::move(parsed);
        }
    }

    // Save the surrounding expected-type hints so we can restore them after
    // each element — we only want these hints to influence the immediate
    // element we're generating, not anything downstream.
    std::string saved_expected_literal = expected_literal_type_;
    std::string saved_expected_enum = expected_enum_type_;

    // Generate each element and collect their values and types
    std::vector<std::string> element_values;
    std::vector<std::string> element_types;

    for (size_t i = 0; i < tuple.elements.size(); ++i) {
        const auto& elem = tuple.elements[i];

        // Reset per-element hints to the surrounding baseline, then apply
        // the tuple-element expected type (if any).
        expected_literal_type_ = saved_expected_literal;
        expected_enum_type_ = saved_expected_enum;

        if (i < expected_elem_types.size()) {
            const std::string& et = expected_elem_types[i];
            // Primitive integer widths — drive integer literal inference.
            if (et == "i1" || et == "i8" || et == "i16" || et == "i32" || et == "i64" ||
                et == "i128" || et == "f32" || et == "f64") {
                expected_literal_type_ = et;
            }
            // Concrete enum/struct instantiation — drive enum constructor
            // monomorphization so e.g. `Just(1)` becomes `Maybe__I64`.
            else if (et.starts_with("%struct.") || et.starts_with("%class.")) {
                expected_enum_type_ = et;
            }
        }

        std::string val = gen_expr(*elem);
        element_values.push_back(val);
        element_types.push_back(last_expr_type_);

        // Mark variable as consumed if element is an identifier (move semantics)
        if (elem->is<parser::IdentExpr>()) {
            const auto& ident = elem->as<parser::IdentExpr>();
            mark_var_consumed(ident.name);
        }
    }

    // Restore the hints we found on entry so post-tuple codegen is unchanged.
    expected_literal_type_ = saved_expected_literal;
    expected_enum_type_ = saved_expected_enum;

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
