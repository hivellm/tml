TML_MODULE("codegen_x86")

//! # LLVM IR Generator - SSE2, SSE4.2, and POPCNT Intrinsics
//!
//! This file implements Native SSE2, SSE4.2 string comparison, SSE4.2 CRC32,
//! and POPCNT intrinsics.
//! Split from intrinsics_slice_simd.cpp for maintainability.
//!
//! ## Sections in this file
//!
//! - Native SSE2 Intrinsics (sse2_cmpeq_epi8, sse2_movemask_epi8)
//! - SSE2 Comparison Intrinsics (sse2_cmpgt_epi8, sse2_cmplt_epi8, etc.)
//! - SSE2 Bitwise Intrinsics (sse2_and_si128, sse2_or_si128, etc.)
//! - SSE2 Min/Max Intrinsics (sse2_min_epu8, sse2_max_epu8, etc.)
//! - SSE2 Movemask Intrinsics (sse2_movemask_ps, sse2_movemask_pd)
//! - SSE2 Pack/Unpack Intrinsics (sse2_packs_epi16, sse2_unpacklo_epi8, etc.)
//! - SSE2 Shift Intrinsics (sse2_slli_epi16, sse2_srli_epi32, etc.)
//! - SSE2 Memory Intrinsics (sse2_storeu_si128, sse2_store_si128)
//! - SSE4.2 String Comparison Intrinsics (sse42_cmpistrm, sse42_cmpistri, etc.)
//! - SSE4.2 CRC32 Intrinsics (sse42_crc32_u8, sse42_crc32_u64, etc.)
//! - POPCNT Intrinsics (popcnt_u32, popcnt_u64)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Handles Native SSE2, SSE4.2, and POPCNT intrinsics.
/// Called from try_gen_intrinsic_slice_simd() after the SIMD vector section.
///
/// Parameters follow the same convention as try_gen_intrinsic:
///   intrinsic_name — base name with module prefix stripped (e.g. "sse2_cmpeq_epi8")
///   fn_name        — the raw qualified function name passed by the caller
///   call           — the parsed call expression including arguments and callee
auto LLVMIRGen::try_gen_simd_sse_intrinsic(const std::string& intrinsic_name,
                                           const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // ============================================================================
    // Native SSE2 Intrinsics (x86-64)
    // ============================================================================

    // sse2_cmpeq_epi8(a: <16 x i8>, b: <16 x i8>) -> <16 x i8>
    // Byte-wise equality comparison. Returns 0xFF for match, 0x00 for no match.
    // Compiles to PCMPEQB on x86.
    if (intrinsic_name == "sse2_cmpeq_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // icmp eq produces <16 x i1>, sext to <16 x i8> gives 0xFF/0x00 per lane
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i8>");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_movemask_epi8(v: <16 x i8>) -> I32
    // Extracts the most significant bit of each byte into a 16-bit integer mask.
    // Compiles to PMOVMSKB on x86.
    if (intrinsic_name == "sse2_movemask_epi8") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse2.pmovmskb.128(<16 x i8> " + vec +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Comparison Intrinsics (2.1)
    // ============================================================================

    // sse2_cmpgt_epi8(a, b) -> <16 x i8>  — PCMPGTB (signed byte greater-than)
    if (intrinsic_name == "sse2_cmpgt_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i8>");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_cmplt_epi8(a, b) -> <16 x i8>  — via PCMPGTB with swapped args
    if (intrinsic_name == "sse2_cmplt_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp slt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i8>");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_cmpeq_epi16(a, b) -> <8 x i16>  — PCMPEQW (word equal)
    if (intrinsic_name == "sse2_cmpeq_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i16>");
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_cmpeq_epi32(a, b) -> <4 x i32>  — PCMPEQD (dword equal)
    if (intrinsic_name == "sse2_cmpeq_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <4 x i1> " + cmp + " to <4 x i32>");
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // sse2_cmpgt_epi16(a, b) -> <8 x i16>  — PCMPGTW (signed word greater-than)
    if (intrinsic_name == "sse2_cmpgt_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i16>");
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_cmpgt_epi32(a, b) -> <4 x i32>  — PCMPGTD (signed dword greater-than)
    if (intrinsic_name == "sse2_cmpgt_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <4 x i1> " + cmp + " to <4 x i32>");
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Bitwise Intrinsics (2.2)
    // ============================================================================

    // sse2_and_si128(a, b) -> <2 x i64>  — PAND (128-bit AND)
    if (intrinsic_name == "sse2_and_si128") {
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

    // sse2_or_si128(a, b) -> <2 x i64>  — POR (128-bit OR)
    if (intrinsic_name == "sse2_or_si128") {
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

    // sse2_xor_si128(a, b) -> <2 x i64>  — PXOR (128-bit XOR)
    if (intrinsic_name == "sse2_xor_si128") {
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

    // sse2_andnot_si128(a, b) -> <2 x i64>  — PANDN (~a & b)
    if (intrinsic_name == "sse2_andnot_si128") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // PANDN = ~a & b  (NOT a, then AND with b)
            std::string not_a = fresh_reg();
            emit_line("  " + not_a + " = xor " + a_type + " " + a + ", <i64 -1, i64 -1>");
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + a_type + " " + not_a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Min/Max Intrinsics (2.3)
    // ============================================================================

    // sse2_min_epu8(a, b) -> <16 x i8>  — PMINUB (unsigned byte min)
    if (intrinsic_name == "sse2_min_epu8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp ult " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <16 x i1> " + cmp + ", " + a_type + " " + a +
                      ", " + a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_max_epu8(a, b) -> <16 x i8>  — PMAXUB (unsigned byte max)
    if (intrinsic_name == "sse2_max_epu8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp ugt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <16 x i1> " + cmp + ", " + a_type + " " + a +
                      ", " + a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_min_epi16(a, b) -> <8 x i16>  — PMINSW (signed word min)
    if (intrinsic_name == "sse2_min_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp slt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <8 x i1> " + cmp + ", " + a_type + " " + a + ", " +
                      a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_max_epi16(a, b) -> <8 x i16>  — PMAXSW (signed word max)
    if (intrinsic_name == "sse2_max_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <8 x i1> " + cmp + ", " + a_type + " " + a + ", " +
                      a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Movemask Intrinsics (2.4)
    // ============================================================================

    // sse2_movemask_ps(v: <4 x float>) -> I32  — MOVMSKPS
    if (intrinsic_name == "sse2_movemask_ps") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse.movmsk.ps(<4 x float> " + vec +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse2_movemask_pd(v: <2 x double>) -> I32  — MOVMSKPD
    if (intrinsic_name == "sse2_movemask_pd") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse2.movmsk.pd(<2 x double> " + vec +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Pack/Unpack Intrinsics (2.5)
    // ============================================================================

    // sse2_packs_epi16(a, b) -> <16 x i8>  — PACKSSWB (i16 -> i8 signed saturation)
    if (intrinsic_name == "sse2_packs_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse2.packsswb.128(<8 x i16> " +
                      a + ", <8 x i16> " + b + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_packus_epi16(a, b) -> <16 x i8>  — PACKUSWB (i16 -> u8 unsigned saturation)
    if (intrinsic_name == "sse2_packus_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse2.packuswb.128(<8 x i16> " +
                      a + ", <8 x i16> " + b + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_packs_epi32(a, b) -> <8 x i16>  — PACKSSDW (i32 -> i16 signed saturation)
    if (intrinsic_name == "sse2_packs_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x i16> @llvm.x86.sse2.packssdw.128(<4 x i32> " +
                      a + ", <4 x i32> " + b + ")");
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_unpacklo_epi8(a, b) -> <16 x i8>  — PUNPCKLBW (interleave low bytes)
    if (intrinsic_name == "sse2_unpacklo_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // shufflevector with mask [0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23]
            std::string result = fresh_reg();
            emit_line("  " + result + " = shufflevector " + a_type + " " + a + ", " + a_type + " " +
                      b +
                      ", <16 x i32> <i32 0, i32 16, i32 1, i32 17, i32 2, i32 18, i32 3, i32 19, "
                      "i32 4, i32 20, i32 5, i32 21, i32 6, i32 22, i32 7, i32 23>");
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_unpackhi_epi8(a, b) -> <16 x i8>  — PUNPCKHBW (interleave high bytes)
    if (intrinsic_name == "sse2_unpackhi_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // shufflevector with mask [8,24,9,25,10,26,11,27,12,28,13,29,14,30,15,31]
            std::string result = fresh_reg();
            emit_line("  " + result + " = shufflevector " + a_type + " " + a + ", " + a_type + " " +
                      b +
                      ", <16 x i32> <i32 8, i32 24, i32 9, i32 25, i32 10, i32 26, i32 11, i32 27, "
                      "i32 12, i32 28, i32 13, i32 29, i32 14, i32 30, i32 15, i32 31>");
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Shift Intrinsics (2.6)
    // ============================================================================

    // sse2_slli_epi16(v, imm) -> <8 x i16>  — Shift left immediate (16-bit lanes)
    if (intrinsic_name == "sse2_slli_epi16") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string vec_type = last_expr_type_;
            std::string imm = gen_expr(*call.args[1]);
            // Splat the shift amount to all lanes
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <8 x i16> undef, i16 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <8 x i16> " + splat +
                      ", <8 x i16> undef, <8 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <8 x i16> " + vec + ", " + splat2);
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_slli_epi32(v, imm) -> <4 x i32>  — Shift left immediate (32-bit lanes)
    if (intrinsic_name == "sse2_slli_epi32") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <4 x i32> undef, i32 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <4 x i32> " + splat +
                      ", <4 x i32> undef, <4 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <4 x i32> " + vec + ", " + splat2);
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // sse2_slli_epi64(v, imm) -> <2 x i64>  — Shift left immediate (64-bit lanes)
    if (intrinsic_name == "sse2_slli_epi64") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <2 x i64> undef, i64 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <2 x i64> " + splat +
                      ", <2 x i64> undef, <2 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <2 x i64> " + vec + ", " + splat2);
            last_expr_type_ = "<2 x i64>";
            return result;
        }
        return "0";
    }

    // sse2_srli_epi16(v, imm) -> <8 x i16>  — Shift right logical immediate (16-bit)
    if (intrinsic_name == "sse2_srli_epi16") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <8 x i16> undef, i16 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <8 x i16> " + splat +
                      ", <8 x i16> undef, <8 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <8 x i16> " + vec + ", " + splat2);
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_srli_epi32(v, imm) -> <4 x i32>  — Shift right logical immediate (32-bit)
    if (intrinsic_name == "sse2_srli_epi32") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <4 x i32> undef, i32 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <4 x i32> " + splat +
                      ", <4 x i32> undef, <4 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <4 x i32> " + vec + ", " + splat2);
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // sse2_srli_epi64(v, imm) -> <2 x i64>  — Shift right logical immediate (64-bit)
    if (intrinsic_name == "sse2_srli_epi64") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <2 x i64> undef, i64 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <2 x i64> " + splat +
                      ", <2 x i64> undef, <2 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <2 x i64> " + vec + ", " + splat2);
            last_expr_type_ = "<2 x i64>";
            return result;
        }
        return "0";
    }

    // sse2_srai_epi16(v, imm) -> <8 x i16>  — Shift right arithmetic immediate (16-bit)
    if (intrinsic_name == "sse2_srai_epi16") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <8 x i16> undef, i16 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <8 x i16> " + splat +
                      ", <8 x i16> undef, <8 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = ashr <8 x i16> " + vec + ", " + splat2);
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_srai_epi32(v, imm) -> <4 x i32>  — Shift right arithmetic immediate (32-bit)
    if (intrinsic_name == "sse2_srai_epi32") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <4 x i32> undef, i32 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <4 x i32> " + splat +
                      ", <4 x i32> undef, <4 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = ashr <4 x i32> " + vec + ", " + splat2);
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Memory Intrinsics (2.7)
    // ============================================================================

    // sse2_storeu_si128(ptr, v)  — Unaligned 128-bit store
    if (intrinsic_name == "sse2_storeu_si128") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string vec = gen_expr(*call.args[1]);
            std::string vec_type = last_expr_type_;
            emit_line("  store " + vec_type + " " + vec + ", ptr " + ptr + ", align 1");
            last_expr_type_ = "void";
            return "";
        }
        return "0";
    }

    // sse2_store_si128(ptr, v)  — Aligned 128-bit store
    if (intrinsic_name == "sse2_store_si128") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string vec = gen_expr(*call.args[1]);
            std::string vec_type = last_expr_type_;
            emit_line("  store " + vec_type + " " + vec + ", ptr " + ptr + ", align 16");
            last_expr_type_ = "void";
            return "";
        }
        return "0";
    }

    // ============================================================================
    // SSE4.2 String Comparison Intrinsics (3.1)
    // ============================================================================

    // sse42_cmpistrm(a, b, imm) -> <16 x i8>  — PCMPISTRM
    // Implicit-length string compare, returns byte mask.
    // imm8 controls comparison mode (see Intel manual).
    if (intrinsic_name == "sse42_cmpistrm") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string imm = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse42.pcmpistrm128(<16 x i8> " +
                      a + ", <16 x i8> " + b + ", i8 " + imm + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse42_cmpistri(a, b, imm) -> I32  — PCMPISTRI
    // Implicit-length string compare, returns index of first match/mismatch.
    if (intrinsic_name == "sse42_cmpistri") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string imm = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.pcmpistri128(<16 x i8> " + a +
                      ", <16 x i8> " + b + ", i8 " + imm + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_cmpestrm(a, la, b, lb, imm) -> <16 x i8>  — PCMPESTRM
    // Explicit-length string compare, returns byte mask.
    if (intrinsic_name == "sse42_cmpestrm") {
        if (call.args.size() >= 5) {
            std::string a = gen_expr(*call.args[0]);
            std::string la = gen_expr(*call.args[1]);
            std::string b = gen_expr(*call.args[2]);
            std::string lb = gen_expr(*call.args[3]);
            std::string imm = gen_expr(*call.args[4]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse42.pcmpestrm128(<16 x i8> " +
                      a + ", i32 " + la + ", <16 x i8> " + b + ", i32 " + lb + ", i8 " + imm + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse42_cmpestri(a, la, b, lb, imm) -> I32  — PCMPESTRI
    // Explicit-length string compare, returns index.
    if (intrinsic_name == "sse42_cmpestri") {
        if (call.args.size() >= 5) {
            std::string a = gen_expr(*call.args[0]);
            std::string la = gen_expr(*call.args[1]);
            std::string b = gen_expr(*call.args[2]);
            std::string lb = gen_expr(*call.args[3]);
            std::string imm = gen_expr(*call.args[4]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.pcmpestri128(<16 x i8> " + a +
                      ", i32 " + la + ", <16 x i8> " + b + ", i32 " + lb + ", i8 " + imm + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE4.2 CRC32 Intrinsics (3.2)
    // ============================================================================

    // sse42_crc32_u8(crc: U32, data: U8) -> U32
    if (intrinsic_name == "sse42_crc32_u8") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.crc32.32.8(i32 " + crc +
                      ", i8 " + data + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_crc32_u16(crc: U32, data: U16) -> U32
    if (intrinsic_name == "sse42_crc32_u16") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.crc32.32.16(i32 " + crc +
                      ", i16 " + data + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_crc32_u32(crc: U32, data: U32) -> U32
    if (intrinsic_name == "sse42_crc32_u32") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.crc32.32.32(i32 " + crc +
                      ", i32 " + data + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_crc32_u64(crc: U64, data: U64) -> U64
    if (intrinsic_name == "sse42_crc32_u64") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @llvm.x86.sse42.crc32.64.64(i64 " + crc +
                      ", i64 " + data + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // POPCNT Intrinsics (3.3)
    // ============================================================================

    // popcnt_u32(val: U32) -> U32  — count set bits
    if (intrinsic_name == "popcnt_u32") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.ctpop.i32(i32 " + val + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // popcnt_u64(val: U64) -> U64  — count set bits
    if (intrinsic_name == "popcnt_u64") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @llvm.ctpop.i64(i64 " + val + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
