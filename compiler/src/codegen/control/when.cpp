//! # LLVM IR Generator - When/Pattern Matching
//!
//! This file implements when expression and pattern comparison code generation.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_pattern_cmp(const parser::Pattern& pattern, const std::string& scrutinee,
                                const std::string& scrutinee_type, const std::string& tag,
                                bool is_primitive) -> std::string {
    if (pattern.is<parser::LiteralPattern>()) {
        const auto& lit_pat = pattern.as<parser::LiteralPattern>();
        std::string lit_val;

        // Get literal value based on token type
        if (lit_pat.literal.kind == lexer::TokenKind::IntLiteral) {
            // Convert to decimal for LLVM IR (handles 0x, 0b, 0o prefixes)
            lit_val = std::to_string(lit_pat.literal.int_value().value);
        } else if (lit_pat.literal.kind == lexer::TokenKind::BoolLiteral) {
            lit_val = lit_pat.literal.bool_value() ? "1" : "0";
        } else if (lit_pat.literal.kind == lexer::TokenKind::FloatLiteral) {
            lit_val = std::to_string(lit_pat.literal.float_value().value);
        } else if (lit_pat.literal.kind == lexer::TokenKind::StringLiteral) {
            // String pattern matching: use str_eq runtime function
            std::string str_val = std::string(lit_pat.literal.string_value().value);
            std::string pattern_str = add_string_literal(str_val);

            // Call str_eq(scrutinee, pattern) - returns i32 (0 or 1)
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + scrutinee + ", ptr " +
                      pattern_str + ")");

            // Convert i32 to i1 for branch condition
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp ne i32 " + eq_i32 + ", 0");
            return cmp;
        } else {
            // Unsupported literal type
            return "";
        }

        std::string cmp = fresh_reg();
        if (scrutinee_type == "float" || scrutinee_type == "double") {
            emit_line("  " + cmp + " = fcmp oeq " + scrutinee_type + " " + scrutinee + ", " +
                      lit_val);
        } else {
            emit_line("  " + cmp + " = icmp eq " + scrutinee_type + " " + scrutinee + ", " +
                      lit_val);
        }
        return cmp;
    } else if (pattern.is<parser::EnumPattern>()) {
        const auto& enum_pat = pattern.as<parser::EnumPattern>();
        std::string variant_name;
        if (enum_pat.path.segments.size() >= 1) {
            variant_name = enum_pat.path.segments.back();
        }

        // Find variant tag
        int variant_tag = -1;
        std::string scrutinee_enum_name;
        if (scrutinee_type.starts_with("%struct.")) {
            scrutinee_enum_name = scrutinee_type.substr(8);
        }

        if (!scrutinee_enum_name.empty()) {
            std::string key = scrutinee_enum_name + "::" + variant_name;
            auto it = enum_variants_.find(key);
            if (it != enum_variants_.end()) {
                variant_tag = it->second;
            }
        }

        // Fallback: Try full path
        if (variant_tag < 0) {
            std::string full_path;
            for (size_t i = 0; i < enum_pat.path.segments.size(); ++i) {
                if (i > 0)
                    full_path += "::";
                full_path += enum_pat.path.segments[i];
            }
            auto it = enum_variants_.find(full_path);
            if (it != enum_variants_.end()) {
                variant_tag = it->second;
            }
        }

        if (variant_tag >= 0) {
            std::string cmp = fresh_reg();
            // For unit-only enums (represented as just i32), scrutinee IS the tag
            // If tag is empty, use scrutinee directly
            std::string tag_value = tag.empty() ? scrutinee : tag;
            // Use appropriate type: i32 for normal enums, scrutinee_type for primitive
            // representations
            std::string cmp_type = tag.empty() ? scrutinee_type : "i32";
            emit_line("  " + cmp + " = icmp eq " + cmp_type + " " + tag_value + ", " +
                      std::to_string(variant_tag));
            return cmp;
        }
        return "";
    } else if (pattern.is<parser::IdentPattern>()) {
        const auto& ident_pat = pattern.as<parser::IdentPattern>();

        // For primitives, IdentPattern is a binding (always matches)
        if (is_primitive) {
            return "";
        }

        // For enums, check if it's a unit variant
        int variant_tag = -1;
        std::string scrutinee_enum_name;
        if (scrutinee_type.starts_with("%struct.")) {
            scrutinee_enum_name = scrutinee_type.substr(8);
        }

        if (!scrutinee_enum_name.empty()) {
            std::string key = scrutinee_enum_name + "::" + ident_pat.name;
            auto it = enum_variants_.find(key);
            if (it != enum_variants_.end()) {
                variant_tag = it->second;
            }
        }

        if (variant_tag >= 0) {
            std::string cmp = fresh_reg();
            // For unit-only enums (represented as just i32), scrutinee IS the tag
            // If tag is empty, use scrutinee directly
            std::string tag_value = tag.empty() ? scrutinee : tag;
            // Use appropriate type: i32 for normal enums, scrutinee_type for primitive
            // representations
            std::string cmp_type = tag.empty() ? scrutinee_type : "i32";
            emit_line("  " + cmp + " = icmp eq " + cmp_type + " " + tag_value + ", " +
                      std::to_string(variant_tag));
            return cmp;
        }
        return ""; // Binding pattern - always matches
    } else if (pattern.is<parser::WildcardPattern>()) {
        return ""; // Always matches
    } else if (pattern.is<parser::RangePattern>()) {
        const auto& range_pat = pattern.as<parser::RangePattern>();

        // Generate comparisons for range bounds
        // Range pattern: start to end (exclusive) or start through end (inclusive)
        std::string cmp_start;
        std::string cmp_end;

        // Generate start comparison: scrutinee >= start
        if (range_pat.start.has_value()) {
            std::string start_val = gen_expr(*range_pat.start.value());
            std::string start_type = last_expr_type_;

            // Ensure types match
            if (start_type != scrutinee_type && !start_type.empty()) {
                // Try to convert if needed
                if ((scrutinee_type == "i64" && start_type == "i32") ||
                    (scrutinee_type == "i32" && start_type == "i64")) {
                    std::string conv = fresh_reg();
                    if (scrutinee_type == "i64" && start_type == "i32") {
                        emit_line("  " + conv + " = sext i32 " + start_val + " to i64");
                    } else {
                        emit_line("  " + conv + " = trunc i64 " + start_val + " to i32");
                    }
                    start_val = conv;
                }
            }

            cmp_start = fresh_reg();
            if (scrutinee_type == "float" || scrutinee_type == "double") {
                emit_line("  " + cmp_start + " = fcmp oge " + scrutinee_type + " " + scrutinee +
                          ", " + start_val);
            } else {
                emit_line("  " + cmp_start + " = icmp sge " + scrutinee_type + " " + scrutinee +
                          ", " + start_val);
            }
        }

        // Generate end comparison: scrutinee < end (exclusive) or scrutinee <= end (inclusive)
        if (range_pat.end.has_value()) {
            std::string end_val = gen_expr(*range_pat.end.value());
            std::string end_type = last_expr_type_;

            // Ensure types match
            if (end_type != scrutinee_type && !end_type.empty()) {
                if ((scrutinee_type == "i64" && end_type == "i32") ||
                    (scrutinee_type == "i32" && end_type == "i64")) {
                    std::string conv = fresh_reg();
                    if (scrutinee_type == "i64" && end_type == "i32") {
                        emit_line("  " + conv + " = sext i32 " + end_val + " to i64");
                    } else {
                        emit_line("  " + conv + " = trunc i64 " + end_val + " to i32");
                    }
                    end_val = conv;
                }
            }

            cmp_end = fresh_reg();
            if (scrutinee_type == "float" || scrutinee_type == "double") {
                if (range_pat.inclusive) {
                    emit_line("  " + cmp_end + " = fcmp ole " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                } else {
                    emit_line("  " + cmp_end + " = fcmp olt " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                }
            } else {
                if (range_pat.inclusive) {
                    emit_line("  " + cmp_end + " = icmp sle " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                } else {
                    emit_line("  " + cmp_end + " = icmp slt " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                }
            }
        }

        // Combine comparisons
        if (!cmp_start.empty() && !cmp_end.empty()) {
            std::string combined = fresh_reg();
            emit_line("  " + combined + " = and i1 " + cmp_start + ", " + cmp_end);
            return combined;
        } else if (!cmp_start.empty()) {
            return cmp_start; // Only lower bound
        } else if (!cmp_end.empty()) {
            return cmp_end; // Only upper bound
        }
        return ""; // Open range - always matches
    }
    return ""; // Default: always matches
}

auto LLVMIRGen::gen_when(const parser::WhenExpr& when) -> std::string {
    // Evaluate scrutinee
    std::string scrutinee = gen_expr(*when.scrutinee);
    std::string scrutinee_type = last_expr_type_;

    // Check if scrutinee is a string (Str type) by examining semantic type
    bool is_string_scrutinee = false;
    types::TypePtr scrutinee_semantic = infer_expr_type(*when.scrutinee);
    if (scrutinee_semantic && scrutinee_semantic->is<types::PrimitiveType>()) {
        const auto& prim = scrutinee_semantic->as<types::PrimitiveType>();
        is_string_scrutinee = (prim.kind == types::PrimitiveKind::Str);
    }

    // If scrutinee_type is ptr, infer the actual struct type from the expression
    // Exception: strings remain as ptr for string pattern matching
    std::string scrutinee_ptr;
    if (scrutinee_type == "ptr") {
        if (!is_string_scrutinee && scrutinee_semantic) {
            scrutinee_type = llvm_type_from_semantic(scrutinee_semantic);
        }
        // scrutinee is already a pointer, use it directly
        scrutinee_ptr = scrutinee;
    } else {
        // Allocate space for scrutinee and store the value
        scrutinee_ptr = fresh_reg();
        emit_line("  " + scrutinee_ptr + " = alloca " + scrutinee_type);
        emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + scrutinee_ptr);
    }

    // Check if scrutinee is a simple primitive type (not enum/struct)
    // Strings are also treated as primitives for pattern matching (compared with str_eq)
    bool is_primitive_scrutinee =
        is_string_scrutinee ||
        (scrutinee_type == "i8" || scrutinee_type == "i16" || scrutinee_type == "i32" ||
         scrutinee_type == "i64" || scrutinee_type == "i128" || scrutinee_type == "float" ||
         scrutinee_type == "double" || scrutinee_type == "i1");

    // For enums/structs, extract tag; for primitives (including strings), we'll compare directly
    std::string tag;
    if (!is_primitive_scrutinee) {
        // Extract tag (assumes enum is { i32, i64 })
        std::string tag_ptr = fresh_reg();
        emit_line("  " + tag_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " +
                  scrutinee_ptr + ", i32 0, i32 0");
        tag = fresh_reg();
        emit_line("  " + tag + " = load i32, ptr " + tag_ptr);
    }

    // Generate labels for each arm + end
    std::vector<std::string> arm_labels;
    for (size_t i = 0; i < when.arms.size(); ++i) {
        arm_labels.push_back(fresh_label("when_arm"));
    }
    std::string label_end = fresh_label("when_end");

    // Allocate temporary for result - will track actual type during arm processing
    std::string result_ptr = fresh_reg();
    std::string result_type = "i32"; // Default, will be updated
    bool result_type_set = false;
    bool all_arms_terminate = true;
    emit_line("  " + result_ptr + " = alloca i64"); // Use i64 to hold most types

    // scrutinee_semantic already computed above for string detection

    // Generate switch based on pattern
    // For now, simplified: each arm is checked sequentially
    for (size_t arm_idx = 0; arm_idx < when.arms.size(); ++arm_idx) {
        const auto& arm = when.arms[arm_idx];
        std::string next_label =
            (arm_idx + 1 < when.arms.size()) ? fresh_label("when_next") : label_end;

        // Check if pattern matches
        // Handle OrPattern: generate comparisons for each sub-pattern and OR them together
        if (arm.pattern->is<parser::OrPattern>()) {
            const auto& or_pat = arm.pattern->as<parser::OrPattern>();
            std::vector<std::string> cmp_results;

            for (const auto& sub_pattern : or_pat.patterns) {
                std::string cmp = gen_pattern_cmp(*sub_pattern, scrutinee, scrutinee_type, tag,
                                                  is_primitive_scrutinee);
                if (!cmp.empty()) {
                    cmp_results.push_back(cmp);
                }
            }

            if (cmp_results.empty()) {
                // All patterns always match (all wildcards)
                emit_line("  br label %" + arm_labels[arm_idx]);
            } else if (cmp_results.size() == 1) {
                emit_line("  br i1 " + cmp_results[0] + ", label %" + arm_labels[arm_idx] +
                          ", label %" + next_label);
            } else {
                // Combine with OR
                std::string combined = cmp_results[0];
                for (size_t i = 1; i < cmp_results.size(); ++i) {
                    std::string new_combined = fresh_reg();
                    emit_line("  " + new_combined + " = or i1 " + combined + ", " + cmp_results[i]);
                    combined = new_combined;
                }
                emit_line("  br i1 " + combined + ", label %" + arm_labels[arm_idx] + ", label %" +
                          next_label);
            }
        } else {
            // Single pattern
            std::string cmp = gen_pattern_cmp(*arm.pattern, scrutinee, scrutinee_type, tag,
                                              is_primitive_scrutinee);
            if (cmp.empty()) {
                // Pattern always matches (wildcard, binding, etc.)
                emit_line("  br label %" + arm_labels[arm_idx]);
            } else {
                emit_line("  br i1 " + cmp + ", label %" + arm_labels[arm_idx] + ", label %" +
                          next_label);
            }
        }

        // Generate arm body
        emit_line(arm_labels[arm_idx] + ":");
        block_terminated_ = false;

        // Push a drop scope for arm-local bindings (e.g., destructured guards)
        push_drop_scope();
        // Save current locals so we can clean up arm-scoped bindings later
        auto saved_locals = locals_;

        // Bind pattern variables
        if (arm.pattern->is<parser::EnumPattern>()) {
            const auto& enum_pat = arm.pattern->as<parser::EnumPattern>();

            if (enum_pat.payload.has_value() && !enum_pat.payload->empty()) {
                // Extract payload pointer (points to the data bytes of the enum)
                std::string payload_ptr = fresh_reg();
                emit_line("  " + payload_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + scrutinee_ptr + ", i32 0, i32 1");

                // Get the semantic type of the scrutinee to find the payload type
                // (scrutinee_semantic is already computed at the start of gen_when)

                // Check if the payload pattern is a TuplePattern
                if (enum_pat.payload->at(0)->is<parser::TuplePattern>()) {
                    const auto& tuple_pat = enum_pat.payload->at(0)->as<parser::TuplePattern>();

                    // Get the tuple type from the enum's type arguments
                    // For Outcome[T, E], Ok's payload is T
                    // For Maybe[T], Just's payload is T
                    types::TypePtr payload_type = nullptr;
                    if (scrutinee_semantic && scrutinee_semantic->is<types::NamedType>()) {
                        const auto& named = scrutinee_semantic->as<types::NamedType>();
                        std::string variant_name;
                        if (!enum_pat.path.segments.empty()) {
                            variant_name = enum_pat.path.segments.back();
                        }

                        // For Outcome: Ok uses type_args[0] (T), Err uses type_args[1] (E)
                        if (named.name == "Outcome" && named.type_args.size() >= 2) {
                            if (variant_name == "Ok") {
                                payload_type = named.type_args[0];
                            } else if (variant_name == "Err") {
                                payload_type = named.type_args[1];
                            }
                        }
                        // For Maybe: Just uses type_args[0] (T)
                        else if (named.name == "Maybe" && !named.type_args.empty()) {
                            if (variant_name == "Just") {
                                payload_type = named.type_args[0];
                            }
                        } else {
                            // Look up the enum definition to get the payload type
                            auto enum_def = env_.lookup_enum(named.name);
                            if (enum_def.has_value()) {
                                for (const auto& [var_name, var_payloads] : enum_def->variants) {
                                    if (var_name == variant_name && !var_payloads.empty()) {
                                        payload_type = var_payloads[0];
                                        break;
                                    }
                                }
                            }
                            // For generic enums, substitute type parameters
                            if (payload_type && !named.type_args.empty()) {
                                std::unordered_map<std::string, types::TypePtr> enum_type_subs;
                                auto enum_def2 = env_.lookup_enum(named.name);
                                if (enum_def2 && !enum_def2->type_params.empty()) {
                                    for (size_t i = 0; i < enum_def2->type_params.size() &&
                                                       i < named.type_args.size();
                                         ++i) {
                                        enum_type_subs[enum_def2->type_params[i]] =
                                            named.type_args[i];
                                    }
                                }
                                if (!enum_type_subs.empty()) {
                                    payload_type =
                                        types::substitute_type(payload_type, enum_type_subs);
                                }
                            }
                        }
                    }

                    // The payload is a tuple stored as an anonymous struct
                    // Get the element types from the payload_type if it's a TupleType
                    std::vector<types::TypePtr> element_types;
                    if (payload_type && payload_type->is<types::TupleType>()) {
                        element_types = payload_type->as<types::TupleType>().elements;
                    }

                    // Get the LLVM type of the tuple for proper GEP
                    std::string tuple_llvm_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "{ i64, i64 }";

                    // For each element in the tuple pattern, extract and bind
                    for (size_t i = 0; i < tuple_pat.elements.size(); ++i) {
                        const auto& elem_pat = tuple_pat.elements[i];

                        if (elem_pat->is<parser::IdentPattern>()) {
                            const auto& ident = elem_pat->as<parser::IdentPattern>();

                            // Skip wildcard patterns like _stride
                            if (ident.name.empty() || ident.name[0] == '_') {
                                continue;
                            }

                            // Get the element type (from inference or default to i64)
                            std::string elem_type = "i64";
                            types::TypePtr elem_semantic_type = nullptr;
                            if (i < element_types.size()) {
                                elem_semantic_type = element_types[i];
                                elem_type = llvm_type_from_semantic(elem_semantic_type, true);
                            }

                            // Extract the i-th element from the tuple
                            // The tuple is stored at payload_ptr as tuple_llvm_type
                            std::string elem_ptr = fresh_reg();
                            emit_line("  " + elem_ptr + " = getelementptr inbounds " +
                                      tuple_llvm_type + ", ptr " + payload_ptr + ", i32 0, i32 " +
                                      std::to_string(i));

                            // For struct types, we just use the pointer directly
                            // For primitives, we load the value
                            if (elem_type.starts_with("%struct.") || elem_type.starts_with("{")) {
                                // Struct/tuple type - variable is the pointer
                                locals_[ident.name] =
                                    VarInfo{elem_ptr, elem_type, elem_semantic_type, std::nullopt};
                            } else {
                                // Primitive type - load and store
                                std::string elem_val = fresh_reg();
                                emit_line("  " + elem_val + " = load " + elem_type + ", ptr " +
                                          elem_ptr);

                                std::string var_alloca = fresh_reg();
                                emit_line("  " + var_alloca + " = alloca " + elem_type);
                                emit_line("  store " + elem_type + " " + elem_val + ", ptr " +
                                          var_alloca);
                                locals_[ident.name] = VarInfo{var_alloca, elem_type,
                                                              elem_semantic_type, std::nullopt};
                            }
                        } else if (elem_pat->is<parser::WildcardPattern>()) {
                            // Wildcard _ - skip binding
                            continue;
                        }
                    }
                } else if (enum_pat.payload->at(0)->is<parser::IdentPattern>()) {
                    // Simple ident pattern - original behavior
                    const auto& ident = enum_pat.payload->at(0)->as<parser::IdentPattern>();

                    // Get payload type from enum type args
                    types::TypePtr payload_type = nullptr;
                    if (scrutinee_semantic && scrutinee_semantic->is<types::NamedType>()) {
                        const auto& named = scrutinee_semantic->as<types::NamedType>();
                        std::string variant_name;
                        if (!enum_pat.path.segments.empty()) {
                            variant_name = enum_pat.path.segments.back();
                        }

                        if (named.name == "Outcome" && named.type_args.size() >= 2) {
                            if (variant_name == "Ok") {
                                payload_type = named.type_args[0];
                            } else if (variant_name == "Err") {
                                payload_type = named.type_args[1];
                            }
                        } else if (named.name == "Maybe" && !named.type_args.empty()) {
                            if (variant_name == "Just") {
                                payload_type = named.type_args[0];
                            }
                        } else {
                            // Look up the enum definition to get the payload type
                            auto enum_def = env_.lookup_enum(named.name);
                            if (enum_def.has_value()) {
                                // Find the variant and get its payload type
                                for (const auto& [var_name, var_payloads] : enum_def->variants) {
                                    if (var_name == variant_name && !var_payloads.empty()) {
                                        payload_type = var_payloads[0];
                                        break;
                                    }
                                }
                            }
                            // For generic enums, the payload type may be a type parameter (T)
                            // Use type args from the scrutinee type to substitute
                            if (payload_type && !named.type_args.empty()) {
                                // Build type subs from enum type params -> type args
                                std::unordered_map<std::string, types::TypePtr> enum_type_subs;
                                auto enum_def2 = env_.lookup_enum(named.name);
                                if (enum_def2 && !enum_def2->type_params.empty()) {
                                    for (size_t i = 0; i < enum_def2->type_params.size() &&
                                                       i < named.type_args.size();
                                         ++i) {
                                        enum_type_subs[enum_def2->type_params[i]] =
                                            named.type_args[i];
                                    }
                                }
                                // Apply substitutions
                                if (!enum_type_subs.empty()) {
                                    payload_type =
                                        types::substitute_type(payload_type, enum_type_subs);
                                }
                            }
                        }
                    }

                    std::string bound_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "i64";

                    // For struct/tuple types, the variable is a pointer to the payload
                    if (bound_type.starts_with("%struct.") || bound_type.starts_with("{")) {
                        locals_[ident.name] =
                            VarInfo{payload_ptr, bound_type, payload_type, std::nullopt};
                    } else {
                        // For primitives, load from payload
                        std::string payload = fresh_reg();
                        emit_line("  " + payload + " = load " + bound_type + ", ptr " +
                                  payload_ptr);

                        std::string var_alloca = fresh_reg();
                        emit_line("  " + var_alloca + " = alloca " + bound_type);
                        emit_line("  store " + bound_type + " " + payload + ", ptr " + var_alloca);
                        locals_[ident.name] =
                            VarInfo{var_alloca, bound_type, payload_type, std::nullopt};
                    }
                }
            }
        }
        // Bind struct pattern variables
        else if (arm.pattern->is<parser::StructPattern>()) {
            const auto& struct_pat = arm.pattern->as<parser::StructPattern>();

            // Get the struct type name from the pattern path
            std::string struct_name;
            if (!struct_pat.path.segments.empty()) {
                struct_name = struct_pat.path.segments.back();
            }

            // Get semantic type info for field types
            // (scrutinee_semantic is already computed at the start of gen_when)

            // Look up struct field info from struct_fields_
            auto struct_it = struct_fields_.find(struct_name);

            for (size_t i = 0; i < struct_pat.fields.size(); ++i) {
                const auto& [field_name, field_pattern] = struct_pat.fields[i];

                // Only handle ident patterns for now
                if (!field_pattern->is<parser::IdentPattern>()) {
                    continue;
                }

                const auto& ident = field_pattern->as<parser::IdentPattern>();
                if (ident.name.empty() || ident.name == "_") {
                    continue;
                }

                // Find field index in struct
                int field_idx = -1;
                std::string field_type = "i64"; // Default
                if (struct_it != struct_fields_.end()) {
                    const auto& fields = struct_it->second;
                    for (size_t fi = 0; fi < fields.size(); ++fi) {
                        if (fields[fi].name == field_name) {
                            field_idx = fields[fi].index;
                            field_type = fields[fi].llvm_type;
                            break;
                        }
                    }
                }

                if (field_idx < 0) {
                    // Field not found in struct_fields_, try to use sequential index
                    field_idx = static_cast<int>(i);
                }

                // Extract field pointer from scrutinee
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + scrutinee_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                // For struct/complex types, store pointer directly
                if (field_type.starts_with("%struct.") || field_type.starts_with("{")) {
                    locals_[ident.name] = VarInfo{field_ptr, field_type, nullptr, std::nullopt};
                } else {
                    // For primitives, load and store
                    std::string field_val = fresh_reg();
                    emit_line("  " + field_val + " = load " + field_type + ", ptr " + field_ptr);

                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + field_type);
                    emit_line("  store " + field_type + " " + field_val + ", ptr " + var_alloca);
                    locals_[ident.name] = VarInfo{var_alloca, field_type, nullptr, std::nullopt};
                }
            }
        }
        // Bind tuple pattern variables
        else if (arm.pattern->is<parser::TuplePattern>()) {
            const auto& tuple_pat = arm.pattern->as<parser::TuplePattern>();

            // Use the existing helper function for tuple pattern binding
            // (scrutinee_semantic is already computed at the start of gen_when)
            gen_tuple_pattern_binding(tuple_pat, scrutinee, scrutinee_type, scrutinee_semantic);
        }
        // Bind array pattern variables: [a, b, c] or [head, ..rest]
        else if (arm.pattern->is<parser::ArrayPattern>()) {
            const auto& array_pat = arm.pattern->as<parser::ArrayPattern>();

            // (scrutinee_semantic is already computed at the start of gen_when)

            // Parse element type from the array type string (e.g., "[5 x i32]" -> "i32")
            std::string elem_type = "i32"; // Default
            size_t x_pos = scrutinee_type.find(" x ");
            if (x_pos != std::string::npos) {
                size_t end_pos = scrutinee_type.rfind(']');
                if (end_pos != std::string::npos && end_pos > x_pos + 3) {
                    elem_type = scrutinee_type.substr(x_pos + 3, end_pos - x_pos - 3);
                }
            }

            // Get semantic element type if available
            types::TypePtr semantic_elem = nullptr;
            if (scrutinee_semantic && scrutinee_semantic->is<types::ArrayType>()) {
                const auto& arr = scrutinee_semantic->as<types::ArrayType>();
                semantic_elem = arr.element;
            }

            // Store the array value to a temporary so we can GEP into it
            std::string array_ptr = fresh_reg();
            emit_line("  " + array_ptr + " = alloca " + scrutinee_type);
            emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + array_ptr);

            // Bind each element pattern
            for (size_t i = 0; i < array_pat.elements.size(); ++i) {
                const auto& elem_pattern = *array_pat.elements[i];

                // Get pointer to element
                std::string elem_ptr = fresh_reg();
                emit_line("  " + elem_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + array_ptr + ", i32 0, i32 " + std::to_string(i));

                // Load the element
                std::string elem_val = fresh_reg();
                emit_line("  " + elem_val + " = load " + elem_type + ", ptr " + elem_ptr);

                // Bind based on pattern type
                if (elem_pattern.is<parser::IdentPattern>()) {
                    const auto& ident = elem_pattern.as<parser::IdentPattern>();
                    if (!ident.name.empty() && ident.name != "_") {
                        std::string alloca_reg = fresh_reg();
                        emit_line("  " + alloca_reg + " = alloca " + elem_type);
                        emit_line("  store " + elem_type + " " + elem_val + ", ptr " + alloca_reg);
                        locals_[ident.name] =
                            VarInfo{alloca_reg, elem_type, semantic_elem, std::nullopt};
                    }
                } else if (elem_pattern.is<parser::WildcardPattern>()) {
                    // Ignore the value
                }
                // Note: Nested patterns could be added here recursively
            }

            // Handle rest pattern if present (e.g., [a, b, ..rest])
            // Rest pattern binds to the remaining elements as a slice/array
            if (array_pat.rest) {
                const auto& rest_pattern = *array_pat.rest;
                if (rest_pattern->is<parser::IdentPattern>()) {
                    const auto& rest_ident = rest_pattern->as<parser::IdentPattern>();
                    if (!rest_ident.name.empty() && rest_ident.name != "_") {
                        // Calculate remaining elements pointer
                        size_t rest_start = array_pat.elements.size();
                        std::string rest_ptr = fresh_reg();
                        emit_line("  " + rest_ptr + " = getelementptr inbounds " + scrutinee_type +
                                  ", ptr " + array_ptr + ", i32 0, i32 " +
                                  std::to_string(rest_start));
                        // Bind as a pointer to the rest of the array
                        locals_[rest_ident.name] =
                            VarInfo{rest_ptr, "ptr", scrutinee_semantic, std::nullopt};
                    }
                }
            }
        }
        // Bind ident pattern (simple variable binding to scrutinee)
        else if (arm.pattern->is<parser::IdentPattern>()) {
            const auto& ident = arm.pattern->as<parser::IdentPattern>();
            if (!ident.name.empty() && ident.name != "_") {
                // Bind the entire scrutinee to the variable
                if (scrutinee_type.starts_with("%struct.") || scrutinee_type.starts_with("{")) {
                    locals_[ident.name] =
                        VarInfo{scrutinee_ptr, scrutinee_type, nullptr, std::nullopt};
                } else {
                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + scrutinee_type);
                    emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " +
                              var_alloca);
                    locals_[ident.name] =
                        VarInfo{var_alloca, scrutinee_type, nullptr, std::nullopt};
                }
            }
        }

        // Register arm-bound variables for drop (e.g., MutexGuard from Just(guard))
        for (const auto& [name, info] : locals_) {
            if (saved_locals.find(name) == saved_locals.end()) {
                // This variable was bound in this arm
                std::string type_name;
                if (info.type.starts_with("%struct.")) {
                    type_name = info.type.substr(8); // Strip "%struct."
                }
                if (!type_name.empty()) {
                    register_for_drop(name, info.reg, type_name, info.type);
                }
            }
        }

        // Execute arm body
        std::string arm_value = gen_expr(*arm.body);
        std::string arm_type = last_expr_type_;

        // Track if this arm terminates (return/break/continue)
        // and update result_type from the first non-terminating arm
        if (!block_terminated_) {
            all_arms_terminate = false;
            if (!result_type_set && arm_type != "void") {
                result_type = arm_type;
                result_type_set = true;
            }
        }

        // Store arm value to result (with type conversion if needed)
        // Don't store void or Unit types - they don't produce storable values
        if (!block_terminated_ && arm_type != "void" && arm_type != "{}") {
            std::string store_value = arm_value;
            std::string store_type = arm_type;

            // Convert i1 to i32 for storage compatibility
            if (arm_type == "i1") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = zext i1 " + arm_value + " to i32");
                store_value = converted;
                store_type = "i32";
            }

            emit_line("  store " + store_type + " " + store_value + ", ptr " + result_ptr);
            // Drop arm-scoped variables before leaving the arm
            emit_scope_drops();
            emit_line("  br label %" + label_end);
        } else if (!block_terminated_) {
            // Void arm - just branch to end
            // Drop arm-scoped variables before leaving the arm
            emit_scope_drops();
            emit_line("  br label %" + label_end);
        }

        // Pop drop scope and restore locals (arm bindings are out of scope)
        pop_drop_scope();
        locals_ = saved_locals;

        // Next check label (if not last arm)
        if (arm_idx + 1 < when.arms.size()) {
            emit_line(next_label + ":");
            block_terminated_ = false;
        }
    }

    // End label
    emit_line(label_end + ":");
    current_block_ = label_end;
    block_terminated_ = false;

    // If all arms terminate (return/break/continue), the when_end is unreachable
    if (all_arms_terminate) {
        emit_line("  unreachable");
        block_terminated_ = true;
        last_expr_type_ = "void";
        return "0";
    }

    // If result type is void or Unit, don't load anything
    if (result_type == "void" || result_type == "{}") {
        last_expr_type_ = result_type;
        return "0";
    }

    // Load result (and convert back if needed)
    std::string result = fresh_reg();
    if (result_type == "i1") {
        // i1 was stored as i32, load as i32 and convert back
        std::string loaded_i32 = fresh_reg();
        emit_line("  " + loaded_i32 + " = load i32, ptr " + result_ptr);
        emit_line("  " + result + " = trunc i32 " + loaded_i32 + " to i1");
    } else {
        emit_line("  " + result + " = load " + result_type + ", ptr " + result_ptr);
    }
    last_expr_type_ = result_type;
    return result;
}

} // namespace tml::codegen
