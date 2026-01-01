// LLVM IR generator - Binary expression generation
// Handles: arithmetic, comparison, logical, bitwise, assignment operators

#include "codegen/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_binary(const parser::BinaryExpr& bin) -> std::string {
    // Handle assignment specially - don't evaluate left for deref assignments
    if (bin.op == parser::BinaryOp::Assign) {
        std::string right = gen_expr(*bin.right);

        if (bin.left->is<parser::IdentExpr>()) {
            auto it = locals_.find(bin.left->as<parser::IdentExpr>().name);
            if (it != locals_.end()) {
                emit_line("  store " + it->second.type + " " + right + ", ptr " + it->second.reg);
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
            // Field assignment: obj.field = value or this.field = value
            const auto& field = bin.left->as<parser::FieldExpr>();
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
                        } else {
                            struct_type = llvm_type_from_semantic(semantic_type);
                        }
                    }
                }

                // Get the struct type name for field lookup
                std::string type_name = struct_type;
                if (type_name.starts_with("%struct.")) {
                    type_name = type_name.substr(8);
                }

                // Get field pointer
                int field_idx = get_field_index(type_name, field.field);
                std::string field_type = get_field_type(type_name, field.field);
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " +
                          struct_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                // Store value to field
                emit_line("  store " + field_type + " " + right + ", ptr " + field_ptr);
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
        // Convert integer operands to double if needed
        if (int_type_size(left_type) > 0) {
            std::string conv = fresh_reg();
            if (left_unsigned) {
                emit_line("  " + conv + " = uitofp " + left_type + " " + left + " to double");
            } else {
                emit_line("  " + conv + " = sitofp " + left_type + " " + left + " to double");
            }
            left = conv;
            left_type = "double";
        }
        if (int_type_size(right_type) > 0) {
            std::string conv = fresh_reg();
            if (right_unsigned) {
                emit_line("  " + conv + " = uitofp " + right_type + " " + right + " to double");
            } else {
                emit_line("  " + conv + " = sitofp " + right_type + " " + right + " to double");
            }
            right = conv;
            right_type = "double";
        }
        // Also handle float literals that need double representation
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

    switch (bin.op) {
    case parser::BinaryOp::Add:
        if (is_string) {
            // String concatenation using str_concat
            emit_line("  " + result + " = call ptr @str_concat(ptr " + left + ", ptr " + right +
                      ")");
            last_expr_type_ = "ptr";
        } else if (is_float) {
            emit_line("  " + result + " = fadd double " + left + ", " + right);
            last_expr_type_ = "double";
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
            emit_line("  " + result + " = fsub double " + left + ", " + right);
            last_expr_type_ = "double";
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
            emit_line("  " + result + " = fmul double " + left + ", " + right);
            last_expr_type_ = "double";
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
            emit_line("  " + result + " = fdiv double " + left + ", " + right);
            last_expr_type_ = "double";
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
            emit_line("  " + result + " = fcmp oeq double " + left + ", " + right);
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
            emit_line("  " + result + " = fcmp one double " + left + ", " + right);
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
            emit_line("  " + result + " = fcmp olt double " + left + ", " + right);
        } else if (is_unsigned) {
            emit_line("  " + result + " = icmp ult " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp slt " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Gt:
        if (is_float) {
            emit_line("  " + result + " = fcmp ogt double " + left + ", " + right);
        } else if (is_unsigned) {
            emit_line("  " + result + " = icmp ugt " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp sgt " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Le:
        if (is_float) {
            emit_line("  " + result + " = fcmp ole double " + left + ", " + right);
        } else if (is_unsigned) {
            emit_line("  " + result + " = icmp ule " + int_type + " " + left + ", " + right);
        } else {
            emit_line("  " + result + " = icmp sle " + int_type + " " + left + ", " + right);
        }
        last_expr_type_ = "i1";
        break;
    case parser::BinaryOp::Ge:
        if (is_float) {
            emit_line("  " + result + " = fcmp oge double " + left + ", " + right);
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
