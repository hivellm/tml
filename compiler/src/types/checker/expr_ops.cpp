TML_MODULE("compiler")

//! # Type Checker - Expression Operators
//!
//! This file implements type checking for binary operators, unary operators,
//! field access, index expressions, and block expressions.

#include "common.hpp"
#include "lexer/token.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace tml::types {

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);
bool is_float_type(const TypePtr& type);
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

// Helper to check if a literal value is zero
static bool is_literal_zero(const parser::Expr& expr) {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral) {
            return lit.token.int_value().value == 0;
        }
        if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
            return lit.token.float_value().value == 0.0;
        }
    }
    return false;
}

auto TypeChecker::check_binary(const parser::BinaryExpr& binary) -> TypePtr {
    auto left = check_expr(*binary.left);

    // Auto-deref `ref`/`mut ref` operands in value context. A binary operator
    // reads through a reference (e.g. `count + 1` where `count: mut ref I64`
    // reads the pointee I64), mirroring unary deref (see check_unary) and the
    // assignment-through-mut-ref allowance below. Returns the resolved inner
    // type when `t` resolves to a RefType, otherwise `t` unchanged (PtrType is
    // left intact so genuine pointer arithmetic keeps its pointer result).
    auto deref_ref = [&](const TypePtr& t) -> TypePtr {
        TypePtr resolved = env_.resolve(t);
        if (resolved && resolved->is<RefType>()) {
            return resolved->as<RefType>().inner;
        }
        return t;
    };

    // For arithmetic AND comparison operators, propagate the left operand's
    // type as the expected type for the right operand. This allows unsuffixed
    // integer literals on the right-hand side to infer the correct type from
    // the left (e.g., `x * 3` where x is I32 → 3 infers as I32; `i <= 138`
    // where i is I64 → 138 infers as I64 — W3 in phase0p).
    //
    // Without this, unsuffixed literals in comparisons fall through to the
    // HIR-builder default (I32) and generate a useless `sext i32 → i64` at
    // codegen — see `.sandbox/w3_repro.tml` for the exact pre-fix IR shape.
    //
    // Bitwise/shift operators also propagate left type for the same reason.
    // Logical `and`/`or` operate on Bool and don't need propagation.
    TypePtr right;
    switch (binary.op) {
    case parser::BinaryOp::Add:
    case parser::BinaryOp::Sub:
    case parser::BinaryOp::Mul:
    case parser::BinaryOp::Div:
    case parser::BinaryOp::Mod:
    case parser::BinaryOp::Lt:
    case parser::BinaryOp::Le:
    case parser::BinaryOp::Gt:
    case parser::BinaryOp::Ge:
    case parser::BinaryOp::Eq:
    case parser::BinaryOp::Ne:
    case parser::BinaryOp::BitAnd:
    case parser::BinaryOp::BitOr:
    case parser::BinaryOp::BitXor:
    case parser::BinaryOp::Shl:
    case parser::BinaryOp::Shr:
        // Propagate the DEREF'd inner type when `left` is a ref, so a bare
        // literal RHS infers the pointee type (I64), not `mut ref I64`.
        right = check_expr(*binary.right, deref_ref(left));
        break;
    default:
        right = check_expr(*binary.right);
        break;
    }

    auto check_binary_types = [&](const char* op_name) {
        // Compare through references: `mut ref I64` compares as `I64`.
        TypePtr resolved_left = env_.resolve(deref_ref(left));
        TypePtr resolved_right = env_.resolve(deref_ref(right));
        if (!types_compatible(resolved_left, resolved_right)) {
            error(std::string("Binary operator '") + op_name + "' requires matching types, found " +
                      type_to_string(resolved_left) + " and " + type_to_string(resolved_right),
                  binary.left->span);
        }
    };

    auto check_assignable = [&]() {
        if (binary.left->is<parser::IdentExpr>()) {
            const auto& ident = binary.left->as<parser::IdentExpr>();
            auto sym = env_.current_scope()->lookup(ident.name);
            if (sym && !sym->is_mutable) {
                // Allow assignment through mutable references (mut ref T)
                // Even if the variable itself isn't mutable, we can assign through it
                auto resolved = env_.resolve(sym->type);
                if (resolved && resolved->is<RefType>() && resolved->as<RefType>().is_mut) {
                    // This is a mutable reference, assignment through it is allowed
                    return;
                }
                error("Cannot assign to immutable variable '" + ident.name + "'", binary.left->span,
                      "T013");
            }
        }
    };

    switch (binary.op) {
    case parser::BinaryOp::Add: {
        // Pointer arithmetic: ptr + int = ptr
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (resolved_left && resolved_left->is<PtrType>()) {
            // ptr + int is valid pointer arithmetic
            if (is_integer_type(resolved_right)) {
                return left; // Result is the same pointer type
            }
        }
        check_binary_types("+");
        return deref_ref(left);
    }
    case parser::BinaryOp::Sub: {
        // Pointer arithmetic: ptr - int = ptr, ptr - ptr = int
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (resolved_left && resolved_left->is<PtrType>()) {
            if (is_integer_type(resolved_right)) {
                // ptr - int = ptr
                return left;
            }
            if (resolved_right && resolved_right->is<PtrType>()) {
                // ptr - ptr = I64 (pointer difference)
                return make_i64();
            }
        }
        check_binary_types("-");
        return deref_ref(left);
    }
    case parser::BinaryOp::Mul:
        check_binary_types("*");
        return deref_ref(left);
    case parser::BinaryOp::Div:
    case parser::BinaryOp::Mod: {
        // Check for division by zero literal
        if (is_literal_zero(*binary.right)) {
            error("Division by zero", binary.right->span, "T052");
        }
        check_binary_types(binary.op == parser::BinaryOp::Div ? "/" : "%");
        return deref_ref(left);
    }
    case parser::BinaryOp::Lt:
    case parser::BinaryOp::Le:
    case parser::BinaryOp::Gt:
    case parser::BinaryOp::Ge:
    case parser::BinaryOp::Eq:
    case parser::BinaryOp::Ne:
        check_binary_types("comparison");
        return make_bool();
    case parser::BinaryOp::And:
    case parser::BinaryOp::Or:
        return make_bool();
    case parser::BinaryOp::BitAnd:
    case parser::BinaryOp::BitOr:
    case parser::BinaryOp::BitXor:
    case parser::BinaryOp::Shl:
    case parser::BinaryOp::Shr:
        return deref_ref(left);
    case parser::BinaryOp::Assign: {
        check_assignable();
        // For assignment through mutable references, check if LHS is mut ref T
        // In that case, RHS should be compatible with T (the inner type)
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (resolved_left && resolved_left->is<RefType>() && resolved_left->as<RefType>().is_mut) {
            // Assigning through mut ref T - check RHS against inner type T
            TypePtr inner = env_.resolve(resolved_left->as<RefType>().inner);
            if (!types_compatible(inner, resolved_right)) {
                error(std::string("Cannot assign value of type ") + type_to_string(resolved_right) +
                          " through reference of type " + type_to_string(resolved_left),
                      binary.left->span);
            }
        } else {
            check_binary_types("=");
        }
        return make_unit();
    }
    case parser::BinaryOp::AddAssign:
    case parser::BinaryOp::SubAssign:
    case parser::BinaryOp::MulAssign:
    case parser::BinaryOp::BitAndAssign:
    case parser::BinaryOp::BitOrAssign:
    case parser::BinaryOp::BitXorAssign:
    case parser::BinaryOp::ShlAssign:
    case parser::BinaryOp::ShrAssign:
        check_assignable();
        return make_unit();
    case parser::BinaryOp::DivAssign:
    case parser::BinaryOp::ModAssign:
        // Check for division by zero literal
        if (is_literal_zero(*binary.right)) {
            error("Division by zero", binary.right->span, "T052");
        }
        check_assignable();
        return make_unit();
    }
    return make_unit();
}

