//! # LLVM IR Generator - Type Casts
//!
//! This file implements type cast code generation.
//!
//! ## Cast Operations
//!
//! | From → To        | LLVM Instruction              |
//! |------------------|-------------------------------|
//! | int → int (wider)| `sext` or `zext`              |
//! | int → int (narrow)| `trunc`                      |
//! | int → float      | `sitofp` or `uitofp`          |
//! | float → int      | `fptosi` or `fptoui`          |
//! | float → float    | `fpext` or `fptrunc`          |
//! | ptr → ptr        | `bitcast` (opaque ptrs: noop) |
//! | int → bool       | `icmp ne 0`                   |
//!
//! ## TML Cast Syntax
//!
//! ```tml
//! let x = value as I64
//! ```

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_cast(const parser::CastExpr& cast) -> std::string {
    // Generate the source expression
    std::string src = gen_expr(*cast.expr);
    std::string src_type = last_expr_type_;
    bool src_is_unsigned = last_expr_is_unsigned_;

    // Get the target type
    std::string target_type = llvm_type_ptr(cast.target);

    // If types are the same, just return the source
    if (src_type == target_type) {
        return src;
    }

    std::string result = fresh_reg();

    // Helper to get bit width
    auto get_bit_width = [](const std::string& ty) -> int {
        if (ty == "i8")
            return 8;
        if (ty == "i16")
            return 16;
        if (ty == "i32")
            return 32;
        if (ty == "i64")
            return 64;
        if (ty == "i128")
            return 128;
        return 0;
    };

    // Integer type check
    auto is_int_type = [](const std::string& ty) -> bool {
        return ty == "i8" || ty == "i16" || ty == "i32" || ty == "i64" || ty == "i128";
    };

    // Integer conversions with proper bit-width comparison
    if (is_int_type(src_type) && is_int_type(target_type)) {
        int src_bits = get_bit_width(src_type);
        int target_bits = get_bit_width(target_type);

        if (src_bits < target_bits) {
            // Widening: use zext for unsigned, sext for signed
            if (src_is_unsigned) {
                emit_line("  " + result + " = zext " + src_type + " " + src + " to " + target_type);
            } else {
                emit_line("  " + result + " = sext " + src_type + " " + src + " to " + target_type);
            }
            last_expr_type_ = target_type;
            return result;
        } else if (src_bits > target_bits) {
            // Narrowing: use trunc
            emit_line("  " + result + " = trunc " + src_type + " " + src + " to " + target_type);
            last_expr_type_ = target_type;
            return result;
        }
        // Same width: fall through (shouldn't happen, checked earlier)
    }

    // Float to int conversions
    if ((src_type == "double" || src_type == "float") &&
        (target_type == "i64" || target_type == "i32" || target_type == "i16" ||
         target_type == "i8")) {
        emit_line("  " + result + " = fptosi " + src_type + " " + src + " to " + target_type);
        last_expr_type_ = target_type;
        return result;
    }

    // Int to float conversions
    if ((src_type == "i64" || src_type == "i32" || src_type == "i16" || src_type == "i8") &&
        (target_type == "double" || target_type == "float")) {
        // Use uitofp for unsigned, sitofp for signed
        if (src_is_unsigned) {
            emit_line("  " + result + " = uitofp " + src_type + " " + src + " to " + target_type);
        } else {
            emit_line("  " + result + " = sitofp " + src_type + " " + src + " to " + target_type);
        }
        last_expr_type_ = target_type;
        return result;
    }

    // Float widening (float to double)
    if (src_type == "float" && target_type == "double") {
        emit_line("  " + result + " = fpext float " + src + " to double");
        last_expr_type_ = "double";
        return result;
    }

    // Float narrowing (double to float)
    if (src_type == "double" && target_type == "float") {
        emit_line("  " + result + " = fptrunc double " + src + " to float");
        last_expr_type_ = "float";
        return result;
    }

    // Bool to int
    if (src_type == "i1" && (target_type == "i8" || target_type == "i16" || target_type == "i32" ||
                             target_type == "i64")) {
        emit_line("  " + result + " = zext i1 " + src + " to " + target_type);
        last_expr_type_ = target_type;
        return result;
    }

    // Int to bool (non-zero check)
    if ((src_type == "i8" || src_type == "i16" || src_type == "i32" || src_type == "i64") &&
        target_type == "i1") {
        emit_line("  " + result + " = icmp ne " + src_type + " " + src + ", 0");
        last_expr_type_ = "i1";
        return result;
    }

    // Pointer casts (ptr to ptr)
    if (src_type == "ptr" && target_type == "ptr") {
        last_expr_type_ = "ptr";
        return src; // No conversion needed in opaque pointer mode
    }

    // Int to pointer
    if ((src_type == "i64" || src_type == "i32") && target_type == "ptr") {
        std::string int_val = src;
        if (src_type == "i32") {
            std::string ext_reg = fresh_reg();
            emit_line("  " + ext_reg + " = zext i32 " + src + " to i64");
            int_val = ext_reg;
        }
        emit_line("  " + result + " = inttoptr i64 " + int_val + " to ptr");
        last_expr_type_ = "ptr";
        return result;
    }

    // Pointer to int
    if (src_type == "ptr" && (target_type == "i64" || target_type == "i32")) {
        std::string ptr_int = fresh_reg();
        emit_line("  " + ptr_int + " = ptrtoint ptr " + src + " to i64");
        if (target_type == "i64") {
            last_expr_type_ = "i64";
            return ptr_int;
        } else {
            emit_line("  " + result + " = trunc i64 " + ptr_int + " to i32");
            last_expr_type_ = "i32";
            return result;
        }
    }

    // Fallback: bitcast for same-size types
    emit_line("  ; Warning: unhandled cast from " + src_type + " to " + target_type);
    last_expr_type_ = target_type;
    return src; // Return source unchanged
}

