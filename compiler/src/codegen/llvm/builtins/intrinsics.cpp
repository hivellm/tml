TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Compiler Intrinsics
//!
//! This file implements compiler intrinsics that map directly to LLVM instructions.
//! Intrinsics are `@intrinsic` decorated functions from `core::intrinsics`.
//!
//! ## Arithmetic Intrinsics
//!
//! | Intrinsic   | Integer   | Float    |
//! |-------------|-----------|----------|
//! | `llvm_add`  | `add`     | `fadd`   |
//! | `llvm_sub`  | `sub`     | `fsub`   |
//! | `llvm_mul`  | `mul`     | `fmul`   |
//! | `llvm_div`  | `sdiv`    | `fdiv`   |
//! | `llvm_rem`  | `srem`    | `frem`   |
//! | `llvm_neg`  | `sub 0,x` | `fneg`   |
//!
//! ## Comparison Intrinsics
//!
//! | Intrinsic  | Integer     | Float       |
//! |------------|-------------|-------------|
//! | `llvm_eq`  | `icmp eq`   | `fcmp oeq`  |
//! | `llvm_ne`  | `icmp ne`   | `fcmp one`  |
//! | `llvm_lt`  | `icmp slt`  | `fcmp olt`  |
//! | `llvm_le`  | `icmp sle`  | `fcmp ole`  |
//! | `llvm_gt`  | `icmp sgt`  | `fcmp ogt`  |
//! | `llvm_ge`  | `icmp sge`  | `fcmp oge`  |
//!
//! ## Bitwise Intrinsics
//!
//! | Intrinsic   | LLVM Instruction |
//! |-------------|------------------|
//! | `llvm_and`  | `and`            |
//! | `llvm_or`   | `or`             |
//! | `llvm_xor`  | `xor`            |
//! | `llvm_not`  | `xor x, -1`      |
//! | `llvm_shl`  | `shl`            |
//! | `llvm_shr`  | `ashr`           |
//!
//! ## Memory Intrinsics
//!
//! | Intrinsic     | Description                     |
//! |---------------|---------------------------------|
//! | `ptr_read`    | Load from pointer               |
//! | `ptr_write`   | Store to pointer                |
//! | `ptr_offset`  | GEP-based pointer arithmetic    |
//!
//! ## Slice Intrinsics
//!
//! | Intrinsic        | Description                   |
//! |------------------|-------------------------------|
//! | `slice_get`      | Get element reference         |
//! | `slice_get_mut`  | Get mutable element reference |
//! | `slice_set`      | Set element value             |
//! | `slice_offset`   | Offset slice pointer          |
//! | `slice_swap`     | Swap two elements             |
//!
//! ## Array Intrinsics
//!
//! | Intrinsic            | Description                  |
//! |----------------------|------------------------------|
//! | `array_as_ptr`       | Get array data pointer       |
//! | `array_as_mut_ptr`   | Get mutable array pointer    |
//! | `array_offset_ptr`   | Offset within array          |
//!
//! ## Compiler Hints
//!
//! | Intrinsic     | LLVM                      |
//! |---------------|---------------------------|
//! | `unreachable` | `unreachable`             |
//! | `assume`      | `@llvm.assume`            |
//! | `likely`      | `@llvm.expect.i1(x,true)` |
//! | `unlikely`    | `@llvm.expect.i1(x,false)`|
//! | `fence`       | `fence seq_cst`           |
//!
//! ## Bit Manipulation
//!
//! | Intrinsic    | LLVM Intrinsic      |
//! |--------------|---------------------|
//! | `ctlz`       | `@llvm.ctlz`        |
//! | `cttz`       | `@llvm.cttz`        |
//! | `ctpop`      | `@llvm.ctpop`       |
//! | `bswap`      | `@llvm.bswap`       |
//! | `bitreverse` | `@llvm.bitreverse`  |
//!
//! ## Math Intrinsics
//!
//! | Intrinsic  | LLVM Intrinsic  |
//! |------------|-----------------|
//! | `sqrt`     | `@llvm.sqrt`    |
//! | `sin`      | `@llvm.sin`     |
//! | `cos`      | `@llvm.cos`     |
//! | `log`      | `@llvm.log`     |
//! | `exp`      | `@llvm.exp`     |
//! | `pow`      | `@llvm.pow`     |
//! | `floor`    | `@llvm.floor`   |
//! | `ceil`     | `@llvm.ceil`    |
//! | `round`    | `@llvm.round`   |
//! | `trunc`    | `@llvm.trunc`   |
//! | `fma`      | `@llvm.fma`     |
//! | `fabs`     | `@llvm.fabs`    |

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <unordered_set>

