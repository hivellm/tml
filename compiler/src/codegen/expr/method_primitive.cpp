//! # LLVM IR Generator - Primitive Type Methods
//!
//! This file implements method calls on primitive types.
//!
//! ## Integer Methods
//!
//! | Method       | Description              |
//! |--------------|--------------------------|
//! | `add`, `sub` | Arithmetic with overflow |
//! | `mul`, `div` | Multiplication, division |
//! | `to_string`  | Convert to string        |
//! | `hash`       | Hash value               |
//! | `cmp`        | Compare, returns Ordering|
//! | `abs`        | Absolute value           |
//!
//! ## Float Methods
//!
//! | Method    | Description         |
//! |-----------|---------------------|
//! | `sqrt`    | Square root         |
//! | `floor`   | Round down          |
//! | `ceil`    | Round up            |
//! | `round`   | Round to nearest    |
//! | `to_string` | Convert to string |
//!
//! ## Bool Methods
//!
//! | Method      | Description |
//! |-------------|-------------|
//! | `to_string` | "true"/"false" |

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_primitive_method(const parser::MethodCallExpr& call,
                                     const std::string& receiver, const std::string& receiver_ptr,
                                     types::TypePtr receiver_type) -> std::optional<std::string> {
    // Apply type substitutions to handle generic types (e.g., T -> I32)
    if (!current_type_subs_.empty() && receiver_type) {
        receiver_type = apply_type_substitutions(receiver_type, current_type_subs_);
    }

    // Unwrap reference type if present
    types::TypePtr inner_type = receiver_type;
    if (receiver_type && receiver_type->is<types::RefType>()) {
        inner_type = receiver_type->as<types::RefType>().inner;
    }

    if (!inner_type || !inner_type->is<types::PrimitiveType>()) {
        return std::nullopt;
    }

    const std::string& method = call.method;
    const auto& prim = inner_type->as<types::PrimitiveType>();
    auto kind = prim.kind;

    bool is_integer = (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
                       kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
                       kind == types::PrimitiveKind::I128 || kind == types::PrimitiveKind::U8 ||
                       kind == types::PrimitiveKind::U16 || kind == types::PrimitiveKind::U32 ||
                       kind == types::PrimitiveKind::U64 || kind == types::PrimitiveKind::U128);
    bool is_signed = (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
                      kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
                      kind == types::PrimitiveKind::I128);
    bool is_float = (kind == types::PrimitiveKind::F32 || kind == types::PrimitiveKind::F64);

    std::string llvm_ty = llvm_type_from_semantic(receiver_type);

    // Arithmetic operations
    if (is_integer || is_float) {
        if (method == "add") {
            emit_coverage("Add::add");
            if (call.args.empty()) {
                report_error("add() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fadd " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + result + " = add " + llvm_ty + " " + receiver + ", " + other);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        if (method == "sub") {
            emit_coverage("Sub::sub");
            if (call.args.empty()) {
                report_error("sub() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fsub " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + result + " = sub " + llvm_ty + " " + receiver + ", " + other);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        if (method == "mul") {
            emit_coverage("Mul::mul");
            if (call.args.empty()) {
                report_error("mul() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fmul " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + result + " = mul " + llvm_ty + " " + receiver + ", " + other);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        if (method == "div") {
            emit_coverage("Div::div");
            if (call.args.empty()) {
                report_error("div() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fdiv " + llvm_ty + " " + receiver + ", " + other);
            } else if (is_signed) {
                emit_line("  " + result + " = sdiv " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + result + " = udiv " + llvm_ty + " " + receiver + ", " + other);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        if (method == "rem" && is_integer) {
            emit_coverage("Rem::rem");
            if (call.args.empty()) {
                report_error("rem() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_signed) {
                emit_line("  " + result + " = srem " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + result + " = urem " + llvm_ty + " " + receiver + ", " + other);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        if (method == "neg") {
            emit_coverage("Neg::neg");
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fneg " + llvm_ty + " " + receiver);
            } else {
                emit_line("  " + result + " = sub " + llvm_ty + " 0, " + receiver);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        // Comparison methods
        if (method == "cmp") {
            emit_coverage("Ord::cmp");
            if (call.args.empty()) {
                report_error("cmp() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other_ptr = gen_expr(*call.args[0]);
            std::string other = fresh_reg();
            emit_line("  " + other + " = load " + llvm_ty + ", ptr " + other_ptr);

            std::string cmp_lt = fresh_reg();
            std::string cmp_eq = fresh_reg();
            if (is_float) {
                emit_line("  " + cmp_lt + " = fcmp olt " + llvm_ty + " " + receiver + ", " + other);
                emit_line("  " + cmp_eq + " = fcmp oeq " + llvm_ty + " " + receiver + ", " + other);
            } else if (is_signed) {
                emit_line("  " + cmp_lt + " = icmp slt " + llvm_ty + " " + receiver + ", " + other);
                emit_line("  " + cmp_eq + " = icmp eq " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + cmp_lt + " = icmp ult " + llvm_ty + " " + receiver + ", " + other);
                emit_line("  " + cmp_eq + " = icmp eq " + llvm_ty + " " + receiver + ", " + other);
            }
            std::string sel1 = fresh_reg();
            emit_line("  " + sel1 + " = select i1 " + cmp_eq + ", i32 1, i32 2");
            std::string tag = fresh_reg();
            emit_line("  " + tag + " = select i1 " + cmp_lt + ", i32 0, i32 " + sel1);
            std::string result = fresh_reg();
            emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + tag + ", 0");
            last_expr_type_ = "%struct.Ordering";
            return result;
        }

        // partial_cmp - returns Maybe[Ordering] (Just(ordering) for numeric types)
        if (method == "partial_cmp") {
            emit_coverage("PartialOrd::partial_cmp");
            if (call.args.empty()) {
                report_error("partial_cmp() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other_ptr = gen_expr(*call.args[0]);
            std::string other = fresh_reg();
            emit_line("  " + other + " = load " + llvm_ty + ", ptr " + other_ptr);

            // Compute comparison like cmp
            std::string cmp_lt = fresh_reg();
            std::string cmp_eq = fresh_reg();
            if (is_float) {
                emit_line("  " + cmp_lt + " = fcmp olt " + llvm_ty + " " + receiver + ", " + other);
                emit_line("  " + cmp_eq + " = fcmp oeq " + llvm_ty + " " + receiver + ", " + other);
            } else if (is_signed) {
                emit_line("  " + cmp_lt + " = icmp slt " + llvm_ty + " " + receiver + ", " + other);
                emit_line("  " + cmp_eq + " = icmp eq " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + cmp_lt + " = icmp ult " + llvm_ty + " " + receiver + ", " + other);
                emit_line("  " + cmp_eq + " = icmp eq " + llvm_ty + " " + receiver + ", " + other);
            }
            std::string sel1 = fresh_reg();
            emit_line("  " + sel1 + " = select i1 " + cmp_eq + ", i32 1, i32 2");
            std::string tag = fresh_reg();
            emit_line("  " + tag + " = select i1 " + cmp_lt + ", i32 0, i32 " + sel1);

            // Build Ordering on stack
            std::string ordering_alloca = fresh_reg();
            emit_line("  " + ordering_alloca + " = alloca %struct.Ordering, align 4");
            std::string ordering_tag_ptr = fresh_reg();
            emit_line("  " + ordering_tag_ptr + " = getelementptr inbounds %struct.Ordering, ptr " +
                      ordering_alloca + ", i32 0, i32 0");
            emit_line("  store i32 " + tag + ", ptr " + ordering_tag_ptr);
            std::string ordering = fresh_reg();
            emit_line("  " + ordering + " = load %struct.Ordering, ptr " + ordering_alloca);

            // Build Maybe[Ordering] = Just(ordering)
            // Maybe[Ordering] layout: { i32 tag, %struct.Ordering payload }
            // tag 0 = Just, tag 1 = Nothing
            std::string maybe_type = "%struct.Maybe__Ordering";
            std::string maybe_alloca = fresh_reg();
            emit_line("  " + maybe_alloca + " = alloca " + maybe_type + ", align 8");

            // Set tag to 0 (Just)
            std::string maybe_tag_ptr = fresh_reg();
            emit_line("  " + maybe_tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_alloca + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + maybe_tag_ptr);

            // Set payload
            std::string payload_ptr = fresh_reg();
            emit_line("  " + payload_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_alloca + ", i32 0, i32 1");
            emit_line("  store %struct.Ordering " + ordering + ", ptr " + payload_ptr);

            // Load final result
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + maybe_type + ", ptr " + maybe_alloca);
            last_expr_type_ = maybe_type;
            return result;
        }

        if (method == "max") {
            emit_coverage("Ord::max");
            if (call.args.empty()) {
                report_error("max() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other = gen_expr(*call.args[0]);
            std::string cmp = fresh_reg();
            if (is_float) {
                emit_line("  " + cmp + " = fcmp ogt " + llvm_ty + " " + receiver + ", " + other);
            } else if (is_signed) {
                emit_line("  " + cmp + " = icmp sgt " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + cmp + " = icmp ugt " + llvm_ty + " " + receiver + ", " + other);
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + cmp + ", " + llvm_ty + " " + receiver +
                      ", " + llvm_ty + " " + other);
            last_expr_type_ = llvm_ty;
            return result;
        }

        if (method == "min") {
            emit_coverage("Ord::min");
            if (call.args.empty()) {
                report_error("min() requires an argument", call.span, "C008");
                return "0";
            }
            std::string other = gen_expr(*call.args[0]);
            std::string cmp = fresh_reg();
            if (is_float) {
                emit_line("  " + cmp + " = fcmp olt " + llvm_ty + " " + receiver + ", " + other);
            } else if (is_signed) {
                emit_line("  " + cmp + " = icmp slt " + llvm_ty + " " + receiver + ", " + other);
            } else {
                emit_line("  " + cmp + " = icmp ult " + llvm_ty + " " + receiver + ", " + other);
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + cmp + ", " + llvm_ty + " " + receiver +
                      ", " + llvm_ty + " " + other);
            last_expr_type_ = llvm_ty;
            return result;
        }

        // abs() -> Self (absolute value for signed integers)
        if (method == "abs" && is_signed) {
            emit_coverage("I32::abs");
            // if this < 0 { 0 - this } else { this }
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp slt " + llvm_ty + " " + receiver + ", 0");
            std::string neg = fresh_reg();
            emit_line("  " + neg + " = sub " + llvm_ty + " 0, " + receiver);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + cmp + ", " + llvm_ty + " " + neg + ", " +
                      llvm_ty + " " + receiver);
            last_expr_type_ = llvm_ty;
            return result;
        }

        // signum() -> Self (sign: -1, 0, or 1)
        if (method == "signum" && is_signed) {
            emit_coverage("I32::signum");
            // if this > 0 { 1 } else if this < 0 { -1 } else { 0 }
            std::string cmp_pos = fresh_reg();
            emit_line("  " + cmp_pos + " = icmp sgt " + llvm_ty + " " + receiver + ", 0");
            std::string cmp_neg = fresh_reg();
            emit_line("  " + cmp_neg + " = icmp slt " + llvm_ty + " " + receiver + ", 0");
            std::string neg_one = fresh_reg();
            emit_line("  " + neg_one + " = select i1 " + cmp_neg + ", " + llvm_ty + " -1, " +
                      llvm_ty + " 0");
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + cmp_pos + ", " + llvm_ty + " 1, " +
                      llvm_ty + " " + neg_one);
            last_expr_type_ = llvm_ty;
            return result;
        }

        // is_positive() -> Bool (this > 0)
        if (method == "is_positive" && is_signed) {
            emit_coverage("I32::is_positive");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp sgt " + llvm_ty + " " + receiver + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // is_negative() -> Bool (this < 0)
        if (method == "is_negative" && is_signed) {
            emit_coverage("I32::is_negative");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp slt " + llvm_ty + " " + receiver + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // pow(exp) -> Self (integer power)
        if (method == "pow" && is_integer) {
            emit_coverage("I32::pow");
            if (call.args.empty()) {
                report_error("pow() requires an exponent argument", call.span, "C008");
                return "0";
            }
            std::string exp = gen_expr(*call.args[0]);
            // Use runtime function for integer power
            // Convert to double, use float_pow, convert back
            std::string double_base = fresh_reg();
            if (is_signed) {
                emit_line("  " + double_base + " = sitofp " + llvm_ty + " " + receiver +
                          " to double");
            } else {
                emit_line("  " + double_base + " = uitofp " + llvm_ty + " " + receiver +
                          " to double");
            }
            std::string i32_exp = exp;
            if (last_expr_type_ == "i64") {
                i32_exp = fresh_reg();
                emit_line("  " + i32_exp + " = trunc i64 " + exp + " to i32");
            } else if (last_expr_type_ != "i32") {
                i32_exp = fresh_reg();
                emit_line("  " + i32_exp + " = sext " + last_expr_type_ + " " + exp + " to i32");
            }
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @float_pow(double " + double_base +
                      ", i32 " + i32_exp + ")");
            std::string result = fresh_reg();
            if (is_signed) {
                emit_line("  " + result + " = fptosi double " + double_result + " to " + llvm_ty);
            } else {
                emit_line("  " + result + " = fptoui double " + double_result + " to " + llvm_ty);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        // ============ Bit Assign Operations (mut this methods) ============
        // These methods mutate the receiver and return void

        // bitand_assign(rhs) - this = this & rhs
        if (method == "bitand_assign" && !receiver_ptr.empty()) {
            emit_coverage("BitAndAssign::bitand_assign");
            if (call.args.empty()) {
                report_error("bitand_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + llvm_ty + " " + receiver + ", " + rhs);
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // bitor_assign(rhs) - this = this | rhs
        if (method == "bitor_assign" && !receiver_ptr.empty()) {
            emit_coverage("BitOrAssign::bitor_assign");
            if (call.args.empty()) {
                report_error("bitor_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = or " + llvm_ty + " " + receiver + ", " + rhs);
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // bitxor_assign(rhs) - this = this ^ rhs
        if (method == "bitxor_assign" && !receiver_ptr.empty()) {
            emit_coverage("BitXorAssign::bitxor_assign");
            if (call.args.empty()) {
                report_error("bitxor_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + llvm_ty + " " + receiver + ", " + rhs);
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // shl_assign(rhs) - this = this << rhs
        if (method == "shl_assign" && !receiver_ptr.empty()) {
            emit_coverage("ShlAssign::shl_assign");
            if (call.args.empty()) {
                report_error("shl_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string rhs_type = last_expr_type_;
            // Ensure shift amount matches receiver type
            std::string rhs_final = rhs;
            if (rhs_type != llvm_ty) {
                rhs_final = fresh_reg();
                // Determine if we need to extend or truncate
                int rhs_bits = (rhs_type == "i8")    ? 8
                               : (rhs_type == "i16") ? 16
                               : (rhs_type == "i32") ? 32
                               : (rhs_type == "i64") ? 64
                                                     : 128;
                int llvm_bits = (llvm_ty == "i8")    ? 8
                                : (llvm_ty == "i16") ? 16
                                : (llvm_ty == "i32") ? 32
                                : (llvm_ty == "i64") ? 64
                                                     : 128;
                if (rhs_bits > llvm_bits) {
                    emit_line("  " + rhs_final + " = trunc " + rhs_type + " " + rhs + " to " +
                              llvm_ty);
                } else {
                    emit_line("  " + rhs_final + " = zext " + rhs_type + " " + rhs + " to " +
                              llvm_ty);
                }
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl " + llvm_ty + " " + receiver + ", " + rhs_final);
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // shr_assign(rhs) - this = this >> rhs (arithmetic for signed, logical for unsigned)
        if (method == "shr_assign" && !receiver_ptr.empty()) {
            emit_coverage("ShrAssign::shr_assign");
            if (call.args.empty()) {
                report_error("shr_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string rhs_type = last_expr_type_;
            // Ensure shift amount matches receiver type
            std::string rhs_final = rhs;
            if (rhs_type != llvm_ty) {
                rhs_final = fresh_reg();
                // Determine if we need to extend or truncate
                int rhs_bits = (rhs_type == "i8")    ? 8
                               : (rhs_type == "i16") ? 16
                               : (rhs_type == "i32") ? 32
                               : (rhs_type == "i64") ? 64
                                                     : 128;
                int llvm_bits = (llvm_ty == "i8")    ? 8
                                : (llvm_ty == "i16") ? 16
                                : (llvm_ty == "i32") ? 32
                                : (llvm_ty == "i64") ? 64
                                                     : 128;
                if (rhs_bits > llvm_bits) {
                    emit_line("  " + rhs_final + " = trunc " + rhs_type + " " + rhs + " to " +
                              llvm_ty);
                } else {
                    emit_line("  " + rhs_final + " = zext " + rhs_type + " " + rhs + " to " +
                              llvm_ty);
                }
            }
            std::string result = fresh_reg();
            if (is_signed) {
                emit_line("  " + result + " = ashr " + llvm_ty + " " + receiver + ", " + rhs_final);
            } else {
                emit_line("  " + result + " = lshr " + llvm_ty + " " + receiver + ", " + rhs_final);
            }
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }
    }

    // Bool operations
    if (kind == types::PrimitiveKind::Bool) {
        if (method == "negate") {
            emit_coverage("Not::negate");
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor i1 " + receiver + ", true");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // duplicate() -> Self (copy semantics for primitives)
    if (method == "duplicate") {
        emit_coverage("Duplicate::duplicate");
        last_expr_type_ = llvm_ty;
        return receiver;
    }

    // to_owned() -> Self
    if (method == "to_owned") {
        emit_coverage("ToOwned::to_owned");
        last_expr_type_ = llvm_ty;
        return receiver;
    }

    // borrow() -> ref Self
    if (method == "borrow") {
        emit_coverage("Borrow::borrow");
        if (!receiver_ptr.empty()) {
            last_expr_type_ = "ptr";
            return receiver_ptr;
        }
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + llvm_ty);
        emit_line("  store " + llvm_ty + " " + receiver + ", ptr " + tmp);
        last_expr_type_ = "ptr";
        return tmp;
    }

    // borrow_mut() -> mut ref Self
    if (method == "borrow_mut") {
        emit_coverage("BorrowMut::borrow_mut");
        if (!receiver_ptr.empty()) {
            last_expr_type_ = "ptr";
            return receiver_ptr;
        }
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + llvm_ty);
        emit_line("  store " + llvm_ty + " " + receiver + ", ptr " + tmp);
        last_expr_type_ = "ptr";
        return tmp;
    }

    // to_string() -> Str and debug_string() -> Str (same for primitives)
    if (method == "to_string" || method == "debug_string") {
        std::string trait_name = (method == "to_string") ? "Display" : "Debug";
        emit_coverage(trait_name + "::" + method);
        std::string result = fresh_reg();
        if (kind == types::PrimitiveKind::Bool) {
            std::string ext = fresh_reg();
            emit_line("  " + ext + " = zext i1 " + receiver + " to i32");
            emit_line("  " + result + " = call ptr @bool_to_string(i32 " + ext + ")");
        } else if (kind == types::PrimitiveKind::I32) {
            emit_line("  " + result + " = call ptr @i32_to_string(i32 " + receiver + ")");
        } else if (kind == types::PrimitiveKind::I64 || kind == types::PrimitiveKind::U64) {
            // I64 and U64 are both already i64 type
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + receiver + ")");
        } else if (kind == types::PrimitiveKind::F64) {
            emit_line("  " + result + " = call ptr @float_to_string(double " + receiver + ")");
        } else if (kind == types::PrimitiveKind::F32) {
            // Convert F32 to F64 first
            std::string ext = fresh_reg();
            emit_line("  " + ext + " = fpext float " + receiver + " to double");
            emit_line("  " + result + " = call ptr @float_to_string(double " + ext + ")");
        } else if (kind == types::PrimitiveKind::Str) {
            last_expr_type_ = "ptr";
            return receiver;
        } else if (kind == types::PrimitiveKind::Char) {
            // Convert Char (U32) to string
            emit_line("  " + result + " = call ptr @char_to_string(i32 " + receiver + ")");
        } else {
            // For other integer types, extend to i64
            std::string ext = fresh_reg();
            if (is_signed) {
                emit_line("  " + ext + " = sext " + llvm_ty + " " + receiver + " to i64");
            } else {
                emit_line("  " + ext + " = zext " + llvm_ty + " " + receiver + " to i64");
            }
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + ext + ")");
        }
        last_expr_type_ = "ptr";
        return result;
    }

    // hash() -> I64
    if (method == "hash") {
        emit_coverage("Hash::hash");
        std::string result = fresh_reg();
        if (kind == types::PrimitiveKind::Bool) {
            emit_line("  " + result + " = zext i1 " + receiver + " to i64");
        } else if (kind == types::PrimitiveKind::Str) {
            std::string hash32 = fresh_reg();
            emit_line("  " + hash32 + " = call i32 @str_hash(ptr " + receiver + ")");
            emit_line("  " + result + " = sext i32 " + hash32 + " to i64");
        } else if (is_integer) {
            std::string val64 = receiver;
            if (llvm_ty != "i64") {
                val64 = fresh_reg();
                if (is_signed) {
                    emit_line("  " + val64 + " = sext " + llvm_ty + " " + receiver + " to i64");
                } else {
                    emit_line("  " + val64 + " = zext " + llvm_ty + " " + receiver + " to i64");
                }
            }
            std::string xor_result = fresh_reg();
            emit_line("  " + xor_result + " = xor i64 " + val64 + ", 14695981039346656037");
            emit_line("  " + result + " = mul i64 " + xor_result + ", 1099511628211");
        } else if (is_float) {
            std::string bits = fresh_reg();
            if (kind == types::PrimitiveKind::F32) {
                std::string bits32 = fresh_reg();
                emit_line("  " + bits32 + " = bitcast float " + receiver + " to i32");
                emit_line("  " + bits + " = zext i32 " + bits32 + " to i64");
            } else {
                emit_line("  " + bits + " = bitcast double " + receiver + " to i64");
            }
            std::string xor_result = fresh_reg();
            emit_line("  " + xor_result + " = xor i64 " + bits + ", 14695981039346656037");
            emit_line("  " + result + " = mul i64 " + xor_result + ", 1099511628211");
        } else {
            return "0";
        }
        last_expr_type_ = "i64";
        return result;
    }

    // Str-specific methods
    if (kind == types::PrimitiveKind::Str) {
        // len() -> I64 (byte length of string)
        if (method == "len") {
            emit_coverage("Str::len");
            std::string len32 = fresh_reg();
            emit_line("  " + len32 + " = call i32 @str_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext i32 " + len32 + " to i64");
            last_expr_type_ = "i64";
            return result;
        }

        // is_empty() -> Bool
        if (method == "is_empty") {
            emit_coverage("Str::is_empty");
            std::string len32 = fresh_reg();
            emit_line("  " + len32 + " = call i32 @str_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp eq i32 " + len32 + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // as_bytes() -> ref [U8] (returns pointer to string data)
        if (method == "as_bytes") {
            emit_coverage("Str::as_bytes");
            // For now, return the string pointer itself as a slice
            // In TML, strings are already represented as pointers to their data
            last_expr_type_ = "ptr";
            return receiver;
        }

        // char_at(index: I64) -> I32
        if (method == "char_at") {
            emit_coverage("Str::char_at");
            if (call.args.empty()) {
                report_error("char_at() requires an index argument", call.span, "C008");
                return "0";
            }
            std::string idx = gen_expr(*call.args[0]);
            std::string idx_type = last_expr_type_;
            std::string idx_i32 = idx;
            if (idx_type == "i64") {
                idx_i32 = fresh_reg();
                emit_line("  " + idx_i32 + " = trunc i64 " + idx + " to i32");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @str_char_at(ptr " + receiver + ", i32 " +
                      idx_i32 + ")");
            last_expr_type_ = "i32";
            return result;
        }

        // slice_str(start: I64, end: I64) -> Str, also slice()
        if (method == "slice_str" || method == "slice") {
            emit_coverage("Str::slice");
            if (call.args.size() < 2) {
                report_error("slice_str() requires start and end arguments", call.span, "C008");
                return "0";
            }
            std::string start = gen_expr(*call.args[0]);
            std::string end = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_substring(ptr " + receiver + ", i64 " +
                      start + ", i64 " + end + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // to_uppercase() -> Str
        if (method == "to_uppercase") {
            emit_coverage("Str::to_uppercase");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_to_upper(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // to_lowercase() -> Str
        if (method == "to_lowercase") {
            emit_coverage("Str::to_lowercase");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_to_lower(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // starts_with(prefix: Str) -> Bool
        if (method == "starts_with") {
            emit_coverage("Str::starts_with");
            if (call.args.empty()) {
                report_error("starts_with() requires a prefix argument", call.span, "C008");
                return "0";
            }
            std::string prefix = gen_expr(*call.args[0]);
            std::string result32 = fresh_reg();
            emit_line("  " + result32 + " = call i32 @str_starts_with(ptr " + receiver + ", ptr " +
                      prefix + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + result32 + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // ends_with(suffix: Str) -> Bool
        if (method == "ends_with") {
            emit_coverage("Str::ends_with");
            if (call.args.empty()) {
                report_error("ends_with() requires a suffix argument", call.span, "C008");
                return "0";
            }
            std::string suffix = gen_expr(*call.args[0]);
            std::string result32 = fresh_reg();
            emit_line("  " + result32 + " = call i32 @str_ends_with(ptr " + receiver + ", ptr " +
                      suffix + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + result32 + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // contains(pattern: Str) -> Bool
        if (method == "contains") {
            emit_coverage("Str::contains");
            if (call.args.empty()) {
                report_error("contains() requires a pattern argument", call.span, "C008");
                return "0";
            }
            std::string pattern = gen_expr(*call.args[0]);
            std::string result32 = fresh_reg();
            emit_line("  " + result32 + " = call i32 @str_contains(ptr " + receiver + ", ptr " +
                      pattern + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + result32 + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // find(pattern: Str) -> I64 (returns -1 if not found)
        if (method == "find") {
            emit_coverage("Str::find");
            if (call.args.empty()) {
                report_error("find() requires a pattern argument", call.span, "C008");
                return "0";
            }
            std::string pattern = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @str_find(ptr " + receiver + ", ptr " + pattern +
                      ")");
            last_expr_type_ = "i64";
            return result;
        }

        // rfind(pattern: Str) -> I64 (returns -1 if not found)
        if (method == "rfind") {
            emit_coverage("Str::rfind");
            if (call.args.empty()) {
                report_error("rfind() requires a pattern argument", call.span, "C008");
                return "0";
            }
            std::string pattern = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @str_rfind(ptr " + receiver + ", ptr " +
                      pattern + ")");
            last_expr_type_ = "i64";
            return result;
        }

        // split(delimiter: Str) -> List[Str]
        if (method == "split") {
            emit_coverage("Str::split");
            if (call.args.empty()) {
                report_error("split() requires a delimiter argument", call.span, "C008");
                return "0";
            }
            std::string delim = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_split(ptr " + receiver + ", ptr " + delim +
                      ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // chars() -> List[I32]
        if (method == "chars") {
            emit_coverage("Str::chars");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_chars(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // trim() -> Str
        if (method == "trim") {
            emit_coverage("Str::trim");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_trim(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // trim_start() -> Str
        if (method == "trim_start") {
            emit_coverage("Str::trim_start");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_trim_start(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // trim_end() -> Str
        if (method == "trim_end") {
            emit_coverage("Str::trim_end");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_trim_end(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // parse_i64() -> Maybe[I64]
        if (method == "parse_i64") {
            emit_coverage("Str::parse_i64");
            std::string value = fresh_reg();
            emit_line("  " + value + " = call i64 @str_parse_i64(ptr " + receiver + ")");
            // Return Just(value) - create Maybe struct with tag=0 and value
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = insertvalue %struct.Maybe__I64 { i32 0, i64 undef }, i64 " + value +
                      ", 1");
            last_expr_type_ = "%struct.Maybe__I64";
            return result;
        }

        // parse_u16() -> Maybe[U16]
        if (method == "parse_u16") {
            emit_coverage("Str::parse_u16");
            std::string value64 = fresh_reg();
            emit_line("  " + value64 + " = call i64 @str_parse_i64(ptr " + receiver + ")");
            std::string value = fresh_reg();
            emit_line("  " + value + " = trunc i64 " + value64 + " to i16");
            // Return Just(value) - create Maybe struct with tag=0 and value
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = insertvalue %struct.Maybe__U16 { i32 0, i16 undef }, i16 " + value +
                      ", 1");
            last_expr_type_ = "%struct.Maybe__U16";
            return result;
        }

        // replace(from: Str, to: Str) -> Str
        if (method == "replace") {
            emit_coverage("Str::replace");
            if (call.args.size() < 2) {
                report_error("replace() requires 'from' and 'to' arguments", call.span, "C008");
                return "0";
            }
            std::string from = gen_expr(*call.args[0]);
            std::string to = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_replace(ptr " + receiver + ", ptr " + from +
                      ", ptr " + to + ")");
            last_expr_type_ = "ptr";
            return result;
        }
    }

    // Try to look up user-defined impl methods for primitive types (e.g., I32::abs)
    // Convert PrimitiveKind to type name
    std::string type_name;
    switch (kind) {
    case types::PrimitiveKind::I8:
        type_name = "I8";
        break;
    case types::PrimitiveKind::I16:
        type_name = "I16";
        break;
    case types::PrimitiveKind::I32:
        type_name = "I32";
        break;
    case types::PrimitiveKind::I64:
        type_name = "I64";
        break;
    case types::PrimitiveKind::I128:
        type_name = "I128";
        break;
    case types::PrimitiveKind::U8:
        type_name = "U8";
        break;
    case types::PrimitiveKind::U16:
        type_name = "U16";
        break;
    case types::PrimitiveKind::U32:
        type_name = "U32";
        break;
    case types::PrimitiveKind::U64:
        type_name = "U64";
        break;
    case types::PrimitiveKind::U128:
        type_name = "U128";
        break;
    case types::PrimitiveKind::F32:
        type_name = "F32";
        break;
    case types::PrimitiveKind::F64:
        type_name = "F64";
        break;
    case types::PrimitiveKind::Bool:
        type_name = "Bool";
        break;
    case types::PrimitiveKind::Str:
        type_name = "Str";
        break;
    case types::PrimitiveKind::Char:
        type_name = "Char";
        break;
    default:
        return std::nullopt;
    }

    // =========================================================================
    // Handle is_zero and is_one inline BEFORE the module lookup fallback.
    // These methods exist in the module registry but should be inlined.
    // =========================================================================
    if (method == "is_zero" && call.args.empty()) {
        bool is_zero_int =
            (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
             kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
             kind == types::PrimitiveKind::I128 || kind == types::PrimitiveKind::U8 ||
             kind == types::PrimitiveKind::U16 || kind == types::PrimitiveKind::U32 ||
             kind == types::PrimitiveKind::U64 || kind == types::PrimitiveKind::U128);
        bool is_zero_float =
            (kind == types::PrimitiveKind::F32 || kind == types::PrimitiveKind::F64);

        if (is_zero_int || is_zero_float) {
            emit_coverage(type_name + "::is_zero");
            std::string result = fresh_reg();
            if (is_zero_float) {
                emit_line("  " + result + " = fcmp oeq " + llvm_ty + " " + receiver + ", 0.0");
            } else {
                emit_line("  " + result + " = icmp eq " + llvm_ty + " " + receiver + ", 0");
            }
            last_expr_type_ = "i1";
            return result;
        }
    }

    if (method == "is_one" && call.args.empty()) {
        bool is_one_int = (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
                           kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
                           kind == types::PrimitiveKind::I128 || kind == types::PrimitiveKind::U8 ||
                           kind == types::PrimitiveKind::U16 || kind == types::PrimitiveKind::U32 ||
                           kind == types::PrimitiveKind::U64 || kind == types::PrimitiveKind::U128);
        bool is_one_float =
            (kind == types::PrimitiveKind::F32 || kind == types::PrimitiveKind::F64);

        if (is_one_int || is_one_float) {
            emit_coverage(type_name + "::is_one");
            std::string result = fresh_reg();
            if (is_one_float) {
                emit_line("  " + result + " = fcmp oeq " + llvm_ty + " " + receiver + ", 1.0");
            } else {
                emit_line("  " + result + " = icmp eq " + llvm_ty + " " + receiver + ", 1");
            }
            last_expr_type_ = "i1";
            return result;
        }
    }

    std::string qualified_name = type_name + "::" + method;
    auto func_sig = env_.lookup_func(qualified_name);

    // If not found in local env, search all imported modules
    if (!func_sig && env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto func_it = mod.functions.find(qualified_name);
            if (func_it != mod.functions.end()) {
                func_sig = func_it->second;
                break;
            }
        }
    }

    if (func_sig) {
        // Look up in functions_ to get the correct LLVM name
        std::string method_lookup_key = type_name + "_" + method;
        auto method_it = functions_.find(method_lookup_key);
        std::string fn_name;
        if (method_it != functions_.end()) {
            fn_name = method_it->second.llvm_name;
        } else {
            fn_name = "@tml_" + get_suite_prefix() + type_name + "_" + method;
        }

        // Check if method has 'mut this' - indicated by first param being a mut ref
        bool is_mut_this = false;
        if (!func_sig->params.empty() && func_sig->params[0]) {
            if (func_sig->params[0]->is<types::RefType>()) {
                is_mut_this = func_sig->params[0]->as<types::RefType>().is_mut;
            }
        }

        // Build arguments - 'this' is passed by pointer for 'mut this' methods, by value otherwise
        std::vector<std::pair<std::string, std::string>> typed_args;
        if (is_mut_this) {
            // For 'mut this', pass a pointer to the receiver
            // Use receiver_ptr if available, otherwise create a temporary alloca
            std::string ptr_to_pass;
            if (!receiver_ptr.empty()) {
                ptr_to_pass = receiver_ptr;
            } else {
                // Need to create temporary storage for the value
                std::string tmp = fresh_reg();
                emit_line("  " + tmp + " = alloca " + llvm_ty);
                emit_line("  store " + llvm_ty + " " + receiver + ", ptr " + tmp);
                ptr_to_pass = tmp;
            }
            typed_args.push_back({"ptr", ptr_to_pass});
        } else {
            typed_args.push_back({llvm_ty, receiver});
        }

        // Add remaining arguments
        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            std::string arg_type = "i32"; // default fallback
            if (i + 1 < func_sig->params.size()) {
                arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
            }
            typed_args.push_back({arg_type, val});
        }

        std::string ret_type = llvm_type_from_semantic(func_sig->return_type);
        std::string args_str;
        for (size_t i = 0; i < typed_args.size(); ++i) {
            if (i > 0)
                args_str += ", ";
            args_str += typed_args[i].first + " " + typed_args[i].second;
        }

        std::string result = fresh_reg();
        if (ret_type == "void") {
            emit_line("  call void " + fn_name + "(" + args_str + ")");
            last_expr_type_ = "void";
            return std::string("void");
        } else {
            emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str + ")");
            last_expr_type_ = ret_type;
            return result;
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
