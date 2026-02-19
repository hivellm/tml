//! # LLVM IR Generator - Primitive Type Methods
//!
//! This file implements method calls on primitive types (part 1).
//! Covers: arithmetic, comparison, formatting, wrapping, and saturating operations.
//!
//! Part 2 (checked arithmetic, hash, Str methods, impl lookup) is in
//! method_primitive_ext.cpp.
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

#include "codegen/llvm/llvm_ir_gen.hpp"

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
            emit_coverage(types::primitive_kind_to_string(kind) + "::add");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::sub");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::mul");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::div");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::rem");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::neg");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::cmp");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::partial_cmp");
            emit_coverage("Ord::partial_cmp");
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

        // clamp(min_val, max_val) -> Self
        if (method == "clamp") {
            emit_coverage("Ord::clamp");
            if (call.args.size() < 2) {
                report_error("clamp() requires two arguments", call.span, "C008");
                return "0";
            }
            std::string min_raw = gen_expr(*call.args[0]);
            std::string min_type = last_expr_type_;
            std::string max_raw = gen_expr(*call.args[1]);
            std::string max_type = last_expr_type_;
            // Arguments may be pointers (ref params) — load to get values
            // But if they're already immediate values (e.g. literals), use directly
            std::string min_val = min_raw;
            if (min_type == "ptr" || min_type.find("*") != std::string::npos) {
                min_val = fresh_reg();
                emit_line("  " + min_val + " = load " + llvm_ty + ", ptr " + min_raw);
            }
            std::string max_val = max_raw;
            if (max_type == "ptr" || max_type.find("*") != std::string::npos) {
                max_val = fresh_reg();
                emit_line("  " + max_val + " = load " + llvm_ty + ", ptr " + max_raw);
            }
            // clamp = max(min_val, min(max_val, self))
            // Step 1: clamped_high = self < max_val ? self : max_val (i.e. min(self, max_val))
            std::string cmp_high = fresh_reg();
            if (is_float) {
                emit_line("  " + cmp_high + " = fcmp olt " + llvm_ty + " " + receiver + ", " +
                          max_val);
            } else if (is_signed) {
                emit_line("  " + cmp_high + " = icmp slt " + llvm_ty + " " + receiver + ", " +
                          max_val);
            } else {
                emit_line("  " + cmp_high + " = icmp ult " + llvm_ty + " " + receiver + ", " +
                          max_val);
            }
            std::string clamped_high = fresh_reg();
            emit_line("  " + clamped_high + " = select i1 " + cmp_high + ", " + llvm_ty + " " +
                      receiver + ", " + llvm_ty + " " + max_val);
            // Step 2: result = clamped_high > min_val ? clamped_high : min_val (i.e.
            // max(clamped_high, min_val))
            std::string cmp_low = fresh_reg();
            if (is_float) {
                emit_line("  " + cmp_low + " = fcmp ogt " + llvm_ty + " " + clamped_high + ", " +
                          min_val);
            } else if (is_signed) {
                emit_line("  " + cmp_low + " = icmp sgt " + llvm_ty + " " + clamped_high + ", " +
                          min_val);
            } else {
                emit_line("  " + cmp_low + " = icmp ugt " + llvm_ty + " " + clamped_high + ", " +
                          min_val);
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + cmp_low + ", " + llvm_ty + " " +
                      clamped_high + ", " + llvm_ty + " " + min_val);
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
            // Convert exponent to double for @llvm.pow.f64
            std::string double_exp = fresh_reg();
            emit_line("  " + double_exp + " = sitofp " + last_expr_type_ + " " + exp +
                      " to double");
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @llvm.pow.f64(double " + double_base +
                      ", double " + double_exp + ")");
            std::string result = fresh_reg();
            if (is_signed) {
                emit_line("  " + result + " = fptosi double " + double_result + " to " + llvm_ty);
            } else {
                emit_line("  " + result + " = fptoui double " + double_result + " to " + llvm_ty);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }

        // ============ Arithmetic Assign Operations (mut this methods) ============
        // These methods mutate the receiver and return void

        // add_assign(rhs) - this = this + rhs
        if (method == "add_assign" && !receiver_ptr.empty()) {
            emit_coverage("AddAssign::add_assign");
            emit_coverage(types::primitive_kind_to_string(kind) + "::add_assign");
            if (call.args.empty()) {
                report_error("add_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fadd " + llvm_ty + " " + receiver + ", " + rhs);
            } else if (is_signed) {
                emit_line("  " + result + " = add nsw " + llvm_ty + " " + receiver + ", " + rhs);
            } else {
                emit_line("  " + result + " = add " + llvm_ty + " " + receiver + ", " + rhs);
            }
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // sub_assign(rhs) - this = this - rhs
        if (method == "sub_assign" && !receiver_ptr.empty()) {
            emit_coverage("SubAssign::sub_assign");
            emit_coverage(types::primitive_kind_to_string(kind) + "::sub_assign");
            if (call.args.empty()) {
                report_error("sub_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fsub " + llvm_ty + " " + receiver + ", " + rhs);
            } else if (is_signed) {
                emit_line("  " + result + " = sub nsw " + llvm_ty + " " + receiver + ", " + rhs);
            } else {
                emit_line("  " + result + " = sub " + llvm_ty + " " + receiver + ", " + rhs);
            }
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // mul_assign(rhs) - this = this * rhs
        if (method == "mul_assign" && !receiver_ptr.empty()) {
            emit_coverage("MulAssign::mul_assign");
            emit_coverage(types::primitive_kind_to_string(kind) + "::mul_assign");
            if (call.args.empty()) {
                report_error("mul_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fmul " + llvm_ty + " " + receiver + ", " + rhs);
            } else if (is_signed) {
                emit_line("  " + result + " = mul nsw " + llvm_ty + " " + receiver + ", " + rhs);
            } else {
                emit_line("  " + result + " = mul " + llvm_ty + " " + receiver + ", " + rhs);
            }
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // div_assign(rhs) - this = this / rhs
        if (method == "div_assign" && !receiver_ptr.empty()) {
            emit_coverage("DivAssign::div_assign");
            emit_coverage(types::primitive_kind_to_string(kind) + "::div_assign");
            if (call.args.empty()) {
                report_error("div_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = fdiv " + llvm_ty + " " + receiver + ", " + rhs);
            } else if (is_signed) {
                emit_line("  " + result + " = sdiv " + llvm_ty + " " + receiver + ", " + rhs);
            } else {
                emit_line("  " + result + " = udiv " + llvm_ty + " " + receiver + ", " + rhs);
            }
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // rem_assign(rhs) - this = this % rhs
        if (method == "rem_assign" && !receiver_ptr.empty()) {
            emit_coverage("RemAssign::rem_assign");
            emit_coverage(types::primitive_kind_to_string(kind) + "::rem_assign");
            if (call.args.empty()) {
                report_error("rem_assign() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            if (is_float) {
                emit_line("  " + result + " = frem " + llvm_ty + " " + receiver + ", " + rhs);
            } else if (is_signed) {
                emit_line("  " + result + " = srem " + llvm_ty + " " + receiver + ", " + rhs);
            } else {
                emit_line("  " + result + " = urem " + llvm_ty + " " + receiver + ", " + rhs);
            }
            emit_line("  store " + llvm_ty + " " + result + ", ptr " + receiver_ptr);
            last_expr_type_ = "void";
            return std::string("void");
        }

        // ============ Bit Assign Operations (mut this methods) ============
        // These methods mutate the receiver and return void

        // bitand_assign(rhs) - this = this & rhs
        if (method == "bitand_assign" && !receiver_ptr.empty()) {
            emit_coverage("BitAndAssign::bitand_assign");
            emit_coverage(types::primitive_kind_to_string(kind) + "::bitand_assign");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::bitor_assign");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::bitxor_assign");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::shl_assign");
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
            emit_coverage(types::primitive_kind_to_string(kind) + "::shr_assign");
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

    // ============ Bitwise Non-Assign Operations (trait methods) ============
    // These return Self and do not mutate the receiver

    if (is_integer) {
        // bitand(rhs) -> Self  (this & rhs)
        if (method == "bitand") {
            emit_coverage("BitAnd::bitand");
            emit_coverage(types::primitive_kind_to_string(kind) + "::bitand");
            if (call.args.empty()) {
                report_error("bitand() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + llvm_ty + " " + receiver + ", " + rhs);
            last_expr_type_ = llvm_ty;
            return result;
        }

        // bitor(rhs) -> Self  (this | rhs)
        if (method == "bitor") {
            emit_coverage("BitOr::bitor");
            emit_coverage(types::primitive_kind_to_string(kind) + "::bitor");
            if (call.args.empty()) {
                report_error("bitor() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = or " + llvm_ty + " " + receiver + ", " + rhs);
            last_expr_type_ = llvm_ty;
            return result;
        }

        // bitxor(rhs) -> Self  (this ^ rhs)
        if (method == "bitxor") {
            emit_coverage("BitXor::bitxor");
            emit_coverage(types::primitive_kind_to_string(kind) + "::bitxor");
            if (call.args.empty()) {
                report_error("bitxor() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + llvm_ty + " " + receiver + ", " + rhs);
            last_expr_type_ = llvm_ty;
            return result;
        }

        // negate() -> Self  (bitwise NOT: ~this)
        if (method == "negate") {
            emit_coverage("Not::negate");
            emit_coverage(types::primitive_kind_to_string(kind) + "::negate");
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + llvm_ty + " " + receiver + ", -1");
            last_expr_type_ = llvm_ty;
            return result;
        }

        // shift_left(rhs) -> Self  (this << rhs)
        if (method == "shift_left") {
            emit_coverage("Shl::shift_left");
            emit_coverage(types::primitive_kind_to_string(kind) + "::shift_left");
            if (call.args.empty()) {
                report_error("shift_left() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string rhs_type = last_expr_type_;
            std::string rhs_final = rhs;
            if (rhs_type != llvm_ty) {
                rhs_final = fresh_reg();
                emit_line("  " + rhs_final + " = zext " + rhs_type + " " + rhs + " to " + llvm_ty);
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl " + llvm_ty + " " + receiver + ", " + rhs_final);
            last_expr_type_ = llvm_ty;
            return result;
        }

        // shift_right(rhs) -> Self  (this >> rhs, arithmetic for signed, logical for unsigned)
        if (method == "shift_right") {
            emit_coverage("Shr::shift_right");
            emit_coverage(types::primitive_kind_to_string(kind) + "::shift_right");
            if (call.args.empty()) {
                report_error("shift_right() requires an argument", call.span, "C008");
                return "0";
            }
            std::string rhs = gen_expr(*call.args[0]);
            std::string rhs_type = last_expr_type_;
            std::string rhs_final = rhs;
            if (rhs_type != llvm_ty) {
                rhs_final = fresh_reg();
                emit_line("  " + rhs_final + " = zext " + rhs_type + " " + rhs + " to " + llvm_ty);
            }
            std::string result = fresh_reg();
            if (is_signed) {
                emit_line("  " + result + " = ashr " + llvm_ty + " " + receiver + ", " + rhs_final);
            } else {
                emit_line("  " + result + " = lshr " + llvm_ty + " " + receiver + ", " + rhs_final);
            }
            last_expr_type_ = llvm_ty;
            return result;
        }
    }

    // Bool operations
    if (kind == types::PrimitiveKind::Bool) {
        if (method == "negate") {
            emit_coverage("Not::negate");
            emit_coverage("Bool::negate");
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor i1 " + receiver + ", true");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // duplicate() -> Self (copy semantics for primitives)
    if (method == "duplicate") {
        emit_coverage("Duplicate::duplicate");
        emit_coverage(types::primitive_kind_to_string(kind) + "::duplicate");
        last_expr_type_ = llvm_ty;
        return receiver;
    }

    // to_owned() -> Self
    if (method == "to_owned") {
        emit_coverage("ToOwned::to_owned");
        emit_coverage(types::primitive_kind_to_string(kind) + "::to_owned");
        last_expr_type_ = llvm_ty;
        return receiver;
    }

    // borrow() -> ref Self
    if (method == "borrow") {
        emit_coverage("Borrow::borrow");
        emit_coverage(types::primitive_kind_to_string(kind) + "::borrow");
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
        emit_coverage(types::primitive_kind_to_string(kind) + "::borrow_mut");
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
        emit_coverage(types::primitive_kind_to_string(kind) + "::" + method);
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

    // fmt_binary() -> Str (Binary behavior)
    if (method == "fmt_binary" && is_integer) {
        emit_coverage("Binary::fmt_binary");
        emit_coverage(types::primitive_kind_to_string(kind) + "::fmt_binary");
        // Extend/truncate receiver to i64 for the runtime call
        std::string val64 = receiver;
        if (llvm_ty != "i64") {
            val64 = fresh_reg();
            if (is_signed) {
                emit_line("  " + val64 + " = sext " + llvm_ty + " " + receiver + " to i64");
            } else {
                emit_line("  " + val64 + " = zext " + llvm_ty + " " + receiver + " to i64");
            }
        }
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @i64_to_binary_str(i64 " + val64 + ")");
        last_expr_type_ = "ptr";
        return result;
    }

    // fmt_octal() -> Str (Octal behavior)
    if (method == "fmt_octal" && is_integer) {
        emit_coverage("Octal::fmt_octal");
        emit_coverage(types::primitive_kind_to_string(kind) + "::fmt_octal");
        std::string val64 = receiver;
        if (llvm_ty != "i64") {
            val64 = fresh_reg();
            if (is_signed) {
                emit_line("  " + val64 + " = sext " + llvm_ty + " " + receiver + " to i64");
            } else {
                emit_line("  " + val64 + " = zext " + llvm_ty + " " + receiver + " to i64");
            }
        }
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @i64_to_octal_str(i64 " + val64 + ")");
        last_expr_type_ = "ptr";
        return result;
    }

    // fmt_lower_hex() -> Str (LowerHex behavior)
    if (method == "fmt_lower_hex" && is_integer) {
        emit_coverage("LowerHex::fmt_lower_hex");
        emit_coverage(types::primitive_kind_to_string(kind) + "::fmt_lower_hex");
        std::string val64 = receiver;
        if (llvm_ty != "i64") {
            val64 = fresh_reg();
            if (is_signed) {
                emit_line("  " + val64 + " = sext " + llvm_ty + " " + receiver + " to i64");
            } else {
                emit_line("  " + val64 + " = zext " + llvm_ty + " " + receiver + " to i64");
            }
        }
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @i64_to_lower_hex_str(i64 " + val64 + ")");
        last_expr_type_ = "ptr";
        return result;
    }

    // fmt_upper_hex() -> Str (UpperHex behavior)
    if (method == "fmt_upper_hex" && is_integer) {
        emit_coverage("UpperHex::fmt_upper_hex");
        emit_coverage(types::primitive_kind_to_string(kind) + "::fmt_upper_hex");
        std::string val64 = receiver;
        if (llvm_ty != "i64") {
            val64 = fresh_reg();
            if (is_signed) {
                emit_line("  " + val64 + " = sext " + llvm_ty + " " + receiver + " to i64");
            } else {
                emit_line("  " + val64 + " = zext " + llvm_ty + " " + receiver + " to i64");
            }
        }
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @i64_to_upper_hex_str(i64 " + val64 + ")");
        last_expr_type_ = "ptr";
        return result;
    }

    // fmt_lower_exp() -> Str (LowerExp behavior) for floats
    if (method == "fmt_lower_exp" && is_float) {
        emit_coverage("LowerExp::fmt_lower_exp");
        emit_coverage(types::primitive_kind_to_string(kind) + "::fmt_lower_exp");
        std::string result = fresh_reg();
        if (kind == types::PrimitiveKind::F32) {
            emit_line("  " + result + " = call ptr @f32_to_exp_string(float " + receiver +
                      ", i32 0)");
        } else {
            emit_line("  " + result + " = call ptr @f64_to_exp_string(double " + receiver +
                      ", i32 0)");
        }
        last_expr_type_ = "ptr";
        return result;
    }

    // fmt_upper_exp() -> Str (UpperExp behavior) for floats
    if (method == "fmt_upper_exp" && is_float) {
        emit_coverage("UpperExp::fmt_upper_exp");
        emit_coverage(types::primitive_kind_to_string(kind) + "::fmt_upper_exp");
        std::string result = fresh_reg();
        if (kind == types::PrimitiveKind::F32) {
            emit_line("  " + result + " = call ptr @f32_to_exp_string(float " + receiver +
                      ", i32 1)");
        } else {
            emit_line("  " + result + " = call ptr @f64_to_exp_string(double " + receiver +
                      ", i32 1)");
        }
        last_expr_type_ = "ptr";
        return result;
    }

    // ========================================================================
    // Wrapping arithmetic (integers wrap naturally in LLVM)
    // ========================================================================

    if (method == "wrapping_add" && is_integer) {
        emit_coverage("WrappingAdd::wrapping_add");
        emit_coverage(types::primitive_kind_to_string(kind) + "::wrapping_add");
        if (call.args.empty()) {
            report_error("wrapping_add() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = add " + llvm_ty + " " + receiver + ", " + other);
        last_expr_type_ = llvm_ty;
        return result;
    }

    if (method == "wrapping_sub" && is_integer) {
        emit_coverage("WrappingSub::wrapping_sub");
        emit_coverage(types::primitive_kind_to_string(kind) + "::wrapping_sub");
        if (call.args.empty()) {
            report_error("wrapping_sub() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = sub " + llvm_ty + " " + receiver + ", " + other);
        last_expr_type_ = llvm_ty;
        return result;
    }

    if (method == "wrapping_mul" && is_integer) {
        emit_coverage("WrappingMul::wrapping_mul");
        emit_coverage(types::primitive_kind_to_string(kind) + "::wrapping_mul");
        if (call.args.empty()) {
            report_error("wrapping_mul() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = mul " + llvm_ty + " " + receiver + ", " + other);
        last_expr_type_ = llvm_ty;
        return result;
    }

    if (method == "wrapping_neg" && is_integer) {
        emit_coverage("WrappingNeg::wrapping_neg");
        emit_coverage(types::primitive_kind_to_string(kind) + "::wrapping_neg");
        std::string result = fresh_reg();
        emit_line("  " + result + " = sub " + llvm_ty + " 0, " + receiver);
        last_expr_type_ = llvm_ty;
        return result;
    }

    // ========================================================================
    // Saturating arithmetic
    // ========================================================================

    if (method == "saturating_add" && is_integer) {
        emit_coverage("SaturatingAdd::saturating_add");
        emit_coverage(types::primitive_kind_to_string(kind) + "::saturating_add");
        if (call.args.empty()) {
            report_error("saturating_add() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        std::string prefix = is_signed ? "s" : "u";
        emit_line("  " + result + " = call " + llvm_ty + " @llvm." + prefix + "add.sat." + llvm_ty +
                  "(" + llvm_ty + " " + receiver + ", " + llvm_ty + " " + other + ")");
        last_expr_type_ = llvm_ty;
        return result;
    }

    if (method == "saturating_sub" && is_integer) {
        emit_coverage("SaturatingSub::saturating_sub");
        emit_coverage(types::primitive_kind_to_string(kind) + "::saturating_sub");
        if (call.args.empty()) {
            report_error("saturating_sub() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        std::string prefix = is_signed ? "s" : "u";
        emit_line("  " + result + " = call " + llvm_ty + " @llvm." + prefix + "sub.sat." + llvm_ty +
                  "(" + llvm_ty + " " + receiver + ", " + llvm_ty + " " + other + ")");
        last_expr_type_ = llvm_ty;
        return result;
    }

    if (method == "saturating_mul" && is_integer) {
        emit_coverage("SaturatingMul::saturating_mul");
        emit_coverage(types::primitive_kind_to_string(kind) + "::saturating_mul");
        if (call.args.empty()) {
            report_error("saturating_mul() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);
        // No LLVM intrinsic for saturating multiply — use overflow detection + select
        std::string op = is_signed ? "smul" : "umul";
        std::string overflow_type = "{ " + llvm_ty + ", i1 }";
        std::string ov_result = fresh_reg();
        emit_line("  " + ov_result + " = call " + overflow_type + " @llvm." + op +
                  ".with.overflow." + llvm_ty + "(" + llvm_ty + " " + receiver + ", " + llvm_ty +
                  " " + other + ")");
        std::string value = fresh_reg();
        std::string overflow = fresh_reg();
        emit_line("  " + value + " = extractvalue " + overflow_type + " " + ov_result + ", 0");
        emit_line("  " + overflow + " = extractvalue " + overflow_type + " " + ov_result + ", 1");
        if (is_signed) {
            // For signed: if overflow, check sign of inputs to decide MAX or MIN
            std::string xor_signs = fresh_reg();
            emit_line("  " + xor_signs + " = xor " + llvm_ty + " " + receiver + ", " + other);
            std::string is_neg = fresh_reg();
            emit_line("  " + is_neg + " = icmp slt " + llvm_ty + " " + xor_signs + ", 0");
            // If product of signs is negative -> MIN, else -> MAX
            std::string sat_min = fresh_reg();
            std::string sat_max = fresh_reg();
            // Get the bit width for the type
            int bits = 32;
            if (llvm_ty == "i8")
                bits = 8;
            else if (llvm_ty == "i16")
                bits = 16;
            else if (llvm_ty == "i32")
                bits = 32;
            else if (llvm_ty == "i64")
                bits = 64;
            else if (llvm_ty == "i128")
                bits = 128;
            int64_t min_val = (bits == 8)    ? -128
                              : (bits == 16) ? -32768
                              : (bits == 32) ? (int64_t)INT32_MIN
                                             : INT64_MIN;
            int64_t max_val = (bits == 8)    ? 127
                              : (bits == 16) ? 32767
                              : (bits == 32) ? (int64_t)INT32_MAX
                                             : INT64_MAX;
            std::string sat_val = fresh_reg();
            emit_line("  " + sat_val + " = select i1 " + is_neg + ", " + llvm_ty + " " +
                      std::to_string(min_val) + ", " + llvm_ty + " " + std::to_string(max_val));
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + overflow + ", " + llvm_ty + " " + sat_val +
                      ", " + llvm_ty + " " + value);
            last_expr_type_ = llvm_ty;
            return result;
        } else {
            // For unsigned: if overflow, saturate to MAX (all ones)
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + overflow + ", " + llvm_ty + " -1, " +
                      llvm_ty + " " + value);
            last_expr_type_ = llvm_ty;
            return result;
        }
    }

    // Delegate to gen_primitive_method_ext for checked arithmetic, hash,
    // Str methods, and user-defined impl method lookup.
    return gen_primitive_method_ext(call, receiver, receiver_ptr, receiver_type, inner_type, kind,
                                    is_integer, is_signed, is_float, llvm_ty);
}

} // namespace tml::codegen