auto TypeChecker::check_unary(const parser::UnaryExpr& unary) -> TypePtr {
    auto operand = check_expr(*unary.operand);

    switch (unary.op) {
    case parser::UnaryOp::Neg:
        return operand;
    case parser::UnaryOp::Not:
        return make_bool();
    case parser::UnaryOp::BitNot:
        return operand;
    case parser::UnaryOp::Ref:
        // In lowlevel blocks, & returns raw pointer (*T) instead of reference (ref T)
        if (in_lowlevel_) {
            if (operand->is<RefType>()) {
                return make_ptr(operand->as<RefType>().inner, false);
            }
            return make_ptr(operand, false);
        }
        // Reborrowing: ref (ref T) -> ref T (like Rust's automatic reborrow)
        if (operand->is<RefType>()) {
            return make_ref(operand->as<RefType>().inner, false);
        }
        return make_ref(operand, false);
    case parser::UnaryOp::RefMut:
        // In lowlevel blocks, &mut returns raw mutable pointer (*mut T)
        if (in_lowlevel_) {
            if (operand->is<RefType>()) {
                return make_ptr(operand->as<RefType>().inner, true);
            }
            return make_ptr(operand, true);
        }
        // Reborrowing: mut ref (mut ref T) -> mut ref T (like Rust's automatic reborrow)
        if (operand->is<RefType>() && operand->as<RefType>().is_mut) {
            return operand; // Already a mutable ref, just return it
        }
        // Allow reborrow from mutable to mutable
        if (operand->is<RefType>()) {
            return make_ref(operand->as<RefType>().inner, true);
        }
        return make_ref(operand, true);
    case parser::UnaryOp::Deref:
        if (operand->is<RefType>()) {
            return operand->as<RefType>().inner;
        }
        if (operand->is<PtrType>()) {
            return operand->as<PtrType>().inner;
        }
        // Handle NamedType cases for pointer and smart pointer types
        if (operand->is<NamedType>()) {
            const auto& named = operand->as<NamedType>();
            // Handle Ptr[T] which is stored as NamedType{name="Ptr", type_args=[T]}
            // This is common in generic contexts where Ptr[Node[T]] appears
            if (named.name == "Ptr" && !named.type_args.empty()) {
                return named.type_args[0];
            }
            // Handle smart pointer types that implement Deref behavior
            // Dereferencing these returns the inner type T
            static const std::unordered_set<std::string> deref_types = {
                "Arc",
                "Rc",
                "Box",
                "Heap",
                "Shared",
                "Sync",
                "MutexGuard",
                "RwLockReadGuard",
                "RwLockWriteGuard",
                "Ref",
                "RefMut",
            };
            if (deref_types.count(named.name) > 0 && !named.type_args.empty()) {
                return named.type_args[0];
            }
        }
        error("Cannot dereference non-reference type", unary.operand->span, "T017");
        return make_unit();
    case parser::UnaryOp::Inc:
    case parser::UnaryOp::Dec:
        return operand;
    }
    return make_unit();
}

