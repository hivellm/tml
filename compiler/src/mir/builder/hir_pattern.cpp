//! # HIR Pattern Lowering to MIR
//!
//! This file implements pattern matching and binding from HIR to MIR.
//! Patterns are used in let bindings, when arms, and function parameters.
//!
//! ## Pattern Types
//!
//! - **Wildcard** (`_`): Matches anything, doesn't bind
//! - **Binding** (`x`, `mut x`): Binds value to variable
//! - **Literal** (`42`, `true`): Matches exact value
//! - **Tuple** (`(a, b)`): Destructures tuple
//! - **Struct** (`Point { x, y }`): Destructures struct
//! - **Enum** (`Just(v)`): Matches enum variant
//! - **Or** (`a | b`): Alternative patterns
//! - **Range** (`0 to 10`): Range match

#include "mir/hir_mir_builder.hpp"

namespace tml::mir {

// ============================================================================
// Pattern Binding
// ============================================================================

void HirMirBuilder::build_pattern_binding(const hir::HirPatternPtr& pattern, Value value) {
    if (!pattern) {
        return;
    }

    std::visit(
        [this, &value](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, hir::HirWildcardPattern>) {
                // Wildcard - nothing to bind
            } else if constexpr (std::is_same_v<T, hir::HirBindingPattern>) {
                // Simple binding - set variable with correct type
                Value typed_value = value;
                if (!typed_value.type) {
                    typed_value.type = convert_type(p.type);
                }
                set_variable(p.name, typed_value);
            } else if constexpr (std::is_same_v<T, hir::HirLiteralPattern>) {
                // Literal pattern in binding position - nothing to bind
            } else if constexpr (std::is_same_v<T, hir::HirTuplePattern>) {
                // Destructure tuple
                for (size_t i = 0; i < p.elements.size(); ++i) {
                    ExtractValueInst extract;
                    extract.aggregate = value;
                    extract.indices = {static_cast<uint32_t>(i)};
                    extract.aggregate_type = value.type;

                    // Get element type from tuple type
                    MirTypePtr elem_type = make_unit_type();
                    if (auto* tuple_type = std::get_if<MirTupleType>(&value.type->kind)) {
                        if (i < tuple_type->elements.size()) {
                            elem_type = tuple_type->elements[i];
                        }
                    }
                    extract.result_type = elem_type;

                    Value elem = emit(extract, elem_type);
                    build_pattern_binding(p.elements[i], elem);
                }
            } else if constexpr (std::is_same_v<T, hir::HirStructPattern>) {
                // Destructure struct
                // fields is std::vector<std::pair<std::string, HirPatternPtr>>
                for (size_t i = 0; i < p.fields.size(); ++i) {
                    const auto& field = p.fields[i];
                    const std::string& field_name = field.first;
                    const hir::HirPatternPtr& field_pattern = field.second;

                    // Get field type from the field pattern if available
                    MirTypePtr field_type = make_unit_type();
                    if (field_pattern) {
                        field_type = convert_type(field_pattern->type());
                    }

                    ExtractValueInst extract;
                    extract.aggregate = value;
                    extract.indices = {static_cast<uint32_t>(i)}; // Use index
                    extract.aggregate_type = value.type;
                    extract.result_type = field_type;

                    Value field_val = emit(extract, field_type);

                    if (field_pattern) {
                        build_pattern_binding(field_pattern, field_val);
                    } else {
                        // Shorthand: { x } binds x directly
                        set_variable(field_name, field_val);
                    }
                }
            } else if constexpr (std::is_same_v<T, hir::HirEnumPattern>) {
                // Extract payload from enum variant
                // payload is std::optional<std::vector<HirPatternPtr>>
                if (p.payload && !p.payload->empty()) {
                    for (size_t i = 0; i < p.payload->size(); ++i) {
                        // Get payload type from the payload pattern
                        const auto& payload_pattern = (*p.payload)[i];
                        MirTypePtr payload_type = make_unit_type();
                        if (payload_pattern) {
                            payload_type = convert_type(payload_pattern->type());
                        }

                        ExtractValueInst extract;
                        extract.aggregate = value;
                        // First index selects variant, second selects payload field
                        extract.indices = {static_cast<uint32_t>(p.variant_index),
                                           static_cast<uint32_t>(i)};
                        extract.aggregate_type = value.type;
                        extract.result_type = payload_type;

                        Value payload_val = emit(extract, payload_type);
                        build_pattern_binding(payload_pattern, payload_val);
                    }
                }
            } else if constexpr (std::is_same_v<T, hir::HirOrPattern>) {
                // Or pattern - bind first alternative
                // In matching context, we'd try all; in binding, just use first
                if (!p.alternatives.empty()) {
                    build_pattern_binding(p.alternatives[0], value);
                }
            } else if constexpr (std::is_same_v<T, hir::HirRangePattern>) {
                // Range pattern - nothing to bind directly
            } else if constexpr (std::is_same_v<T, hir::HirArrayPattern>) {
                // Destructure array
                size_t idx = 0;
                for (const auto& elem : p.elements) {
                    // Get element from array
                    Value index_val = const_int(static_cast<int64_t>(idx), 64, false);

                    GetElementPtrInst gep;
                    gep.base = value;
                    gep.indices = {index_val};
                    gep.base_type = value.type;

                    MirTypePtr elem_type = make_unit_type();
                    if (auto* arr_type = std::get_if<MirArrayType>(&value.type->kind)) {
                        elem_type = arr_type->element;
                    }
                    gep.result_type = make_pointer_type(elem_type, false);

                    Value ptr = emit(gep, gep.result_type);

                    LoadInst load;
                    load.ptr = ptr;
                    load.result_type = elem_type;

                    Value elem_val = emit(load, elem_type);
                    build_pattern_binding(elem, elem_val);
                    ++idx;
                }

                // Handle rest pattern if present
                if (p.rest) {
                    // Rest gets remaining elements as slice
                    // Simplified: skip for now
                }
            }
        },
        pattern->kind);
}

