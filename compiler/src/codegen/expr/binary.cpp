//! # LLVM IR Generator - Binary Expressions
//!
//! This file implements binary operator code generation.
//!
//! ## Operator Categories
//!
//! | Category    | Operators                    | LLVM Instructions   |
//! |-------------|------------------------------|---------------------|
//! | Arithmetic  | `+` `-` `*` `/` `%`          | add, sub, mul, div  |
//! | Comparison  | `==` `!=` `<` `>` `<=` `>=`  | icmp, fcmp          |
//! | Logical     | `and` `or`                   | and, or (short-circuit)|
//! | Bitwise     | `&` `\|` `^` `<<` `>>`        | and, or, xor, shl, shr|
//! | Assignment  | `=`                          | store               |
//!
//! ## Type Handling
//!
//! - Integer operations use `add`, `sub`, `mul`, `sdiv`/`udiv`
//! - Float operations use `fadd`, `fsub`, `fmul`, `fdiv`
//! - Comparisons use `icmp`/`fcmp` with appropriate predicates
//!
//! ## Assignment
//!
//! Assignment to identifiers uses `store` instruction.
//! Compound assignments (+=, -=, etc.) are lowered to load-op-store.

#include "codegen/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"

#include <functional>

namespace tml::codegen {

auto LLVMIRGen::gen_binary(const parser::BinaryExpr& bin) -> std::string {
    // Handle assignment specially - don't evaluate left for deref assignments
    if (bin.op == parser::BinaryOp::Assign) {
        // For field assignments, we need to set expected_enum_type_ BEFORE evaluating RHS
        // This is needed for generic enum unit variants like 'Nothing' to get the correct type
        std::string saved_expected_enum_type = expected_enum_type_;

        if (bin.left->is<parser::FieldExpr>()) {
            // Get the field type to use as expected enum type for RHS
            types::TypePtr lhs_type = infer_expr_type(*bin.left);
            if (lhs_type) {
                std::string llvm_type = llvm_type_from_semantic(lhs_type);
                if (llvm_type.starts_with("%struct.")) {
                    expected_enum_type_ = llvm_type;
                }
            }
        }

        std::string right = gen_expr(*bin.right);

        // Restore expected_enum_type_
        expected_enum_type_ = saved_expected_enum_type;

        if (bin.left->is<parser::IdentExpr>()) {
            auto it = locals_.find(bin.left->as<parser::IdentExpr>().name);
            if (it != locals_.end()) {
                // Check if this is a mutable reference - if so, we need to store THROUGH the
                // reference
                bool is_mut_ref = false;
                std::string inner_llvm_type = it->second.type;

                if (it->second.semantic_type && it->second.semantic_type->is<types::RefType>() &&
                    it->second.semantic_type->as<types::RefType>().is_mut) {
                    is_mut_ref = true;
                    // Get the inner type for store
                    const auto& ref_type = it->second.semantic_type->as<types::RefType>();
                    if (ref_type.inner) {
                        inner_llvm_type = llvm_type_from_semantic(ref_type.inner);
                    }
                }

                if (is_mut_ref) {
                    // Assignment through mutable reference:
                    // 1. Load the pointer from the alloca
                    // 2. Store the value through that pointer
                    std::string ptr_reg = fresh_reg();
                    emit_line("  " + ptr_reg + " = load ptr, ptr " + it->second.reg);
                    emit_line("  store " + inner_llvm_type + " " + right + ", ptr " + ptr_reg);
                } else {
                    // Handle integer truncation if needed (e.g., i32 result to i8 variable)
                    std::string value_to_store = right;
                    std::string right_type = last_expr_type_;
                    std::string target_type = it->second.type;

                    // Helper to get integer size
                    auto get_int_size = [](const std::string& t) -> int {
                        if (t == "i8")
                            return 8;
                        if (t == "i16")
                            return 16;
                        if (t == "i32")
                            return 32;
                        if (t == "i64")
                            return 64;
                        if (t == "i128")
                            return 128;
                        return 0;
                    };

                    if (right_type != target_type) {
                        int right_size = get_int_size(right_type);
                        int target_size = get_int_size(target_type);

                        if (right_size > 0 && target_size > 0 && right_size > target_size) {
                            // Truncate to smaller type
                            std::string trunc_reg = fresh_reg();
                            emit_line("  " + trunc_reg + " = trunc " + right_type + " " + right +
                                      " to " + target_type);
                            value_to_store = trunc_reg;
                        }
                    }

                    emit_line("  store " + target_type + " " + value_to_store + ", ptr " +
                              it->second.reg);
                }
            }
        } else if (bin.left->is<parser::UnaryExpr>()) {
            const auto& unary = bin.left->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Deref) {
                // Dereferenced pointer assignment: *ptr = value
                // Get the pointer (not the dereferenced value!)
                std::string ptr = gen_expr(*unary.operand);

                // Infer the inner type from the operand's type
                types::TypePtr operand_type = infer_expr_type(*unary.operand);
                std::string inner_llvm_type = "i32"; // default

                if (operand_type) {
                    if (std::holds_alternative<types::RefType>(operand_type->kind)) {
                        const auto& ref_type = std::get<types::RefType>(operand_type->kind);
                        if (ref_type.inner) {
                            inner_llvm_type = llvm_type_from_semantic(ref_type.inner);
                        }
                    } else if (std::holds_alternative<types::PtrType>(operand_type->kind)) {
                        const auto& ptr_type = std::get<types::PtrType>(operand_type->kind);
                        if (ptr_type.inner) {
                            inner_llvm_type = llvm_type_from_semantic(ptr_type.inner);
                        }
                    }
                }

                emit_line("  store " + inner_llvm_type + " " + right + ", ptr " + ptr);
            }
        } else if (bin.left->is<parser::FieldExpr>()) {
            // Field assignment: obj.field = value or this.field = value or ClassName.static = value
            const auto& field = bin.left->as<parser::FieldExpr>();

            // Check for static field assignment first
            if (field.object->is<parser::IdentExpr>()) {
                const auto& ident = field.object->as<parser::IdentExpr>();
                std::string static_key = ident.name + "." + field.field;
                auto static_it = static_fields_.find(static_key);
                if (static_it != static_fields_.end()) {
                    // Store to global static field
                    emit_line("  store " + static_it->second.type + " " + right + ", ptr " +
                              static_it->second.global_name);
                    return right; // Return the assigned value
                }
            }

            std::string struct_type;
            std::string struct_ptr;

            if (field.object->is<parser::IdentExpr>()) {
                const auto& ident = field.object->as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    struct_type = it->second.type;
                    struct_ptr = it->second.reg;

                    // Special handling for 'this' in impl methods
                    if (ident.name == "this" && !current_impl_type_.empty()) {
                        struct_type = "%struct." + current_impl_type_;
                        // 'this' is already a pointer parameter, not an alloca - use it directly
                        // struct_ptr is already "%this" which is the direct pointer
                    }
                }
            }

            if (!struct_type.empty() && !struct_ptr.empty()) {
                // If struct_type is ptr (e.g., for mut ref parameters), resolve the inner type
                // and load the pointer from the alloca first
                if (struct_type == "ptr") {
                    types::TypePtr semantic_type = infer_expr_type(*field.object);
                    if (semantic_type) {
                        if (semantic_type->is<types::RefType>()) {
                            const auto& ref = semantic_type->as<types::RefType>();
                            struct_type = llvm_type_from_semantic(ref.inner);
                            // struct_ptr points to an alloca containing a pointer to the struct
                            // We need to load the pointer first
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                            struct_ptr = loaded_ptr;
                        } else if (semantic_type->is<types::PtrType>()) {
                            const auto& ptr = semantic_type->as<types::PtrType>();
                            struct_type = llvm_type_from_semantic(ptr.inner);
                            // Same - load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                            struct_ptr = loaded_ptr;
                        } else if (semantic_type->is<types::ClassType>()) {
                            const auto& cls = semantic_type->as<types::ClassType>();
                            struct_type = "%class." + cls.name;
                            // For class types, 'this' is already a direct pointer - no load needed
                        } else {
                            struct_type = llvm_type_from_semantic(semantic_type);
                        }
                    }
                }

                // Get the struct type name for field lookup
                std::string type_name = struct_type;
                // Strip pointer suffix if present
                if (type_name.ends_with("*")) {
                    type_name = type_name.substr(0, type_name.length() - 1);
                }
                // Strip %struct. or %class. prefix
                if (type_name.starts_with("%struct.")) {
                    type_name = type_name.substr(8);
                } else if (type_name.starts_with("%class.")) {
                    type_name = type_name.substr(7);
                }

                // Check if this is a property setter call
                std::string prop_key = type_name + "." + field.field;
                auto prop_it = class_properties_.find(prop_key);
                if (prop_it != class_properties_.end() && prop_it->second.has_setter) {
                    // Property assignment - call setter method instead of direct field store
                    const auto& prop_info = prop_it->second;
                    std::string setter_name =
                        "@tml_" + get_suite_prefix() + type_name + "_set_" + prop_info.name;

                    if (prop_info.is_static) {
                        // Static property setter - no 'this' parameter
                        emit_line("  call void " + setter_name + "(" + prop_info.llvm_type + " " +
                                  right + ")");
                    } else {
                        // Instance property setter - pass 'this' pointer and value
                        emit_line("  call void " + setter_name + "(ptr " + struct_ptr + ", " +
                                  prop_info.llvm_type + " " + right + ")");
                    }
                    return right;
                }

                // For class fields, use the class type without pointer suffix
                std::string gep_type = struct_type;
                if (gep_type.ends_with("*")) {
                    gep_type = gep_type.substr(0, gep_type.length() - 1);
                }

                // Get field pointer
                int field_idx = get_field_index(type_name, field.field);
                std::string field_type = get_field_type(type_name, field.field);
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + gep_type + ", ptr " +
                          struct_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                // Store value to field
                emit_line("  store " + field_type + " " + right + ", ptr " + field_ptr);
            }
        } else if (bin.left->is<parser::PathExpr>()) {
            // PathExpr assignment: Counter::count = value (static field via :: syntax)
            const auto& path = bin.left->as<parser::PathExpr>();
            if (path.path.segments.size() == 2) {
                std::string class_name = path.path.segments[0];
                std::string field_name = path.path.segments[1];
                std::string static_key = class_name + "." + field_name;
                auto static_it = static_fields_.find(static_key);
                if (static_it != static_fields_.end()) {
                    // Store to global static field
                    emit_line("  store " + static_it->second.type + " " + right + ", ptr " +
                              static_it->second.global_name);
                    return right;
                }
            }
        } else if (bin.left->is<parser::IndexExpr>()) {
            // Array index assignment: arr[i] = value
            const auto& idx_expr = bin.left->as<parser::IndexExpr>();

            // Get the array pointer
            std::string arr_ptr;
            std::string arr_type;
            if (idx_expr.object->is<parser::IdentExpr>()) {
                const auto& ident = idx_expr.object->as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    arr_ptr = it->second.reg;
                    arr_type = it->second.type;
                }
            }