auto TypeChecker::check_field_access(const parser::FieldExpr& field) -> TypePtr {
    // Handle static field access: ClassName.staticField
    if (field.object->is<parser::IdentExpr>()) {
        const auto& ident = field.object->as<parser::IdentExpr>();
        auto class_def = env_.lookup_class(ident.name);
        if (class_def.has_value()) {
            // Look for static field
            for (const auto& f : class_def->fields) {
                if (f.name == field.field && f.is_static) {
                    // Check visibility for static field access
                    if (!check_member_visibility(f.vis, ident.name, field.field,
                                                 field.object->span)) {
                        return f.type; // Return type for error recovery
                    }
                    return f.type;
                }
            }
            // If we're here, it might be a non-static field accessed statically (error)
            // Or the field doesn't exist - fall through to regular handling
        }
    }

    auto obj_type = check_expr(*field.object);

    if (obj_type->is<RefType>()) {
        obj_type = obj_type->as<RefType>().inner;
    }

    // Handle optional chaining on field access: expr?.field
    // The object type must be Maybe[T]. We look up the field on T and wrap in Maybe.
    if (field.optional_chain) {
        TypePtr inner_type;
        if (obj_type->is<NamedType>()) {
            auto& named = obj_type->as<NamedType>();
            if (named.name == "Maybe" && !named.type_args.empty()) {
                inner_type = named.type_args[0];
            }
        }
        if (!inner_type) {
            error("Optional chaining `?.` requires receiver of type Maybe[T], got " +
                      type_to_string(obj_type),
                  field.span, "T090");
            return make_unit();
        }

        // Look up the field on the inner type
        TypePtr field_type;
        TypePtr check_inner = inner_type;
        if (check_inner->is<RefType>()) {
            check_inner = check_inner->as<RefType>().inner;
        }
        if (check_inner->is<NamedType>()) {
            auto& inner_named = check_inner->as<NamedType>();
            auto struct_def = env_.lookup_struct(inner_named.name);
            if (struct_def) {
                std::unordered_map<std::string, TypePtr> subs;
                if (!struct_def->type_params.empty() && !inner_named.type_args.empty()) {
                    for (size_t i = 0;
                         i < struct_def->type_params.size() && i < inner_named.type_args.size();
                         ++i) {
                        subs[struct_def->type_params[i]] = inner_named.type_args[i];
                    }
                }
                for (const auto& fld : struct_def->fields) {
                    if (fld.name == field.field) {
                        field_type = subs.empty() ? fld.type : substitute_type(fld.type, subs);
                        break;
                    }
                }
            }
        }

        if (!field_type) {
            error("No field '" + field.field + "' found on type " + type_to_string(inner_type) +
                      " (from optional chaining on " + type_to_string(obj_type) + ")",
                  field.span, "T074");
            return make_unit();
        }

        // Flatten: if the field is already Maybe[V], return Maybe[V]
        if (field_type->is<NamedType>() && field_type->as<NamedType>().name == "Maybe") {
            return field_type;
        }
        // Wrap in Maybe[FieldType]
        return std::make_shared<Type>(Type{NamedType{"Maybe", "", {field_type}}});
    }

    // Handle class type field access with visibility checking
    if (obj_type->is<ClassType>()) {
        auto& class_type = obj_type->as<ClassType>();
        // Search for the field in this class and its parent classes
        std::string current_class = class_type.name;
        while (!current_class.empty()) {
            auto current_def = env_.lookup_class(current_class);
            if (!current_def.has_value())
                break;

            // Look for the field in this class
            for (const auto& f : current_def->fields) {
                if (f.name == field.field) {
                    // Check visibility (defining class is current_class, not class_type.name)
                    if (!check_member_visibility(f.vis, current_class, field.field,
                                                 field.object->span)) {
                        return f.type; // Return type anyway for error recovery
                    }
                    return f.type;
                }
            }

            // Check parent class
            if (current_def->base_class.has_value()) {
                current_class = current_def->base_class.value();
            } else {
                break;
            }
        }
        error("Unknown field: " + field.field + " on class " + class_type.name, field.object->span,
              "T073");
    }

    if (obj_type->is<NamedType>()) {
        auto& named = obj_type->as<NamedType>();

        // First check if this is a class (NamedType can refer to classes too)
        auto class_def = env_.lookup_class(named.name);
        if (class_def.has_value()) {
            // Search for the field in this class and its parent classes
            std::string current_class = named.name;
            while (!current_class.empty()) {
                auto current_def = env_.lookup_class(current_class);
                if (!current_def.has_value())
                    break;

                // Look for the field in this class
                for (const auto& f : current_def->fields) {
                    if (f.name == field.field) {
                        // Check visibility
                        if (!check_member_visibility(f.vis, current_class, field.field,
                                                     field.object->span)) {
                            return f.type; // Return type anyway for error recovery
                        }
                        return f.type;
                    }
                }

                // Check parent class
                if (current_def->base_class.has_value()) {
                    current_class = current_def->base_class.value();
                } else {
                    break;
                }
            }
            error("Unknown field: " + field.field + " on class " + named.name, field.object->span,
                  "T073");
            return make_unit();
        }

        // Handle Ptr[T] - dereference through to inner type for field access
        // This allows (*ptr).field syntax to work by auto-dereferencing Ptr[T] to T
        if (named.name == "Ptr" && !named.type_args.empty()) {
            auto inner_type = named.type_args[0];
            if (inner_type->is<NamedType>()) {
                auto& inner_named = inner_type->as<NamedType>();
                auto inner_struct = env_.lookup_struct(inner_named.name);
                if (inner_struct) {
                    std::unordered_map<std::string, TypePtr> inner_subs;
                    if (!inner_struct->type_params.empty() && !inner_named.type_args.empty()) {
                        for (size_t i = 0; i < inner_struct->type_params.size() &&
                                           i < inner_named.type_args.size();
                             ++i) {
                            inner_subs[inner_struct->type_params[i]] = inner_named.type_args[i];
                        }
                    }
                    for (const auto& fld : inner_struct->fields) {
                        if (fld.name == field.field) {
                            if (!inner_subs.empty()) {
                                return substitute_type(fld.type, inner_subs);
                            }
                            return fld.type;
                        }
                    }
                    error("Unknown field: " + field.field + " on Ptr[" + inner_named.name + "]",
                          field.object->span, "T074");
                    return make_unit();
                }
            }
        }

        // Otherwise check if it's a struct
        auto struct_def = env_.lookup_struct(named.name);
        if (struct_def) {
            std::unordered_map<std::string, TypePtr> subs;
            if (!struct_def->type_params.empty() && !named.type_args.empty()) {
                for (size_t i = 0; i < struct_def->type_params.size() && i < named.type_args.size();
                     ++i) {
                    subs[struct_def->type_params[i]] = named.type_args[i];
                }
            }

            for (const auto& fld : struct_def->fields) {
                if (fld.name == field.field) {
                    if (!subs.empty()) {
                        return substitute_type(fld.type, subs);
                    }
                    return fld.type;
                }
            }

            // Deref coercion: if field not found and type implements Deref, try inner type
            // This handles smart pointers like Arc[T], Box[T], MutexGuard[T], etc.
            static const std::unordered_set<std::string> deref_types = {
                "Arc",
                "Rc",
                "Box",
                "Heap",
                "Shared",
                "Sync",
                "MutexGuard",
                "RwLockReadGuard",
                "RwLockWriteGuard",
                "Ref",
                "RefMut",
                "Ptr", // Allow (*ptr).field to access fields through Ptr[T]
            };

            if (deref_types.count(named.name) > 0 && !named.type_args.empty()) {
                // Get the inner type (T in Arc[T])
                auto inner_type = named.type_args[0];

                // Recursively look up field on the inner type
                if (inner_type->is<NamedType>()) {
                    auto& inner_named = inner_type->as<NamedType>();
                    auto inner_struct = env_.lookup_struct(inner_named.name);
                    if (inner_struct) {
                        std::unordered_map<std::string, TypePtr> inner_subs;
                        if (!inner_struct->type_params.empty() && !inner_named.type_args.empty()) {
                            for (size_t i = 0; i < inner_struct->type_params.size() &&
                                               i < inner_named.type_args.size();
                                 ++i) {
                                inner_subs[inner_struct->type_params[i]] = inner_named.type_args[i];
                            }
                        }

                        for (const auto& fld : inner_struct->fields) {
                            if (fld.name == field.field) {
                                if (!inner_subs.empty()) {
                                    return substitute_type(fld.type, inner_subs);
                                }
                                return fld.type;
                            }
                        }
                    }
                }
            }

            error("Unknown field: " + field.field, field.object->span, "T005");
        }
    }

    if (obj_type->is<TupleType>()) {
        auto& tuple = obj_type->as<TupleType>();
        try {
            size_t idx = std::stoul(field.field);
            if (idx < tuple.elements.size()) {
                return tuple.elements[idx];
            }
        } catch (...) {}
        error("Invalid tuple field: " + field.field, field.object->span, "T036");
    }

    return make_unit();
}