// ============================================================================
// Pattern Matching (for when arms)
// ============================================================================

auto HirMirBuilder::build_pattern_match(const hir::HirPatternPtr& pattern, Value scrutinee) -> Value {
    if (!pattern) {
        return const_bool(true);
    }

    return std::visit(
        [this, &scrutinee](const auto& p) -> Value {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, hir::HirWildcardPattern>) {
                // Wildcard always matches
                return const_bool(true);
            } else if constexpr (std::is_same_v<T, hir::HirBindingPattern>) {
                // Binding always matches (captures the value)
                return const_bool(true);
            } else if constexpr (std::is_same_v<T, hir::HirLiteralPattern>) {
                // Compare scrutinee with literal
                Value lit_val = std::visit(
                    [this](const auto& v) -> Value {
                        using LT = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<LT, int64_t>) {
                            return const_int(v, 64, true);
                        } else if constexpr (std::is_same_v<LT, uint64_t>) {
                            return const_int(static_cast<int64_t>(v), 64, false);
                        } else if constexpr (std::is_same_v<LT, double>) {
                            return const_float(v, true);
                        } else if constexpr (std::is_same_v<LT, bool>) {
                            return const_bool(v);
                        } else if constexpr (std::is_same_v<LT, char>) {
                            return const_int(static_cast<int64_t>(v), 32, false);
                        } else if constexpr (std::is_same_v<LT, std::string>) {
                            return const_string(v);
                        } else {
                            return const_unit();
                        }
                    },
                    p.value);

                BinaryInst cmp;
                cmp.op = BinOp::Eq;
                cmp.left = scrutinee;
                cmp.right = lit_val;
                cmp.result_type = make_bool_type();

                return emit(cmp, make_bool_type());
            } else if constexpr (std::is_same_v<T, hir::HirTuplePattern>) {
                // Match all tuple elements
                Value result = const_bool(true);

                for (size_t i = 0; i < p.elements.size(); ++i) {
                    ExtractValueInst extract;
                    extract.aggregate = scrutinee;
                    extract.indices = {static_cast<uint32_t>(i)};
                    extract.aggregate_type = scrutinee.type;

                    MirTypePtr elem_type = make_unit_type();
                    if (auto* tuple_type = std::get_if<MirTupleType>(&scrutinee.type->kind)) {
                        if (i < tuple_type->elements.size()) {
                            elem_type = tuple_type->elements[i];
                        }
                    }
                    extract.result_type = elem_type;

                    Value elem = emit(extract, elem_type);
                    Value elem_match = build_pattern_match(p.elements[i], elem);

                    // AND with previous results
                    BinaryInst and_inst;
                    and_inst.op = BinOp::And;
                    and_inst.left = result;
                    and_inst.right = elem_match;
                    and_inst.result_type = make_bool_type();

                    result = emit(and_inst, make_bool_type());
                }

                return result;
            } else if constexpr (std::is_same_v<T, hir::HirStructPattern>) {
                // Match all struct fields
                Value result = const_bool(true);

                for (size_t i = 0; i < p.fields.size(); ++i) {
                    const auto& field = p.fields[i];
                    const hir::HirPatternPtr& field_pattern = field.second;

                    // Get field type from field pattern
                    MirTypePtr field_type = make_unit_type();
                    if (field_pattern) {
                        field_type = convert_type(field_pattern->type());
                    }

                    ExtractValueInst extract;
                    extract.aggregate = scrutinee;
                    extract.indices = {static_cast<uint32_t>(i)};
                    extract.aggregate_type = scrutinee.type;
                    extract.result_type = field_type;

                    Value field_val = emit(extract, field_type);

                    Value field_match;
                    if (field_pattern) {
                        field_match = build_pattern_match(field_pattern, field_val);
                    } else {
                        field_match = const_bool(true); // Shorthand always matches
                    }

                    BinaryInst and_inst;
                    and_inst.op = BinOp::And;
                    and_inst.left = result;
                    and_inst.right = field_match;
                    and_inst.result_type = make_bool_type();

                    result = emit(and_inst, make_bool_type());
                }

                return result;
            } else if constexpr (std::is_same_v<T, hir::HirEnumPattern>) {
                // Check variant tag first
                // Extract discriminant (assume it's the first field of enum)
                ExtractValueInst extract_tag;
                extract_tag.aggregate = scrutinee;
                extract_tag.indices = {0}; // Tag is at index 0
                extract_tag.aggregate_type = scrutinee.type;
                extract_tag.result_type = make_i32_type(); // Tag is i32

                Value tag = emit(extract_tag, extract_tag.result_type);

                // Compare with expected variant index
                Value expected_tag = const_int(p.variant_index, 32, true);

                BinaryInst tag_cmp;
                tag_cmp.op = BinOp::Eq;
                tag_cmp.left = tag;
                tag_cmp.right = expected_tag;
                tag_cmp.result_type = make_bool_type();

                Value tag_match = emit(tag_cmp, make_bool_type());

                // If no payload patterns, tag match is sufficient
                if (!p.payload || p.payload->empty()) {
                    return tag_match;
                }

                // Check payload patterns
                Value result = tag_match;

                for (size_t i = 0; i < p.payload->size(); ++i) {
                    // Get payload type from payload pattern
                    const auto& payload_pattern = (*p.payload)[i];
                    MirTypePtr payload_type = make_unit_type();
                    if (payload_pattern) {
                        payload_type = convert_type(payload_pattern->type());
                    }

                    ExtractValueInst extract_payload;
                    extract_payload.aggregate = scrutinee;
                    extract_payload.indices = {static_cast<uint32_t>(p.variant_index),
                                               static_cast<uint32_t>(i)};
                    extract_payload.aggregate_type = scrutinee.type;
                    extract_payload.result_type = payload_type;

                    Value payload_val = emit(extract_payload, payload_type);
                    Value payload_match = build_pattern_match(payload_pattern, payload_val);

                    BinaryInst and_inst;
                    and_inst.op = BinOp::And;
                    and_inst.left = result;
                    and_inst.right = payload_match;
                    and_inst.result_type = make_bool_type();

                    result = emit(and_inst, make_bool_type());
                }

                return result;
            } else if constexpr (std::is_same_v<T, hir::HirOrPattern>) {
                // Match any alternative
                Value result = const_bool(false);

                for (const auto& alt : p.alternatives) {
                    Value alt_match = build_pattern_match(alt, scrutinee);

                    BinaryInst or_inst;
                    or_inst.op = BinOp::Or;
                    or_inst.left = result;
                    or_inst.right = alt_match;
                    or_inst.result_type = make_bool_type();

                    result = emit(or_inst, make_bool_type());
                }

                return result;
            } else if constexpr (std::is_same_v<T, hir::HirRangePattern>) {
                // Check if value is in range
                Value result = const_bool(true);

                if (p.start) {
                    Value start_val = const_int(*p.start, 64, true);

                    BinaryInst ge;
                    ge.op = BinOp::Ge;
                    ge.left = scrutinee;
                    ge.right = start_val;
                    ge.result_type = make_bool_type();

                    result = emit(ge, make_bool_type());
                }

                if (p.end) {
                    Value end_val = const_int(*p.end, 64, true);

                    BinOp cmp_op = p.inclusive ? BinOp::Le : BinOp::Lt;

                    BinaryInst cmp;
                    cmp.op = cmp_op;
                    cmp.left = scrutinee;
                    cmp.right = end_val;
                    cmp.result_type = make_bool_type();

                    Value end_match = emit(cmp, make_bool_type());

                    BinaryInst and_inst;
                    and_inst.op = BinOp::And;
                    and_inst.left = result;
                    and_inst.right = end_match;
                    and_inst.result_type = make_bool_type();

                    result = emit(and_inst, make_bool_type());
                }

                return result;
            } else if constexpr (std::is_same_v<T, hir::HirArrayPattern>) {
                // Match array elements
                Value result = const_bool(true);

                for (size_t i = 0; i < p.elements.size(); ++i) {
                    Value index_val = const_int(static_cast<int64_t>(i), 64, false);

                    GetElementPtrInst gep;
                    gep.base = scrutinee;
                    gep.indices = {index_val};
                    gep.base_type = scrutinee.type;

                    MirTypePtr elem_type = make_unit_type();
                    if (auto* arr_type = std::get_if<MirArrayType>(&scrutinee.type->kind)) {
                        elem_type = arr_type->element;
                    }
                    gep.result_type = make_pointer_type(elem_type, false);

                    Value ptr = emit(gep, gep.result_type);

                    LoadInst load;
                    load.ptr = ptr;
                    load.result_type = elem_type;

                    Value elem = emit(load, elem_type);
                    Value elem_match = build_pattern_match(p.elements[i], elem);

                    BinaryInst and_inst;
                    and_inst.op = BinOp::And;
                    and_inst.left = result;
                    and_inst.right = elem_match;
                    and_inst.result_type = make_bool_type();

                    result = emit(and_inst, make_bool_type());
                }

                return result;
            } else {
                return const_bool(true);
            }
        },
        pattern->kind);
}

} // namespace tml::mir
