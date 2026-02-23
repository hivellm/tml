TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Float Intrinsic Builtins
//!
//! This file implements float intrinsics via try_gen_builtin_string().
//!
//! ## History
//!
//! Originally contained 13 `str_*` builtins and 16 `*_to_string` builtins.
//! All were removed as dead code:
//! - Phase 18: Char builtins migrated to pure TML
//! - Phase 31: String builtins migrated to inline IR
//! - Phase 36: All `*_to_string` builtins removed (dead — no TML callers;
//!   all `.to_string()` calls go through method dispatch, all `lowlevel` calls
//!   resolve through `functions_[]` map, not this builtin interceptor)
//!
//! ## Remaining Functions
//!
//! | Function                | Implementation                   |
//! |-------------------------|----------------------------------|
//! | `f*_is_nan/is_infinite` | Pure LLVM IR (`fcmp`)            |
//! | `f64_round`             | `@llvm.round.f64` intrinsic     |

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_string(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Integer/float to_string builtins — removed in Phase 36 (dead code).
    // All `.to_string()` calls dispatch through method_primitive.cpp.
    // All `lowlevel { f64_to_string_precision(...) }` calls resolve via functions_[] map.
    // The bare builtin interceptor here was unreachable.

    // ========================================================================
    // Float Intrinsics (pure LLVM IR, no C runtime)
    // ========================================================================

    // f32_is_nan(n) -> Bool — pure LLVM IR: fcmp uno (NaN != NaN)
    if (fn_name == "f32_is_nan") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = fcmp uno float " + n + ", 0.0");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f64_is_nan(n) -> Bool — pure LLVM IR: fcmp uno (NaN != NaN)
    if (fn_name == "f64_is_nan") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = fcmp uno double " + n + ", 0.0");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f32_is_infinite(n) -> Bool — pure LLVM IR: fabs + fcmp oeq +inf
    if (fn_name == "f32_is_infinite") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string abs_val = fresh_reg();
            emit_line("  " + abs_val + " = call float @llvm.fabs.f32(float " + n + ")");
            std::string result = fresh_reg();
            // Float +inf = 0x7F800000, LLVM float hex = upper 32 bits of 64-bit
            emit_line("  " + result + " = fcmp oeq float " + abs_val + ", 0x7F80000000000000");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f64_is_infinite(n) -> Bool — pure LLVM IR: fabs + fcmp oeq +inf
    if (fn_name == "f64_is_infinite") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string abs_val = fresh_reg();
            emit_line("  " + abs_val + " = call double @llvm.fabs.f64(double " + n + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = fcmp oeq double " + abs_val + ", 0x7FF0000000000000");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f64_round(n) -> F64
    if (fn_name == "f64_round") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @llvm.round.f64(double " + n + ")");
            last_expr_type_ = "double";
            return result;
        }
        last_expr_type_ = "double";
        return "0.0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