auto TypeChecker::check_index(const parser::IndexExpr& idx) -> TypePtr {
    auto obj_type = check_expr(*idx.object);
    check_expr(*idx.index);

    // Resolve the type in case it's a type alias
    auto resolved = env_.resolve(obj_type);

    if (resolved->is<ArrayType>()) {
        return resolved->as<ArrayType>().element;
    }
    if (resolved->is<SliceType>()) {
        return resolved->as<SliceType>().element;
    }

    return make_unit();
}

auto TypeChecker::check_block(const parser::BlockExpr& block) -> TypePtr {
    TML_DEBUG_LN("[check_block] Entering block with " << block.stmts.size() << " statements");
    env_.push_scope();
    TypePtr result = make_unit();

    // Save and reset the returned_in_block_ flag for this block scope
    bool saved_returned = returned_in_block_;
    returned_in_block_ = false;

    for (const auto& stmt : block.stmts) {
        // S016: Warn if code follows a return in the same block
        if (returned_in_block_) {
            warning("Unreachable code after return", stmt->span, "S016");
            // Still type-check for better error reporting, but don't keep looping warnings
            result = check_stmt(*stmt);
            continue;
        }

        TML_DEBUG_LN("[check_block] Checking statement at index " << stmt->kind.index());
        result = check_stmt(*stmt);

        // Check if this statement was a return (ExprStmt containing ReturnExpr)
        if (std::holds_alternative<parser::ExprStmt>(stmt->kind)) {
            const auto& expr_stmt = std::get<parser::ExprStmt>(stmt->kind);
            if (expr_stmt.expr && expr_stmt.expr->is<parser::ReturnExpr>()) {
                returned_in_block_ = true;
            }
        }
    }

    if (block.expr) {
        if (returned_in_block_) {
            warning("Unreachable code after return", (*block.expr)->span, "S016");
        }
        TML_DEBUG_LN("[check_block] Checking trailing expression");
        // Pass expected return type for implicit returns (array literal inference)
        result = check_expr(**block.expr, current_return_type_);
    }

    // S014: Check for unused local variables before the scope is popped
    for (const auto& [name, sym] : env_.current_scope()->symbols()) {
        if (!name.empty() && name[0] != '_') {
            if (read_vars_.find(name) == read_vars_.end()) {
                warning("Unused variable '" + name + "'", sym.span, "S014");
            }
        }
    }

    // Restore parent block's returned state
    returned_in_block_ = saved_returned;

    env_.pop_scope();
    TML_DEBUG_LN("[check_block] Exiting block");
    return result;
}

} // namespace tml::types
