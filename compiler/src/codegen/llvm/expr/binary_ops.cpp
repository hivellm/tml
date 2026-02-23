TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Binary Operator Evaluation
//!
//! This file implements operand evaluation, type coercion, and operator codegen
//! for binary expressions. It is the second half of the binary expression pipeline,
//! called from gen_binary() after assignment and string concatenation are handled.
//!
//! ## Responsibilities
//!
//! - Evaluate left/right operands
//! - Tuple comparison (element-by-element equality and ordering)
//! - Enum struct tag comparison
//! - Type promotion (integer widening, float conversion)
//! - String detection (Str vs raw pointer)
//! - Pointer arithmetic
//! - Operator switch: arithmetic, comparison, logical, bitwise

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace tml::codegen {

auto LLVMIRGen::gen_binary_ops(const parser::BinaryExpr& bin) -> std::string {
    std::string left = gen_expr(*bin.left);
    std::string left_type = last_expr_type_;
    std::string right = gen_expr(*bin.right);
    std::string right_type = last_expr_type_;
    std::string result = fresh_reg();

    // Check if either operand is a struct (enum)
    bool is_enum_struct = left_type.starts_with("%struct.") && right_type.starts_with("%struct.");

    // Check if operands are tuples (anonymous LLVM structs: { type1, type2, ... })
    bool is_tuple = left_type.starts_with("{ ") && left_type.ends_with(" }") &&
                    right_type.starts_with("{ ") && right_type.ends_with(" }") &&
                    left_type == right_type;

    // Handle tuple comparison (element-by-element)
    if (is_tuple && (bin.op == parser::BinaryOp::Eq || bin.op == parser::BinaryOp::Ne)) {
        // Emit coverage for tuple PartialEq
        if (bin.op == parser::BinaryOp::Eq) {
            emit_coverage("PartialEq::eq");
        } else {
            emit_coverage("PartialEq::ne");
        }

        // Parse tuple element types from the LLVM type string "{ i32, i32 }"
        std::vector<std::string> elem_types;
        std::string inner = left_type.substr(2, left_type.size() - 4); // Remove "{ " and " }"
        size_t pos = 0;
        while (pos < inner.size()) {
            // Find the next comma or end
            size_t comma_pos = inner.find(", ", pos);
            if (comma_pos == std::string::npos) {
                elem_types.push_back(inner.substr(pos));
                break;
            }
            elem_types.push_back(inner.substr(pos, comma_pos - pos));
            pos = comma_pos + 2; // Skip ", "
        }

        // Allocate space for both tuples
        std::string left_alloca = fresh_reg();
        emit_line("  " + left_alloca + " = alloca " + left_type);
        emit_line("  store " + left_type + " " + left + ", ptr " + left_alloca);

        std::string right_alloca = fresh_reg();
        emit_line("  " + right_alloca + " = alloca " + right_type);
        emit_line("  store " + right_type + " " + right + ", ptr " + right_alloca);

        // Compare element by element
        std::string cmp_result = "1"; // Start with true (i1 1)
        for (size_t i = 0; i < elem_types.size(); ++i) {
            // Get pointers to elements
            std::string left_elem_ptr = fresh_reg();
            emit_line("  " + left_elem_ptr + " = getelementptr " + left_type + ", ptr " +
                      left_alloca + ", i32 0, i32 " + std::to_string(i));
            std::string left_elem = fresh_reg();
            emit_line("  " + left_elem + " = load " + elem_types[i] + ", ptr " + left_elem_ptr);

            std::string right_elem_ptr = fresh_reg();
            emit_line("  " + right_elem_ptr + " = getelementptr " + right_type + ", ptr " +
                      right_alloca + ", i32 0, i32 " + std::to_string(i));
            std::string right_elem = fresh_reg();
            emit_line("  " + right_elem + " = load " + elem_types[i] + ", ptr " + right_elem_ptr);

            // Compare elements
            std::string elem_cmp = fresh_reg();
            if (elem_types[i] == "double" || elem_types[i] == "float") {
                emit_line("  " + elem_cmp + " = fcmp oeq " + elem_types[i] + " " + left_elem +
                          ", " + right_elem);
            } else {
                emit_line("  " + elem_cmp + " = icmp eq " + elem_types[i] + " " + left_elem + ", " +
                          right_elem);
            }

            // AND with previous result
            std::string new_result = fresh_reg();
            emit_line("  " + new_result + " = and i1 " + cmp_result + ", " + elem_cmp);
            cmp_result = new_result;
        }

        // For != we need to negate the result
        if (bin.op == parser::BinaryOp::Ne) {
            std::string neg_result = fresh_reg();
            emit_line("  " + neg_result + " = xor i1 " + cmp_result + ", 1");
            last_expr_type_ = "i1";
            return neg_result;
        }

        last_expr_type_ = "i1";
        return cmp_result;
    }

    // Handle tuple ordering comparison (lexicographic order: <, >, <=, >=)
    if (is_tuple && (bin.op == parser::BinaryOp::Lt || bin.op == parser::BinaryOp::Gt ||
                     bin.op == parser::BinaryOp::Le || bin.op == parser::BinaryOp::Ge)) {
        // Emit coverage for tuple PartialOrd
        emit_coverage("PartialOrd::partial_cmp");

        // Parse tuple element types from the LLVM type string "{ i32, i32 }"
        std::vector<std::string> elem_types;
        std::string inner = left_type.substr(2, left_type.size() - 4); // Remove "{ " and " }"
        size_t pos = 0;
        while (pos < inner.size()) {
            size_t comma_pos = inner.find(", ", pos);
            if (comma_pos == std::string::npos) {
                elem_types.push_back(inner.substr(pos));
                break;
            }
            elem_types.push_back(inner.substr(pos, comma_pos - pos));
            pos = comma_pos + 2;
        }

        // Allocate space for both tuples
        std::string left_alloca = fresh_reg();
        emit_line("  " + left_alloca + " = alloca " + left_type);
        emit_line("  store " + left_type + " " + left + ", ptr " + left_alloca);

        std::string right_alloca = fresh_reg();
        emit_line("  " + right_alloca + " = alloca " + right_type);
        emit_line("  store " + right_type + " " + right + ", ptr " + right_alloca);

        // Lexicographic comparison using result alloca
        // Initialize with "all equal" result (false for </>, true for <=/>=)
        bool equal_result = (bin.op == parser::BinaryOp::Le || bin.op == parser::BinaryOp::Ge);
        std::string final_label = fresh_label("tuple_cmp_done");
        std::string result_alloca = fresh_reg();
        emit_line("  " + result_alloca + " = alloca i1");
        emit_line("  store i1 " + std::string(equal_result ? "1" : "0") + ", ptr " + result_alloca);

        for (size_t i = 0; i < elem_types.size(); ++i) {
            // Get pointers to elements
            std::string left_elem_ptr = fresh_reg();
            emit_line("  " + left_elem_ptr + " = getelementptr " + left_type + ", ptr " +
                      left_alloca + ", i32 0, i32 " + std::to_string(i));
            std::string left_elem = fresh_reg();
            emit_line("  " + left_elem + " = load " + elem_types[i] + ", ptr " + left_elem_ptr);

            std::string right_elem_ptr = fresh_reg();
            emit_line("  " + right_elem_ptr + " = getelementptr " + right_type + ", ptr " +
                      right_alloca + ", i32 0, i32 " + std::to_string(i));
            std::string right_elem = fresh_reg();
            emit_line("  " + right_elem + " = load " + elem_types[i] + ", ptr " + right_elem_ptr);

            // Check if elements are equal
            std::string eq_cmp = fresh_reg();
            bool is_float_elem = (elem_types[i] == "double" || elem_types[i] == "float");
            if (is_float_elem) {
                emit_line("  " + eq_cmp + " = fcmp oeq " + elem_types[i] + " " + left_elem + ", " +
                          right_elem);
            } else {
                emit_line("  " + eq_cmp + " = icmp eq " + elem_types[i] + " " + left_elem + ", " +
                          right_elem);
            }

            std::string not_eq_label = fresh_label("tuple_cmp_neq");
            std::string next_label =
                (i + 1 < elem_types.size()) ? fresh_label("tuple_cmp_next") : final_label;

            emit_line("  br i1 " + eq_cmp + ", label %" + next_label + ", label %" + not_eq_label);

            // Not equal - do the actual comparison
            emit_line(not_eq_label + ":");
            std::string cmp_result = fresh_reg();

            // Determine comparison predicate based on operator
            std::string cmp_pred;
            if (is_float_elem) {
                switch (bin.op) {
                case parser::BinaryOp::Lt:
                    cmp_pred = "olt";
                    break;
                case parser::BinaryOp::Gt:
                    cmp_pred = "ogt";
                    break;
                case parser::BinaryOp::Le:
                    cmp_pred = "olt";
                    break; // for <=, if not equal, check <
                case parser::BinaryOp::Ge:
                    cmp_pred = "ogt";
                    break; // for >=, if not equal, check >
                default:
                    cmp_pred = "oeq";
                }
                emit_line("  " + cmp_result + " = fcmp " + cmp_pred + " " + elem_types[i] + " " +
                          left_elem + ", " + right_elem);
            } else {
                switch (bin.op) {
                case parser::BinaryOp::Lt:
                    cmp_pred = "slt";
                    break;
                case parser::BinaryOp::Gt:
                    cmp_pred = "sgt";
                    break;
                case parser::BinaryOp::Le:
                    cmp_pred = "slt";
                    break; // for <=, if not equal, check <
                case parser::BinaryOp::Ge:
                    cmp_pred = "sgt";
                    break; // for >=, if not equal, check >
                default:
                    cmp_pred = "eq";
                }
                emit_line("  " + cmp_result + " = icmp " + cmp_pred + " " + elem_types[i] + " " +
                          left_elem + ", " + right_elem);
            }

            emit_line("  store i1 " + cmp_result + ", ptr " + result_alloca);
            emit_line("  br label %" + final_label);

            // Continue to next element
            if (i + 1 < elem_types.size()) {
                emit_line(next_label + ":");
            }
        }

        // All elements were equal - result_alloca already has the correct value
        emit_line(final_label + ":");
        std::string final_result = fresh_reg();
        emit_line("  " + final_result + " = load i1, ptr " + result_alloca);

        last_expr_type_ = "i1";
        return final_result;
    }

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

    // Helper to check if semantic type is a string (Str)
    auto is_str_type = [](const types::TypePtr& t) -> bool {
        if (!t)
            return false;
        // Check for Str primitive type
        if (auto* prim = std::get_if<types::PrimitiveType>(&t->kind)) {
            return prim->kind == types::PrimitiveKind::Str;
        }
        // Check for NamedType "Str"
        if (auto* named = std::get_if<types::NamedType>(&t->kind)) {
            return named->name == "Str";
        }
        return false;
    };

    // Helper to check if type is unknown (null or unit - meaning type couldn't be determined)
    auto is_unknown_type = [](const types::TypePtr& t) -> bool {
        if (!t)
            return true;
        // Check for unit type (void) - this happens when infer_expr_type fails
        if (auto* prim = std::get_if<types::PrimitiveType>(&t->kind)) {
            return prim->kind == types::PrimitiveKind::Unit;
        }
        return false;
    };

    // Check if BOTH operands are strings (Str type) - only then use str_eq/str_concat
    // Important: Don't use str_eq for general pointer comparisons (like Ptr[Node[T]])
    bool is_string = false;
    if (left_type == "ptr" && right_type == "ptr") {
        bool left_is_str = is_str_type(left_semantic);
        bool right_is_str = is_str_type(right_semantic);

        // If both are known Str, definitely string operation
        if (left_is_str && right_is_str) {
            is_string = true;
        }
        // For Add operation: if at least one is known Str, and the other is unknown
        // (null or unit semantic type), treat as string concatenation.
        // Reasoning: ptr + ptr as integer addition makes no sense.
        else if (bin.op == parser::BinaryOp::Add) {
            bool left_unknown = is_unknown_type(left_semantic);
            bool right_unknown = is_unknown_type(right_semantic);

            // If one is Str and other is unknown, it's probably string concat
            if ((left_is_str && right_unknown) || (right_is_str && left_unknown)) {
                is_string = true;
            }
            // If both are unknown but we're adding two ptrs, also assume string concat
            // (since ptr + ptr is meaningless otherwise)
            else if (left_unknown && right_unknown) {
                is_string = true;
            }
        }
    }

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
    bool is_ptr_arith =
        (left_type == "ptr" && right_type != "ptr") || (right_type == "ptr" && left_type != "ptr");
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

    // =========================================================================
    // Coverage Tracking for Inlined Primitive Operators
    // =========================================================================
    // When coverage is enabled, we track which trait methods are implicitly
    // used by inlined primitive operators. This allows coverage reports to
    // show that using `a & b` exercises `BitAnd::bitand`.
    auto emit_operator_coverage = [&](const std::string& trait_name, const std::string& method) {
        emit_coverage(trait_name + "::" + method);
    };

    switch (bin.op) {
    case parser::BinaryOp::Add:
        emit_operator_coverage("Add", "add");
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
        emit_operator_coverage("Sub", "sub");
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
        emit_operator_coverage("Mul", "mul");
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
        emit_operator_coverage("Div", "div");
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
        emit_operator_coverage("Rem", "rem");
        if (is_unsigned) {
            emit_line("  " + result + " = urem " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = srem " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = int_type;
        break;
    // Comparisons return i1 (use fcmp for floats, icmp for integers, str_eq for strings)
    case parser::BinaryOp::Eq:
        emit_operator_coverage("Eq", "eq");
        if (is_float) {
            emit_line("  " + result + " = fcmp oeq " + float_type + " " + left + ", " + right);
        } else if (is_string) {
            // String comparison using str_eq runtime function (returns i32, convert to i1)
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + left + ", ptr " + right + ")");
            emit_line("  " + result + " = icmp ne i32 " + eq_i32 + ", 0");
        } else if (left_type == "ptr" && right_type == "ptr") {
            // Pointer equality comparison (non-string pointers)
            emit_line("  " + result + " = icmp eq ptr " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp eq " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Ne:
        emit_operator_coverage("Eq", "ne");
        if (is_float) {
            emit_line("  " + result + " = fcmp one " + float_type + " " + left + ", " + right);
        } else if (is_string) {
            // String comparison: NOT str_eq (str_eq returns i32, convert to i1)
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + left + ", ptr " + right + ")");
            emit_line("  " + result + " = icmp eq i32 " + eq_i32 + ", 0");
        } else if (left_type == "ptr" && right_type == "ptr") {
            // Pointer inequality comparison (non-string pointers)
            emit_line("  " + result + " = icmp ne ptr " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp ne " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Lt:
        emit_operator_coverage("Ord", "lt");
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
        emit_operator_coverage("Ord", "gt");
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
        emit_operator_coverage("Ord", "le");
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
        emit_operator_coverage("Ord", "ge");
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
        emit_operator_coverage("BitAnd", "bitand");
        emit_line("  " + result + " = and " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::BitOr:
        emit_operator_coverage("BitOr", "bitor");
        emit_line("  " + result + " = or " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::BitXor:
        emit_operator_coverage("BitXor", "bitxor");
        emit_line("  " + result + " = xor " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::Shl:
        emit_operator_coverage("Shl", "shift_left");
        // nuw = no unsigned wrap for shift
        emit_line("  " + result + " = shl nuw " + int_type + " " + left + ", " + right);
        last_expr_type_ = int_type;
        break;
    case parser::BinaryOp::Shr:
        emit_operator_coverage("Shr", "shift_right");
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