namespace tml::codegen {

auto LLVMIRGen::try_gen_intrinsic(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Known intrinsic function names (from core::intrinsics module)
    // These are matched by name rather than @intrinsic attribute for simplicity
    static const std::unordered_set<std::string> intrinsics = {
        "unreachable", "assume", "likely", "unlikely", "llvm_add", "llvm_sub", "llvm_mul",
        "llvm_div", "llvm_rem", "llvm_neg", "llvm_and", "llvm_or", "llvm_xor", "llvm_not",
        "llvm_shl", "llvm_shr", "llvm_eq", "llvm_ne", "llvm_lt", "llvm_le", "llvm_gt", "llvm_ge",
        "transmute", "size_of", "align_of", "alignof_type", "sizeof_type", "type_name", "type_id",
        "ptr_offset", "ptr_read", "ptr_write", "ptr_copy", "store_byte", "volatile_read",
        "volatile_write", "ptr_read_volatile", "ptr_write_volatile", "ptr_read_unaligned",
        "ptr_write_unaligned", "memcpy", "memmove", "memset", "atomic_load", "atomic_store",
        "atomic_cas", "atomic_exchange", "atomic_add", "atomic_sub", "atomic_and", "atomic_or",
        "atomic_xor", "fence", "compiler_fence", "black_box", "spin_loop_hint",
        // Slice intrinsics
        "slice_get", "slice_get_mut", "slice_set", "slice_swap", "slice_offset",
        // Math intrinsics
        "sqrt", "sin", "cos", "log", "exp", "pow", "floor", "ceil", "round", "trunc", "fma", "fabs",
        "copysign", "minnum", "maxnum",
        // Bit manipulation intrinsics
        "ctlz", "cttz", "ctpop", "bswap", "bitreverse",
        // Drop intrinsic - for explicit destruction
        "drop",
        // Checked arithmetic intrinsics
        "checked_add", "checked_sub", "checked_mul", "checked_div",
        // Saturating arithmetic intrinsics
        "saturating_add", "saturating_sub", "saturating_mul",
        // Reflection intrinsics
        "field_count", "variant_count", "field_name", "field_type_id", "field_offset",
        // OOP reflection intrinsics
        "base_class", "is_abstract", "is_sealed", "method_count", "method_name", "is_virtual",
        "is_override", "is_static_method", "interface_method_count", "interface_method_name",
        "impl_count", "impl_name",
        // Dynamic function pointer call intrinsics
        "call_fn_ptr_i64", "call_fn_ptr_void", "call_fn_ptr_ptr",
        // Memory copy/set intrinsics
        "copy_nonoverlapping", "copy", "write_bytes",
        // SIMD vector intrinsics
        "simd_load", "simd_store", "simd_extract", "simd_insert", "simd_splat", "simd_load_ptr",
        "simd_bitmask",
        // Native SSE2 intrinsics
        "sse2_cmpeq_epi8", "sse2_movemask_epi8",
        // SSE2 comparison
        "sse2_cmpgt_epi8", "sse2_cmpeq_epi16", "sse2_cmpeq_epi32", "sse2_cmpgt_epi16",
        "sse2_cmpgt_epi32", "sse2_cmplt_epi8",
        // SSE2 bitwise
        "sse2_and_si128", "sse2_or_si128", "sse2_xor_si128", "sse2_andnot_si128",
        // SSE2 min/max
        "sse2_min_epu8", "sse2_max_epu8", "sse2_min_epi16", "sse2_max_epi16",
        // SSE2 movemask
        "sse2_movemask_ps", "sse2_movemask_pd",
        // SSE2 pack/unpack
        "sse2_packs_epi16", "sse2_packus_epi16", "sse2_packs_epi32", "sse2_unpacklo_epi8",
        "sse2_unpackhi_epi8",
        // SSE2 shift
        "sse2_slli_epi16", "sse2_slli_epi32", "sse2_slli_epi64", "sse2_srli_epi16",
        "sse2_srli_epi32", "sse2_srli_epi64", "sse2_srai_epi16", "sse2_srai_epi32",
        // SSE2 memory
        "sse2_storeu_si128", "sse2_store_si128",
        // SSE4.2 string comparison
        "sse42_cmpistrm", "sse42_cmpistri", "sse42_cmpestrm", "sse42_cmpestri",
        // SSE4.2 CRC32
        "sse42_crc32_u8", "sse42_crc32_u16", "sse42_crc32_u32", "sse42_crc32_u64",
        // POPCNT
        "popcnt_u32", "popcnt_u64",
        // Array element intrinsics
        "array_take", "array_get", "array_get_ref", "array_set", "array_uninit"};

    // Extract base name for intrinsic matching - handles qualified paths like
    // "core::intrinsics::sqrt" by extracting just "sqrt"
    std::string base_name = fn_name;
    if (fn_name.find("::") != std::string::npos) {
        size_t last_sep = fn_name.rfind("::");
        if (last_sep != std::string::npos) {
            base_name = fn_name.substr(last_sep + 2);
        }
    }

    if (intrinsics.find(base_name) == intrinsics.end()) {
        return std::nullopt;
    }

    // Use base_name for all subsequent intrinsic checks
    const std::string& intrinsic_name = base_name;

    // Coverage instrumentation for intrinsics
    // This tracks all intrinsic calls uniformly, regardless of which specific intrinsic
    emit_coverage(intrinsic_name);

    // Function signature lookup (used by some intrinsics for type info)
    auto func_sig = env_.lookup_func(fn_name);

    // Helper: check if a type string is a float type (scalar or vector)
    // Matches "float", "double", "<N x float>", "<N x double>"
    auto is_float_type = [](const std::string& t) -> bool {
        if (t == "float" || t == "double")
            return true;
        if (t.starts_with("<") &&
            (t.find("x float>") != std::string::npos || t.find("x double>") != std::string::npos))
            return true;
        return false;
    };

    // ============================================================================
    // Arithmetic Intrinsics
    // ============================================================================

    // llvm_add[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_add") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fadd " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = add " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_sub[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_sub") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fsub " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = sub " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_mul[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_mul") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fmul " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = mul " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_div[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_div") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fdiv " + a_type + " " + a + ", " + b);
            } else {
                // Default to signed division for now
                emit_line("  " + result + " = sdiv " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_rem[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_rem") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = frem " + a_type + " " + a + ", " + b);
            } else {
                // Default to signed remainder
                emit_line("  " + result + " = srem " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_neg[T](a: T) -> T
    if (intrinsic_name == "llvm_neg") {
        if (!call.args.empty()) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fneg " + a_type + " " + a);
            } else {
                emit_line("  " + result + " = sub " + a_type + " 0, " + a);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Comparison Intrinsics
    // ============================================================================

    // llvm_eq[T](a: T, b: T) -> Bool (or <N x i1> for vector types)
    if (intrinsic_name == "llvm_eq") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fcmp oeq " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp eq " + a_type + " " + a + ", " + b);
            }
            // Vector comparison returns <N x i1>, scalar returns i1
            if (a_type.starts_with("<")) {
                auto space_pos = a_type.find(' ');
                std::string n = a_type.substr(1, space_pos - 1);
                last_expr_type_ = "<" + n + " x i1>";
            } else {
                last_expr_type_ = "i1";
            }
            return result;
        }
        return "0";
    }

