//! # MIR Builder - Patterns
//!
//! This file implements pattern binding and destructuring.
//!
//! ## Supported Patterns
//!
//! | Pattern     | Example          | MIR Result                 |
//! |-------------|------------------|----------------------------|
//! | Identifier  | `x`              | Bind value to variable     |
//! | Tuple       | `(a, b)`         | ExtractValue for each elem |
//! | Struct      | `Point { x, y }` | ExtractValue for fields    |
//! | Enum        | `Just(v)`        | ExtractValue for payload   |
//! | Wildcard    | `_`              | Ignore value               |
//! | Literal     | `42`             | No binding                 |
//! | Or          | `A \| B`         | Use first alternative      |
//!
//! ## Nested Patterns
//!
//! Patterns are handled recursively, allowing arbitrary nesting
//! like `(a, Point { x, y: (b, c) })`.

#include "mir/mir_builder.hpp"

namespace tml::mir {

void MirBuilder::build_pattern_binding(const parser::Pattern& pattern, Value value) {
    std::visit(
        [this, &value](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                ctx_.variables[p.name] = value;
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                for (size_t i = 0; i < p.elements.size(); ++i) {
                    ExtractValueInst extract;
                    extract.aggregate = value;
                    extract.indices = {static_cast<uint32_t>(i)};

                    // Get element type from tuple
                    MirTypePtr elem_type = make_i32_type();
                    if (auto* tuple = std::get_if<MirTupleType>(&value.type->kind)) {
                        if (i < tuple->elements.size()) {
                            elem_type = tuple->elements[i];
                        }
                    }

                    auto elem = emit(std::move(extract), elem_type);
                    build_pattern_binding(*p.elements[i], elem);
                }
            } else if constexpr (std::is_same_v<T, parser::WildcardPattern>) {
                // Ignore
            } else if constexpr (std::is_same_v<T, parser::StructPattern>) {
                // Get struct type info to look up field indices
                std::string struct_name;
                if (auto* st = std::get_if<MirStructType>(&value.type->kind)) {
                    struct_name = st->name;
                }

                // Look up struct definition for field order
                std::vector<std::string> field_order;
                if (!struct_name.empty()) {
                    if (auto struct_def = env_.lookup_struct(struct_name)) {
                        for (const auto& [fname, _] : struct_def->fields) {
                            field_order.push_back(fname);
                        }
                    }
                }

                // Bind each field pattern
                for (const auto& [field_name, field_pattern] : p.fields) {
                    // Find field index
                    uint32_t field_index = 0;
                    MirTypePtr field_type = make_i32_type();
                    for (size_t i = 0; i < field_order.size(); ++i) {
                        if (field_order[i] == field_name) {
                            field_index = static_cast<uint32_t>(i);
                            if (!struct_name.empty()) {
                                if (auto struct_def = env_.lookup_struct(struct_name)) {
                                    field_type =
                                        convert_semantic_type(struct_def->fields[i].second);
                                }
                            }
                            break;
                        }
                    }

                    ExtractValueInst extract;
                    extract.aggregate = value;
                    extract.indices = {field_index};
                    extract.aggregate_type = value.type;
                    extract.result_type = field_type;

                    auto field_val = emit(std::move(extract), field_type);
                    build_pattern_binding(*field_pattern, field_val);
                }
            } else if constexpr (std::is_same_v<T, parser::EnumPattern>) {
                // For enum patterns, extract the payload if present
                if (p.payload.has_value() && !p.payload->empty()) {
                    // Payload starts at index 1 (index 0 is discriminant)
                    for (size_t i = 0; i < p.payload->size(); ++i) {
                        ExtractValueInst extract;
                        extract.aggregate = value;
                        extract.indices = {static_cast<uint32_t>(i + 1)}; // +1 to skip discriminant

                        // Get payload type from enum definition if possible
                        MirTypePtr payload_type = make_i32_type();
                        if (auto* et = std::get_if<MirEnumType>(&value.type->kind)) {
                            if (auto enum_def = env_.lookup_enum(et->name)) {
                                std::string variant_name =
                                    p.path.segments.empty() ? "" : p.path.segments.back();
                                for (const auto& [vname, vtypes] : enum_def->variants) {
                                    if (vname == variant_name && i < vtypes.size()) {
                                        payload_type = convert_semantic_type(vtypes[i]);
                                        break;
                                    }
                                }
                            }
                        }

                        auto payload_val = emit(std::move(extract), payload_type);
                        build_pattern_binding(*(*p.payload)[i], payload_val);
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::LiteralPattern>) {
                // Literal patterns don't bind any variables
            } else if constexpr (std::is_same_v<T, parser::OrPattern>) {
                // For or patterns, we only bind from the first alternative
                // (type checker ensures all alternatives bind the same names)
                if (!p.patterns.empty()) {
                    build_pattern_binding(*p.patterns[0], value);
                }
            }
        },
        pattern.kind);
}

} // namespace tml::mir