            if (!arr_ptr.empty()) {
                // Generate index
                std::string idx = gen_expr(*idx_expr.index);
                std::string idx_i64 = fresh_reg();
                if (last_expr_type_ == "i64") {
                    idx_i64 = idx;
                } else {
                    emit_line("  " + idx_i64 + " = sext " + last_expr_type_ + " " + idx +
                              " to i64");
                }

                // Get element type from array type
                // Array type is like "[5 x i32]", we need "i32"
                std::string elem_type = "i32"; // default
                types::TypePtr semantic_type = infer_expr_type(*idx_expr.object);
                if (semantic_type && semantic_type->is<types::ArrayType>()) {
                    const auto& arr = semantic_type->as<types::ArrayType>();
                    elem_type = llvm_type_from_semantic(arr.element);
                }

                // Get element pointer
                std::string elem_ptr = fresh_reg();
                emit_line("  " + elem_ptr + " = getelementptr " + arr_type + ", ptr " + arr_ptr +
                          ", i64 0, i64 " + idx_i64);

                // Store value to element
                emit_line("  store " + elem_type + " " + right + ", ptr " + elem_ptr);
            }
        }
        return right;
    }

    // Handle compound assignment operators (+=, -=, *=, /=, %=, etc.)
    if (bin.op >= parser::BinaryOp::AddAssign && bin.op <= parser::BinaryOp::ShrAssign) {
        if (bin.left->is<parser::IdentExpr>()) {
            const auto& ident = bin.left->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;

                // Load current value
                std::string current = fresh_reg();
                emit_line("  " + current + " = load " + var.type + ", ptr " + var.reg);

                // Generate right operand
                std::string right = gen_expr(*bin.right);

                // Perform the operation
                std::string result = fresh_reg();
                std::string op_type = var.type;
                bool is_float = (op_type == "double" || op_type == "float");

                switch (bin.op) {
                case parser::BinaryOp::AddAssign:
                    if (is_float)
                        emit_line("  " + result + " = fadd " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = add nsw " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::SubAssign:
                    if (is_float)
                        emit_line("  " + result + " = fsub " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = sub nsw " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::MulAssign:
                    if (is_float)
                        emit_line("  " + result + " = fmul " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = mul nsw " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::DivAssign:
                    if (is_float)
                        emit_line("  " + result + " = fdiv " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = sdiv " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::ModAssign:
                    emit_line("  " + result + " = srem " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::BitAndAssign:
                    emit_line("  " + result + " = and " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::BitOrAssign:
                    emit_line("  " + result + " = or " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::BitXorAssign:
                    emit_line("  " + result + " = xor " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::ShlAssign:
                    emit_line("  " + result + " = shl " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::ShrAssign:
                    emit_line("  " + result + " = ashr " + op_type + " " + current + ", " + right);
                    break;
                default:
                    result = current;
                    break;
                }

                // Store result back
                emit_line("  store " + var.type + " " + result + ", ptr " + var.reg);
                last_expr_type_ = var.type;
                return result;
            }
        }
        report_error("Compound assignment requires a variable on the left side", bin.span);
        return "0";
    }

    // =========================================================================
    // String Concat Chain Optimization
    // =========================================================================
    // Detect patterns like "a" + "b" + "c" and optimize:
    // 1. If ALL are literals -> concatenate at compile time (zero runtime cost!)
    // 2. Otherwise -> fuse into single allocation (one call instead of N-1)
    if (bin.op == parser::BinaryOp::Add) {
        // Check if operands are strings (we infer types from left/right operands)
        types::TypePtr left_type_check = infer_expr_type(*bin.left);
        bool is_string_add =
            left_type_check && left_type_check->is<types::PrimitiveType>() &&
            left_type_check->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str;

        if (is_string_add) {
            // Collect all strings in the concat chain using a helper lambda
            std::vector<const parser::Expr*> strings;

            // Helper function to collect strings recursively
            std::function<void(const parser::Expr&)> collect_strings = [&](const parser::Expr& e) {
                if (e.is<parser::BinaryExpr>()) {
                    const auto& b = e.as<parser::BinaryExpr>();
                    if (b.op == parser::BinaryOp::Add) {
                        // Check if this binary op is also a string concat
                        types::TypePtr t = infer_expr_type(e);
                        if (t && t->is<types::PrimitiveType>() &&
                            t->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str) {
                            // This is also a string concat - recurse
                            collect_strings(*b.left);
                            collect_strings(*b.right);
                            return;
                        }
                    }
                }
                // Not a concat - this is a leaf string
                strings.push_back(&e);
            };

            // Start collection from left and right operands
            collect_strings(*bin.left);
            collect_strings(*bin.right);

            // =========================================================================
            // COMPILE-TIME STRING LITERAL CONCATENATION
            // =========================================================================
            // If ALL strings are literals, concatenate at compile time!
            // This makes "Hello" + " " + "World" + "!" essentially free.
            bool all_literals = true;
            std::string concatenated;
            for (const auto* s : strings) {
                if (s->is<parser::LiteralExpr>()) {
                    const auto& lit = s->as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                        concatenated += std::string(lit.token.string_value().value);
                        continue;
                    }
                }
                all_literals = false;
                break;
            }

            if (all_literals && strings.size() >= 2) {
                // Emit the concatenated string as a constant - ZERO RUNTIME COST!
                std::string const_name = add_string_literal(concatenated);
                last_expr_type_ = "ptr";
                return const_name;
            }
            // =========================================================================

            // =========================================================================
            // INLINE STRING CONCAT CODEGEN
            // =========================================================================
            // Generate inline LLVM IR for string concatenation to avoid FFI overhead.
            // For each string:
            //   - If literal: use known length (compile-time constant)
            //   - If runtime: call strlen
            // Then: malloc(total_len + 1), memcpy each string, null terminate
            //
            // This saves ~5-10ns per concat by avoiding the function call overhead.
            if (strings.size() >= 2 && strings.size() <= 4) {
                // Collect string values and their lengths
                struct StringInfo {
                    std::string value;  // LLVM register or constant
                    std::string len;    // Length (constant or register)
                    bool is_literal;    // True if compile-time known
                    size_t literal_len; // Length if literal
                };
                std::vector<StringInfo> infos;
                infos.reserve(strings.size());

                size_t total_literal_len = 0;
                bool has_runtime_strings = false;

                for (const auto* s : strings) {
                    StringInfo info;
                    if (s->is<parser::LiteralExpr>()) {
                        const auto& lit = s->as<parser::LiteralExpr>();
                        if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                            std::string str_val(lit.token.string_value().value);
                            info.value = add_string_literal(str_val);
                            info.literal_len = str_val.size();
                            info.len = std::to_string(str_val.size());
                            info.is_literal = true;
                            total_literal_len += str_val.size();
                            infos.push_back(info);
                            continue;
                        }
                    }
                    // Runtime string - need to call strlen
                    info.value = gen_expr(*s);
                    info.is_literal = false;
                    info.literal_len = 0;
                    has_runtime_strings = true;
                    infos.push_back(info);
                }

                // Calculate lengths for runtime strings
                std::string total_len_reg;
                if (has_runtime_strings) {
                    // Start with known literal length
                    total_len_reg = fresh_reg();
                    emit_line("  " + total_len_reg + " = add i64 0, " +
                              std::to_string(total_literal_len));

                    for (auto& info : infos) {
                        if (!info.is_literal) {
                            // Call strlen for runtime strings
                            std::string len_reg = fresh_reg();
                            emit_line("  " + len_reg + " = call i64 @strlen(ptr " + info.value +
                                      ")");
                            info.len = len_reg;
                            // Add to total
                            std::string new_total = fresh_reg();
                            emit_line("  " + new_total + " = add i64 " + total_len_reg + ", " +
                                      len_reg);
                            total_len_reg = new_total;
                        }
                    }
                } else {
                    // All literals - total is known at compile time
                    total_len_reg = std::to_string(total_literal_len);
                }

                // Allocate buffer: malloc(total_len + 1) for null terminator
                std::string alloc_size = fresh_reg();
                emit_line("  " + alloc_size + " = add i64 " + total_len_reg + ", 1");
                std::string result_ptr = fresh_reg();
                emit_line("  " + result_ptr + " = call ptr @malloc(i64 " + alloc_size + ")");

                // Copy each string using memcpy
                std::string offset = "0";
                bool offset_is_const = true; // Track if offset is a compile-time constant
                size_t const_offset = 0;     // Numeric value if constant

                for (size_t i = 0; i < infos.size(); ++i) {
                    const auto& info = infos[i];
                    // Calculate destination pointer
                    std::string dest_ptr;
                    if (offset == "0") {
                        dest_ptr = result_ptr;
                    } else {
                        dest_ptr = fresh_reg();
                        emit_line("  " + dest_ptr + " = getelementptr i8, ptr " + result_ptr +
                                  ", i64 " + offset);
                    }

                    // Copy the string (memcpy intrinsic)
                    emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr " + dest_ptr + ", ptr " +
                              info.value + ", i64 " + info.len + ", i1 false)");

                    // Update offset for next string
                    if (i < infos.size() - 1) {
                        if (offset_is_const && info.is_literal) {
                            // Both offset and current length are constants - keep as constant
                            const_offset += info.literal_len;
                            offset = std::to_string(const_offset);
                        } else {
                            // Need runtime addition
                            std::string new_offset = fresh_reg();
                            emit_line("  " + new_offset + " = add i64 " + offset + ", " + info.len);
                            offset = new_offset;
                            offset_is_const = false;
                        }
                    }
                }

                // Null terminate
                std::string end_ptr = fresh_reg();
                emit_line("  " + end_ptr + " = getelementptr i8, ptr " + result_ptr + ", i64 " +
                          total_len_reg);
                emit_line("  store i8 0, ptr " + end_ptr);

                last_expr_type_ = "ptr";
                return result_ptr;
            }
            // =========================================================================

            // For 5+ strings, fall through to default two-operand concatenation
        }
    }
    // =========================================================================

    std::string left = gen_expr(*bin.left);
    std::string left_type = last_expr_type_;
    std::string right = gen_expr(*bin.right);
    std::string right_type = last_expr_type_;
    std::string result = fresh_reg();

    // Check if either operand is a struct (enum)
    bool is_enum_struct = left_type.starts_with("%struct.") && right_type.starts_with("%struct.");

    // For enum struct comparisons, extract the tag field (first i32)
    if (is_enum_struct) {
        // Allocate space for left struct
        std::string left_alloca = fresh_reg();
        emit_line("  " + left_alloca + " = alloca " + left_type);
        emit_line("  store " + left_type + " " + left + ", ptr " + left_alloca);

        // Extract tag from left
        std::string left_tag_ptr = fresh_reg();
        emit_line("  " + left_tag_ptr + " = getelementptr " + left_type + ", ptr " + left_alloca +
                  ", i32 0, i32 0");
        std::string left_tag = fresh_reg();
        emit_line("  " + left_tag + " = load i32, ptr " + left_tag_ptr);

        // Allocate space for right struct
        std::string right_alloca = fresh_reg();
        emit_line("  " + right_alloca + " = alloca " + right_type);
        emit_line("  store " + right_type + " " + right + ", ptr " + right_alloca);

        // Extract tag from right
        std::string right_tag_ptr = fresh_reg();
        emit_line("  " + right_tag_ptr + " = getelementptr " + right_type + ", ptr " +
                  right_alloca + ", i32 0, i32 0");
        std::string right_tag = fresh_reg();
        emit_line("  " + right_tag + " = load i32, ptr " + right_tag_ptr);

        // Replace left/right with tag values
        left = left_tag;
        right = right_tag;
        left_type = "i32";
        right_type = "i32";
    }

    // Get semantic types for signedness detection
    types::TypePtr left_semantic = infer_expr_type(*bin.left);
    types::TypePtr right_semantic = infer_expr_type(*bin.right);

    auto check_unsigned = [](const types::TypePtr& t) -> bool {
        if (!t)
            return false;
        if (auto* prim = std::get_if<types::PrimitiveType>(&t->kind)) {
            return prim->kind == types::PrimitiveKind::U8 ||
                   prim->kind == types::PrimitiveKind::U16 ||
                   prim->kind == types::PrimitiveKind::U32 ||
                   prim->kind == types::PrimitiveKind::U64 ||
                   prim->kind == types::PrimitiveKind::U128;
        }
        return false;
    };

    bool left_unsigned = check_unsigned(left_semantic);
    bool right_unsigned = check_unsigned(right_semantic);

    // Check if either operand is a float
    bool is_float = (left_type == "double" || left_type == "float" || right_type == "double" ||
                     right_type == "float");

    // Determine if we're dealing with F32 (float) or F64 (double)
    // F32 is only used when BOTH operands are float (not mixed with double)
    bool is_f32 = (left_type == "float" && (right_type == "float" || right_type != "double")) ||
                  (right_type == "float" && (left_type == "float" || left_type != "double"));
    // Override: if either is double, use double
    if (left_type == "double" || right_type == "double") {
        is_f32 = false;
    }
    std::string float_type = is_f32 ? "float" : "double";

    // Handle float literals (e.g., 3.0)
    if (!is_float && bin.right->is<parser::LiteralExpr>()) {
        const auto& lit = bin.right->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
            is_float = true;
        }
    }
    if (!is_float && bin.left->is<parser::LiteralExpr>()) {
        const auto& lit = bin.left->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
            is_float = true;
        }
    }

    // Helper to get integer type size in bits
    auto int_type_size = [](const std::string& t) -> int {
        if (t == "i8")
            return 8;
        if (t == "i16")
            return 16;
        if (t == "i32")
            return 32;
        if (t == "i64")
            return 64;
        if (t == "i128")
            return 128;
        return 0;
    };

    // Helper to extend integer to target type
    auto extend_int = [&](std::string& val, const std::string& from_type,
                          const std::string& to_type, bool is_unsigned_val) {
        if (from_type == to_type)
            return;
        std::string conv = fresh_reg();
        if (is_unsigned_val) {
            emit_line("  " + conv + " = zext " + from_type + " " + val + " to " + to_type);
        } else {
            emit_line("  " + conv + " = sext " + from_type + " " + val + " to " + to_type);
        }
        val = conv;
    };

    if (is_float) {
        // Convert integer operands to the target float type if needed
        if (int_type_size(left_type) > 0) {
            std::string conv = fresh_reg();
            if (left_unsigned) {
                emit_line("  " + conv + " = uitofp " + left_type + " " + left + " to " +
                          float_type);
            } else {
                emit_line("  " + conv + " = sitofp " + left_type + " " + left + " to " +
                          float_type);
            }
            left = conv;
            left_type = float_type;
        }
        if (int_type_size(right_type) > 0) {
            std::string conv = fresh_reg();
            if (right_unsigned) {
                emit_line("  " + conv + " = uitofp " + right_type + " " + right + " to " +
                          float_type);
            } else {
                emit_line("  " + conv + " = sitofp " + right_type + " " + right + " to " +
                          float_type);
            }
            right = conv;
            right_type = float_type;
        }
        // Handle float <-> double conversions
        if (left_type == "float" && float_type == "double") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = fpext float " + left + " to double");
            left = conv;
            left_type = "double";
        } else if (left_type == "double" && float_type == "float") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = fptrunc double " + left + " to float");
            left = conv;
            left_type = "float";
        }
        if (right_type == "float" && float_type == "double") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = fpext float " + right + " to double");
            right = conv;
            right_type = "double";
        } else if (right_type == "double" && float_type == "float") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = fptrunc double " + right + " to float");
            right = conv;
            right_type = "float";
        }
        // Also handle float literals
        if (bin.right->is<parser::LiteralExpr>()) {
            const auto& lit = bin.right->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                right = std::to_string(lit.token.float_value().value);
            }
        }
    } else {
        // Integer type promotion: promote both to the larger type
        int left_size = int_type_size(left_type);
        int right_size = int_type_size(right_type);

        if (left_size > 0 && right_size > 0 && left_size != right_size) {
            // Promote smaller to larger
            if (left_size > right_size) {
                extend_int(right, right_type, left_type, right_unsigned);
                right_type = left_type;
            } else {
                extend_int(left, left_type, right_type, left_unsigned);
                left_type = right_type;
            }
        }
    }

    // Check if either operand is i64 (after promotion)
    bool is_i64 = (left_type == "i64" || right_type == "i64");
    // Check for smaller integer types (i8, i16)
    bool is_i8 = (left_type == "i8" && right_type == "i8");
    bool is_i16 = (left_type == "i16" && right_type == "i16");

    // Check if either operand is Bool (i1)
    bool is_bool = (left_type == "i1" || right_type == "i1");

    // Check if BOTH operands are strings (ptr) - only then use str_eq
    bool is_string = (left_type == "ptr" && right_type == "ptr");

    // Determine the integer type to use (largest type wins)
    std::string int_type = "i32";
    if (is_bool) {
        int_type = "i1";
    } else if (is_i64) {
        int_type = "i64";
    } else if (is_i16) {
        int_type = "i16";
    } else if (is_i8) {
        int_type = "i8";
    }

    // Use unsigned operations if either operand is unsigned
    bool is_unsigned = left_unsigned || right_unsigned;

    // Check for pointer arithmetic (ptr + int or int + ptr)
    bool is_ptr_arith = (left_type == "ptr" && right_type != "ptr") ||
                        (right_type == "ptr" && left_type != "ptr");
    std::string ptr_operand, idx_operand;
    types::TypePtr ptr_semantic_type = nullptr;
    if (is_ptr_arith) {
        if (left_type == "ptr") {
            ptr_operand = left;
            idx_operand = right;
            ptr_semantic_type = left_semantic;
        } else {
            ptr_operand = right;
            idx_operand = left;
            ptr_semantic_type = right_semantic;
        }
    }

    switch (bin.op) {
    case parser::BinaryOp::Add:
        if (is_ptr_arith) {
            // Pointer arithmetic: ptr + int -> getelementptr
            // Determine element type from semantic pointer type
            std::string elem_type = "i8"; // default to byte-level arithmetic
            if (ptr_semantic_type && ptr_semantic_type->is<types::PtrType>()) {
                const auto& ptr = ptr_semantic_type->as<types::PtrType>();
                if (ptr.inner) {
                    elem_type = llvm_type_from_semantic(ptr.inner);
                }
            }
            emit_line("  " + result + " = getelementptr " + elem_type + ", ptr " + ptr_operand +
                      ", i64 " + idx_operand);
            last_expr_type_ = "ptr";
        } else if (is_string) {
            // String concatenation using str_concat_opt (O(1) amortized)
            emit_line("  " + result + " = call ptr @str_concat_opt(ptr " + left + ", ptr " + right +
                      ")");
            last_expr_type_ = "ptr";
        } else if (is_float) {
            emit_line("  " + result + " = fadd " + float_type + " " + left + ", " + right);
            last_expr_type_ = float_type;
        } else if (is_unsigned) {
            emit_line("  " + result + " = add nuw " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        } else {
            emit_line("  " + result + " = add nsw " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        }
        break;
    case parser::BinaryOp::Sub:
        if (is_float) {
            emit_line("  " + result + " = fsub " + float_type + " " + left + ", " + right);
            last_expr_type_ = float_type;
        } else if (is_unsigned) {
            emit_line("  " + result + " = sub nuw " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        } else {
            emit_line("  " + result + " = sub nsw " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        }
        break;
    case parser::BinaryOp::Mul:
        if (is_float) {
            emit_line("  " + result + " = fmul " + float_type + " " + left + ", " + right);
            last_expr_type_ = float_type;
        } else if (is_unsigned) {
            emit_line("  " + result + " = mul nuw " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        } else {
            emit_line("  " + result + " = mul nsw " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        }
        break;
    case parser::BinaryOp::Div:
        if (is_float) {
            emit_line("  " + result + " = fdiv " + float_type + " " + left + ", " + right);
            last_expr_type_ = float_type;
        } else if (is_unsigned) {
            emit_line("  " + result + " = udiv " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        } else {
            emit_line("  " + result + " = sdiv " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
        }
        break;
    case parser::BinaryOp::Mod:
        if (is_unsigned) {
            emit_line("  " + result + " = urem " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = srem " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = int_type;
        break;
    // Comparisons return i1 (use fcmp for floats, icmp for integers, str_eq for strings)
    case parser::BinaryOp::Eq:
        if (is_float) {
            emit_line("  " + result + " = fcmp oeq " + float_type + " " + left + ", " + right);
        } else if (is_string) {
            // String comparison using str_eq runtime function (returns i32, convert to i1)
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + left + ", ptr " + right + ")");
            emit_line("  " + result + " = icmp ne i32 " + eq_i32 + ", 0");
        } else {
            emit_line("  " + result + " = icmp eq " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Ne:
        if (is_float) {
            emit_line("  " + result + " = fcmp one " + float_type + " " + left + ", " + right);
        } else if (is_string) {
            // String comparison: NOT str_eq (str_eq returns i32, convert to i1)
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + left + ", ptr " + right + ")");
            emit_line("  " + result + " = icmp eq i32 " + eq_i32 + ", 0");
        } else {
            emit_line("  " + result + " = icmp ne " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Lt:
        if (is_float) {
            emit_line("  " + result + " = fcmp olt " + float_type + " " + left + ", " + right);
        } else if (is_unsigned) {
            emit_line("  " + result + " = icmp ult " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp slt " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Gt:
        if (is_float) {
            emit_line("  " + result + " = fcmp ogt " + float_type + " " + left + ", " + right);
        } else if (is_unsigned) {
            emit_line("  " + result + " = icmp ugt " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp sgt " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Le:
        if (is_float) {
            emit_line("  " + result + " = fcmp ole " + float_type + " " + left + ", " + right);
        } else if (is_unsigned) {
            emit_line("  " + result + " = icmp ule " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp sle " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Ge:
        if (is_float) {
            emit_line("  " + result + " = fcmp oge " + float_type + " " + left + ", " + right);
        } else if (is_unsigned) {
            emit_line("  " + result + " = icmp uge " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp sge " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    // Logical operators work on i1
    case parser::BinaryOp::And:
        emit_line("  " + result + " = and i1 " + left + ", " + right);
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Or:
        emit_line("  " + result + " = or i1 " + left + ", " + right);
        last_expr_type_ = "i1";
        break;
    // Bitwise operators work on same type
    case parser::BinaryOp::BitAnd:
        emit_line("  " + result + " = and " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::BitOr:
        emit_line("  " + result + " = or " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::BitXor:
        emit_line("  " + result + " = xor " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::Shl:
        // nuw = no unsigned wrap for shift
        emit_line("  " + result + " = shl nuw " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::Shr:
        if (is_unsigned) {
            // Logical shift right for unsigned (fills with 0s)
            emit_line("  " + result + " = lshr " + int_type + " " + left + ", " + right);
        } else {
            // Arithmetic shift right for signed (fills with sign bit)
            emit_line("  " + result + " = ashr " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = int_type;
        break;
    // Note: Assign is handled above before evaluating left/right
    default:
        emit_line("  " + result + " = add nsw i32 " + left + ", " + right);
        break;
    }

    return result;
}

} // namespace tml::codegen