    // llvm_ne[T](a: T, b: T) -> Bool (or <N x i1> for vector types)
    if (intrinsic_name == "llvm_ne") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fcmp one " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp ne " + a_type + " " + a + ", " + b);
            }
            // Vector comparison returns <N x i1>, scalar returns i1
            if (a_type.starts_with("<")) {
                auto space_pos = a_type.find(' ');
                std::string n = a_type.substr(1, space_pos - 1);
                last_expr_type_ = "<" + n + " x i1>";
            } else {
                last_expr_type_ = "i1";
            }
            return result;
        }
        return "0";
    }

    // llvm_lt[T](a: T, b: T) -> Bool
    if (intrinsic_name == "llvm_lt") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fcmp olt " + a_type + " " + a + ", " + b);
            } else {
                // Default to signed comparison
                emit_line("  " + result + " = icmp slt " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_le[T](a: T, b: T) -> Bool
    if (intrinsic_name == "llvm_le") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fcmp ole " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp sle " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_gt[T](a: T, b: T) -> Bool
    if (intrinsic_name == "llvm_gt") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fcmp ogt " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp sgt " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_ge[T](a: T, b: T) -> Bool
    if (intrinsic_name == "llvm_ge") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (is_float_type(a_type)) {
                emit_line("  " + result + " = fcmp oge " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp sge " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Bitwise Intrinsics
    // ============================================================================

    // llvm_and[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_and") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_or[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_or") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = or " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_xor[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_xor") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_not[T](a: T) -> T
    if (intrinsic_name == "llvm_not") {
        if (!call.args.empty()) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + a_type + " " + a + ", -1");
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_shl[T](a: T, b: T) -> T
    if (intrinsic_name == "llvm_shl") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_shr[T](a: T, b: T) -> T (arithmetic shift right for signed)
    if (intrinsic_name == "llvm_shr") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            // Default to arithmetic shift (preserves sign)
            emit_line("  " + result + " = ashr " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Memory Intrinsics
    // ============================================================================

    // Helper: resolve element type for ptr_read/ptr_write family intrinsics
    // Priority: 1) generic type param [T], 2) pointer inner type, 3) default i32
    auto resolve_ptr_elem_type = [&](const parser::CallExpr& c, int ptr_arg_idx) -> std::string {
        // Try generic type parameter first (e.g., ptr_read[I32](...))
        if (c.callee->is<parser::PathExpr>()) {
            const auto& path_expr = c.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    std::string type_llvm = llvm_type_from_semantic(resolved, true);
                    if (type_llvm != "void" && type_llvm != "{}")
                        return type_llvm;
                }
            }
        }
        // Fall back to pointer inner type
        if (ptr_arg_idx < static_cast<int>(c.args.size())) {
            types::TypePtr arg_type = infer_expr_type(*c.args[ptr_arg_idx]);
            if (arg_type && arg_type->is<types::PtrType>()) {
                std::string elem =
                    llvm_type_from_semantic(arg_type->as<types::PtrType>().inner, true);
                if (elem != "void" && elem != "{}")
                    return elem;
            }
        }
        return std::string("i32"); // Default
    };

    // ptr_read[T](ptr: Ptr[T]) -> T
    if (intrinsic_name == "ptr_read") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string ptr_type = last_expr_type_;

            if (ptr_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + ptr + " to ptr");
                ptr = conv;
            }

            std::string elem_type = resolve_ptr_elem_type(call, 0);

            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_type + ", ptr " + ptr);
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // ptr_write[T](ptr: Ptr[T], val: T)
    if (intrinsic_name == "ptr_write") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string ptr_type = last_expr_type_;

            // If the pointer argument is i64 (e.g., RawMutPtr.addr field),
            // convert it to ptr with inttoptr
            if (ptr_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + ptr + " to ptr");
                ptr = conv;
            }

            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;
            emit_line("  store " + val_type + " " + val + ", ptr " + ptr);
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // store_byte(ptr: *U8, offset: I64, byte: I32) - store byte at ptr+offset
    // Optimized for tight loops - combines GEP and store in one intrinsic
    if (intrinsic_name == "store_byte") {
        if (call.args.size() >= 3) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string byte_val = gen_expr(*call.args[2]);
            std::string gep_reg = fresh_reg();
            std::string trunc_reg = fresh_reg();
            // GEP to get offset pointer
            emit_line("  " + gep_reg + " = getelementptr i8, ptr " + ptr + ", i64 " + offset);
            // Truncate I32 to i8
            emit_line("  " + trunc_reg + " = trunc i32 " + byte_val + " to i8");
            // Store the byte
            emit_line("  store i8 " + trunc_reg + ", ptr " + gep_reg);
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // ptr_read_volatile[T](ptr) -> T
    // Like ptr_read but with volatile semantics (prevents reordering/elimination)
    if (intrinsic_name == "ptr_read_volatile" || intrinsic_name == "volatile_read") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string ptr_type = last_expr_type_;

            if (ptr_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + ptr + " to ptr");
                ptr = conv;
            }

            std::string elem_type = resolve_ptr_elem_type(call, 0);

            std::string result = fresh_reg();
            emit_line("  " + result + " = load volatile " + elem_type + ", ptr " + ptr);
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // ptr_write_volatile[T](ptr, val)
    // Like ptr_write but with volatile semantics
    if (intrinsic_name == "ptr_write_volatile" || intrinsic_name == "volatile_write") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string ptr_type = last_expr_type_;

            if (ptr_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + ptr + " to ptr");
                ptr = conv;
            }

            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;
            emit_line("  store volatile " + val_type + " " + val + ", ptr " + ptr);
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // ptr_read_unaligned[T](ptr) -> T
    // Like ptr_read but with align 1 (no alignment requirement)
    if (intrinsic_name == "ptr_read_unaligned") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string ptr_type = last_expr_type_;

            if (ptr_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + ptr + " to ptr");
                ptr = conv;
            }

            std::string elem_type = resolve_ptr_elem_type(call, 0);

            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_type + ", ptr " + ptr + ", align 1");
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // ptr_write_unaligned[T](ptr, val)
    // Like ptr_write but with align 1 (no alignment requirement)
    if (intrinsic_name == "ptr_write_unaligned") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string ptr_type = last_expr_type_;

            if (ptr_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + ptr + " to ptr");
                ptr = conv;
            }

            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;
            emit_line("  store " + val_type + " " + val + ", ptr " + ptr + ", align 1");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // memcpy(dst, src, size) - non-overlapping memory copy
    if (intrinsic_name == "memcpy") {
        if (call.args.size() >= 3) {
            std::string dst = gen_expr(*call.args[0]);
            std::string dst_type = last_expr_type_;
            if (dst_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + dst + " to ptr");
                dst = conv;
            }

            std::string src = gen_expr(*call.args[1]);
            std::string src_type = last_expr_type_;
            if (src_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + src + " to ptr");
                src = conv;
            }

            std::string size = gen_expr(*call.args[2]);
            std::string size_type = last_expr_type_;
            std::string size64 = size;
            if (size_type == "i32") {
                size64 = fresh_reg();
                emit_line("  " + size64 + " = sext i32 " + size + " to i64");
            }

            emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " +
                      size64 + ", i1 false)");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // memmove(dst, src, size) - potentially overlapping memory copy
    if (intrinsic_name == "memmove") {
        if (call.args.size() >= 3) {
            std::string dst = gen_expr(*call.args[0]);
            std::string dst_type = last_expr_type_;
            if (dst_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + dst + " to ptr");
                dst = conv;
            }

            std::string src = gen_expr(*call.args[1]);
            std::string src_type = last_expr_type_;
            if (src_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + src + " to ptr");
                src = conv;
            }

            std::string size = gen_expr(*call.args[2]);
            std::string size_type = last_expr_type_;
            std::string size64 = size;
            if (size_type == "i32") {
                size64 = fresh_reg();
                emit_line("  " + size64 + " = sext i32 " + size + " to i64");
            }

            emit_line("  call void @llvm.memmove.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " +
                      size64 + ", i1 false)");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // memset(dst, value, size) - fill memory with byte value
    if (intrinsic_name == "memset") {
        if (call.args.size() >= 3) {
            std::string dst = gen_expr(*call.args[0]);
            std::string dst_type = last_expr_type_;
            if (dst_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + dst + " to ptr");
                dst = conv;
            }

            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;
            // Truncate to i8 if needed
            std::string val8 = val;
            if (val_type != "i8") {
                val8 = fresh_reg();
                emit_line("  " + val8 + " = trunc " + val_type + " " + val + " to i8");
            }

            std::string size = gen_expr(*call.args[2]);
            std::string size_type = last_expr_type_;
            std::string size64 = size;
            if (size_type == "i32") {
                size64 = fresh_reg();
                emit_line("  " + size64 + " = sext i32 " + size + " to i64");
            }

            emit_line("  call void @llvm.memset.p0.i64(ptr " + dst + ", i8 " + val8 + ", i64 " +
                      size64 + ", i1 false)");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // Helper: coerce a value to ptr type for memory intrinsics.
    // RawPtr[T]/RawMutPtr[T] structs wrap an i64 addr field — extract and inttoptr.
    auto coerce_to_ptr = [&](std::string val, const std::string& val_type) -> std::string {
        if (val_type == "ptr")
            return val;
        if (val_type == "i64") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = inttoptr i64 " + val + " to ptr");
            return conv;
        }
        // Struct types like %struct.RawMutPtr__I64 — extract field 0 (i64 addr), then inttoptr
        if (val_type.find("%struct.") != std::string::npos ||
            val_type.find("{ i64 }") != std::string::npos) {
            std::string addr = fresh_reg();
            emit_line("  " + addr + " = extractvalue " + val_type + " " + val + ", 0");
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = inttoptr i64 " + addr + " to ptr");
            return conv;
        }
        return val;
    };

    // copy_nonoverlapping[T](src: Ptr[T], dst: Ptr[T], count: I64)
    // ptr_copy[T](src: Ptr[T], dst: Ptr[T], count: I64) - alias
    // Copies count*sizeof(T) bytes from src to dst. Regions must NOT overlap.
    if (intrinsic_name == "copy_nonoverlapping" || intrinsic_name == "ptr_copy") {
        if (call.args.size() >= 3) {
            std::string src = gen_expr(*call.args[0]);
            std::string src_type = last_expr_type_;
            std::string dst = gen_expr(*call.args[1]);
            std::string dst_type = last_expr_type_;
            std::string count = gen_expr(*call.args[2]);
            std::string count_type = last_expr_type_;

            // Resolve element size from type parameter [T]
            int elem_size = 1; // Default to byte
            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.generics && !path_expr.generics->args.empty()) {
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        auto resolved =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        std::string type_llvm = llvm_type_from_semantic(resolved);
                        if (type_llvm == "i8")
                            elem_size = 1;
                        else if (type_llvm == "i16")
                            elem_size = 2;
                        else if (type_llvm == "i32" || type_llvm == "float")
                            elem_size = 4;
                        else if (type_llvm == "i64" || type_llvm == "double" || type_llvm == "ptr")
                            elem_size = 8;
                        else if (type_llvm == "i128")
                            elem_size = 16;
                    }
                }
            }

            // Convert count to i64 if needed
            std::string count64 = count;
            if (count_type == "i32") {
                count64 = fresh_reg();
                emit_line("  " + count64 + " = sext i32 " + count + " to i64");
            }

            // Compute byte_count = count * elem_size
            std::string byte_count;
            if (elem_size == 1) {
                byte_count = count64;
            } else {
                byte_count = fresh_reg();
                emit_line("  " + byte_count + " = mul i64 " + count64 + ", " +
                          std::to_string(elem_size));
            }

            src = coerce_to_ptr(src, src_type);
            dst = coerce_to_ptr(dst, dst_type);
            emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " +
                      byte_count + ", i1 false)");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // copy[T](src: Ptr[T], dst: Ptr[T], count: I64)
    // Copies count*sizeof(T) bytes from src to dst. Safe for overlapping regions.
    if (intrinsic_name == "copy") {
        if (call.args.size() >= 3) {
            std::string src = gen_expr(*call.args[0]);
            std::string src_type = last_expr_type_;
            std::string dst = gen_expr(*call.args[1]);
            std::string dst_type = last_expr_type_;
            std::string count = gen_expr(*call.args[2]);
            std::string count_type = last_expr_type_;

            // Resolve element size from type parameter [T]
            int elem_size = 1;
            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.generics && !path_expr.generics->args.empty()) {
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        auto resolved =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        std::string type_llvm = llvm_type_from_semantic(resolved);
                        if (type_llvm == "i8")
                            elem_size = 1;
                        else if (type_llvm == "i16")
                            elem_size = 2;
                        else if (type_llvm == "i32" || type_llvm == "float")
                            elem_size = 4;
                        else if (type_llvm == "i64" || type_llvm == "double" || type_llvm == "ptr")
                            elem_size = 8;
                        else if (type_llvm == "i128")
                            elem_size = 16;
                    }
                }
            }

            // Convert count to i64 if needed
            std::string count64 = count;
            if (count_type == "i32") {
                count64 = fresh_reg();
                emit_line("  " + count64 + " = sext i32 " + count + " to i64");
            }

            // Compute byte_count = count * elem_size
            std::string byte_count;
            if (elem_size == 1) {
                byte_count = count64;
            } else {
                byte_count = fresh_reg();
                emit_line("  " + byte_count + " = mul i64 " + count64 + ", " +
                          std::to_string(elem_size));
            }

            src = coerce_to_ptr(src, src_type);
            dst = coerce_to_ptr(dst, dst_type);
            emit_line("  call void @llvm.memmove.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " +
                      byte_count + ", i1 false)");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // write_bytes[T](dst: Ptr[T], val: U8, count: I64)
    // Sets count*sizeof(T) bytes at dst to val.
    if (intrinsic_name == "write_bytes") {
        if (call.args.size() >= 3) {
            std::string dst = gen_expr(*call.args[0]);
            std::string dst_type = last_expr_type_;
            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;
            std::string count = gen_expr(*call.args[2]);
            std::string count_type = last_expr_type_;

            // Resolve element size from type parameter [T]
            int elem_size = 1;
            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.generics && !path_expr.generics->args.empty()) {
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        auto resolved =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        std::string type_llvm = llvm_type_from_semantic(resolved);
                        if (type_llvm == "i8")
                            elem_size = 1;
                        else if (type_llvm == "i16")
                            elem_size = 2;
                        else if (type_llvm == "i32" || type_llvm == "float")
                            elem_size = 4;
                        else if (type_llvm == "i64" || type_llvm == "double" || type_llvm == "ptr")
                            elem_size = 8;
                        else if (type_llvm == "i128")
                            elem_size = 16;
                    }
                }
            }

            // Convert count to i64 if needed
            std::string count64 = count;
            if (count_type == "i32") {
                count64 = fresh_reg();
                emit_line("  " + count64 + " = sext i32 " + count + " to i64");
            }

            // Compute byte_count = count * elem_size
            std::string byte_count;
            if (elem_size == 1) {
                byte_count = count64;
            } else {
                byte_count = fresh_reg();
                emit_line("  " + byte_count + " = mul i64 " + count64 + ", " +
                          std::to_string(elem_size));
            }

            // Truncate val to i8 if needed
            std::string val8 = val;
            if (val_type != "i8") {
                val8 = fresh_reg();
                emit_line("  " + val8 + " = trunc " + val_type + " " + val + " to i8");
            }

            dst = coerce_to_ptr(dst, dst_type);
            emit_line("  call void @llvm.memset.p0.i64(ptr " + dst + ", i8 " + val8 + ", i64 " +
                      byte_count + ", i1 false)");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // ptr_offset[T](ptr: Ptr[T], count: I64) -> Ptr[T]
    // Also handles ptr_offset(ptr: mut ref T, count: I32) -> mut ref T for memory tests
    if (intrinsic_name == "ptr_offset") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);

            // Infer element type from Ptr[T] or ref T
            // Default to i32 for *Unit (void*) to match I32-sized memory operations
            std::string elem_type = "i32";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type) {
                auto is_unit_type = [](const types::TypePtr& t) {
                    if (t && t->is<types::PrimitiveType>()) {
                        return t->as<types::PrimitiveType>().kind == types::PrimitiveKind::Unit;
                    }
                    return false;
                };

                if (arg_type->is<types::PtrType>()) {
                    auto inner = arg_type->as<types::PtrType>().inner;
                    // For *Unit (void*), default to i32 (for I32 memory operations)
                    if (!is_unit_type(inner)) {
                        elem_type = llvm_type_from_semantic(inner);
                    }
                } else if (arg_type->is<types::RefType>()) {
                    auto inner = arg_type->as<types::RefType>().inner;
                    if (!is_unit_type(inner)) {
                        elem_type = llvm_type_from_semantic(inner);
                    }
                }
            }

            std::string count = gen_expr(*call.args[1]);
            std::string count_type = last_expr_type_;

            // Convert count to i64 if it's i32
            std::string count64 = count;
            if (count_type == "i32") {
                count64 = fresh_reg();
                emit_line("  " + count64 + " = sext i32 " + count + " to i64");
            }

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr inbounds " + elem_type + ", ptr " + ptr +
                      ", i64 " + count64);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // Slice, Array, Type Info, Unsafe Conversions, SIMD, and SSE2 intrinsics
    // Delegated to intrinsics_slice_simd.cpp
    if (auto result = try_gen_intrinsic_slice_simd(intrinsic_name, fn_name, call)) {
        return result;
    }

    // Delegate to extended intrinsics (compiler hints, checked/saturating arithmetic,
    // bit manipulation, math, reflection) in intrinsics_extended.cpp
    return try_gen_intrinsic_extended(intrinsic_name, call, fn_name);
}

} // namespace tml::codegen
