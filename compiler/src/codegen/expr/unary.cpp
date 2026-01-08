//! # LLVM IR Generator - Unary Expressions
//!
//! This file implements unary operator code generation.
//!
//! ## Operators
//!
//! | Operator | TML Syntax  | LLVM Instruction       |
//! |----------|-------------|------------------------|
//! | Negate   | `-x`        | `sub 0, x` or `fneg`   |
//! | Not      | `not x`     | `xor x, 1` (bool)      |
//! | BitNot   | `~x`        | `xor x, -1`            |
//! | Ref      | `ref x`     | Return alloca ptr      |
//! | RefMut   | `mut ref x` | Return alloca ptr      |
//! | Deref    | `*ptr`      | `load` from ptr        |
//!
//! ## Reference Operations
//!
//! `ref` and `mut ref` return the address of a variable without loading:
//! - For identifiers: return the alloca register directly
//! - For field access: return GEP to field
//!
//! ## Dereference
//!
//! `*ptr` emits a `load` instruction from the pointer.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_unary(const parser::UnaryExpr& unary) -> std::string {
    // Handle ref operations specially - we need the address, not the value
    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        // Get pointer to the operand
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                // Return the alloca pointer directly (don't load)
                last_expr_type_ = "ptr";
                return it->second.reg;
            }
        }
        // Handle field access: ref this.field, ref x.field
        if (unary.operand->is<parser::FieldExpr>()) {
            const auto& field_expr = unary.operand->as<parser::FieldExpr>();

            // Get the struct pointer
            std::string struct_ptr;
            types::TypePtr base_type = infer_expr_type(*field_expr.object);

            if (field_expr.object->is<parser::IdentExpr>()) {
                const auto& ident = field_expr.object->as<parser::IdentExpr>();
                auto local_it = locals_.find(ident.name);
                if (local_it != locals_.end()) {
                    struct_ptr = local_it->second.reg;
                }
            }

            if (!struct_ptr.empty() && base_type) {
                // Get the field index
                std::string type_name;
                std::vector<types::TypePtr> type_args;
                if (base_type->is<types::NamedType>()) {
                    type_name = base_type->as<types::NamedType>().name;
                    type_args = base_type->as<types::NamedType>().type_args;
                } else if (base_type->is<types::RefType>()) {
                    auto inner = base_type->as<types::RefType>().inner;
                    if (inner && inner->is<types::NamedType>()) {
                        type_name = inner->as<types::NamedType>().name;
                        type_args = inner->as<types::NamedType>().type_args;
                    }
                }

                if (!type_name.empty()) {
                    auto struct_def = env_.lookup_struct(type_name);
                    int field_idx = -1;
                    if (struct_def) {
                        for (size_t i = 0; i < struct_def->fields.size(); ++i) {
                            if (struct_def->fields[i].first == field_expr.field) {
                                field_idx = static_cast<int>(i);
                                break;
                            }
                        }
                    }

                    if (field_idx >= 0) {
                        std::string llvm_struct_type = llvm_type_name(type_name);
                        if (!type_args.empty()) {
                            llvm_struct_type =
                                "%struct." + mangle_struct_name(type_name, type_args);
                        }

                        std::string field_ptr = fresh_reg();
                        emit_line("  " + field_ptr + " = getelementptr " + llvm_struct_type +
                                  ", ptr " + struct_ptr + ", i32 0, i32 " +
                                  std::to_string(field_idx));
                        last_expr_type_ = "ptr";
                        return field_ptr;
                    }
                }
            }
        }
        report_error("Can only take reference of variables", unary.span);
        last_expr_type_ = "ptr";
        return "null";
    }

    // Handle deref - load from pointer
    if (unary.op == parser::UnaryOp::Deref) {
        std::string ptr = gen_expr(*unary.operand);
        std::string result = fresh_reg();

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

        emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + ptr);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // Handle postfix increment (i++)
    if (unary.op == parser::UnaryOp::Inc) {
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;
                // Load current value
                std::string old_val = fresh_reg();
                emit_line("  " + old_val + " = load " + var.type + ", ptr " + var.reg);
                // Add 1
                std::string new_val = fresh_reg();
                emit_line("  " + new_val + " = add " + var.type + " " + old_val + ", 1");
                // Store new value
                emit_line("  store " + var.type + " " + new_val + ", ptr " + var.reg);
                // Return old value (postfix semantics)
                return old_val;
            }
        }
        report_error("Can only increment variables", unary.span);
        return "0";
    }

    // Handle postfix decrement (i--)
    if (unary.op == parser::UnaryOp::Dec) {
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;
                // Load current value
                std::string old_val = fresh_reg();
                emit_line("  " + old_val + " = load " + var.type + ", ptr " + var.reg);
                // Subtract 1
                std::string new_val = fresh_reg();
                emit_line("  " + new_val + " = sub " + var.type + " " + old_val + ", 1");
                // Store new value
                emit_line("  store " + var.type + " " + new_val + ", ptr " + var.reg);
                // Return old value (postfix semantics)
                return old_val;
            }
        }
        report_error("Can only decrement variables", unary.span);
        return "0";
    }

    std::string operand = gen_expr(*unary.operand);
    std::string operand_type = last_expr_type_;
    std::string result = fresh_reg();

    switch (unary.op) {
    case parser::UnaryOp::Neg:
        if (operand_type == "double" || operand_type == "float") {
            emit_line("  " + result + " = fsub double 0.0, " + operand);
            last_expr_type_ = "double";
        } else {
            emit_line("  " + result + " = sub " + operand_type + " 0, " + operand);
            last_expr_type_ = operand_type;
        }
        break;
    case parser::UnaryOp::Not:
        emit_line("  " + result + " = xor i1 " + operand + ", 1");
        last_expr_type_ = "i1";
        break;
    case parser::UnaryOp::BitNot:
        emit_line("  " + result + " = xor i32 " + operand + ", -1");
        last_expr_type_ = "i32";
        break;
    default:
        return operand;
    }

    return result;
}

} // namespace tml::codegen