auto LLVMIRGen::gen_is_check(const parser::IsExpr& is_expr) -> std::string {
    // Generate the expression to check
    std::string obj_ptr = gen_expr(*is_expr.expr);

    // Get the target type name
    std::string target_name;
    if (is_expr.target->is<parser::NamedType>()) {
        const auto& named = is_expr.target->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            target_name = named.path.segments.back();
        }
    }

    if (target_name.empty()) {
        report_error("Invalid type in 'is' expression", is_expr.span);
        last_expr_type_ = "i1";
        return "false";
    }

    // Check if target is a known class type
    auto target_class_def = env_.lookup_class(target_name);
    if (!target_class_def.has_value()) {
        // Not a class type - for non-class types, we just return false at runtime
        last_expr_type_ = "i1";
        return "false";
    }

    // Get the compile-time type of the expression
    types::TypePtr expr_type = infer_expr_type(*is_expr.expr);
    std::string expr_class_name;
    if (expr_type && expr_type->is<types::ClassType>()) {
        expr_class_name = expr_type->as<types::ClassType>().name;
    } else if (expr_type && expr_type->is<types::NamedType>()) {
        // Sometimes class types are stored as NamedType
        expr_class_name = expr_type->as<types::NamedType>().name;
    }

    if (!expr_class_name.empty()) {
        // Check if expression type is the same as or a subclass of target type
        std::string current = expr_class_name;
        while (!current.empty()) {
            if (current == target_name) {
                // Expression type IS the target type or a subclass of it
                // At compile time, we know this is always true
                last_expr_type_ = "i1";
                return "true";
            }
            // Move to parent class
            auto current_def = env_.lookup_class(current);
            if (current_def && current_def->base_class) {
                current = current_def->base_class.value();
            } else {
                break;
            }
        }
        // Expression type is not related to target type
        // At compile time, we know this is always false
        last_expr_type_ = "i1";
        return "false";
    }

    // Fallback: dynamic check using vtable comparison (exact match only)
    std::string result = fresh_reg();
    std::string vtable_ptr_ptr = fresh_reg();
    std::string obj_vtable = fresh_reg();

    // Load vtable pointer from object (first field)
    std::string class_type = "%class." + target_name;
    emit_line("  " + vtable_ptr_ptr + " = getelementptr " + class_type + ", ptr " + obj_ptr +
              ", i32 0, i32 0");
    emit_line("  " + obj_vtable + " = load ptr, ptr " + vtable_ptr_ptr);

    // Compare with target vtable
    std::string target_vtable = "@vtable." + target_name;
    emit_line("  " + result + " = icmp eq ptr " + obj_vtable + ", " + target_vtable);

    last_expr_type_ = "i1";
    return result;
}

} // namespace tml::codegen
