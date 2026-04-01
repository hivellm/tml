TML_MODULE("codegen_x86")

//! # LLVM IR Generator - AVX2 and FMA Intrinsics
//!
//! This file implements AVX2 and FMA (Fused Multiply-Add) intrinsics.
//! Split from intrinsics_slice_simd.cpp for maintainability.
//!
//! ## Sections in this file
//!
//! - AVX2 Comparison Intrinsics (avx2_cmpeq_epi8, avx2_cmpgt_epi32, etc.)
//! - AVX2 Bitwise & Movemask Intrinsics (avx2_and_si256, avx2_movemask_epi8, etc.)
//! - AVX2 Shuffle & Permute Intrinsics (avx2_shuffle_epi8, avx2_permute4x64_epi64, etc.)
//! - AVX2 Horizontal & Pack Intrinsics (avx2_hadd_epi16, avx2_packs_epi32, etc.)
//! - AVX2 Gather Intrinsics (avx2_gather_epi32, avx2_gather_epi64, avx2_gather_ps)
//! - AVX2 Variable Shift Intrinsics (avx2_sllv_epi32, avx2_srlv_epi64, etc.)
//! - FMA Intrinsics (fma_fmadd_ps, fma_fmsub_pd, fma_fnmadd_ps, etc.)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Handles AVX2 and FMA intrinsics.
/// Called from try_gen_intrinsic_slice_simd() after the SSE section.
///
/// Parameters follow the same convention as try_gen_intrinsic:
///   intrinsic_name — base name with module prefix stripped (e.g. "avx2_cmpeq_epi8")
///   fn_name        — the raw qualified function name passed by the caller
///   call           — the parsed call expression including arguments and callee
auto LLVMIRGen::try_gen_simd_avx_intrinsic(const std::string& intrinsic_name,
                                           const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // ============================================================================
    // AVX2 Comparison Intrinsics (4.3)
    // ============================================================================

    // avx2_cmpeq_epi8(a, b) -> <32 x i8>  — VPCMPEQB
    if (intrinsic_name == "avx2_cmpeq_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <32 x i1> " + cmp + " to <32 x i8>");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_cmpeq_epi16(a, b) -> <16 x i16>  — VPCMPEQW
    if (intrinsic_name == "avx2_cmpeq_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i16>");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_cmpeq_epi32(a, b) -> <8 x i32>  — VPCMPEQD
    if (intrinsic_name == "avx2_cmpeq_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i32>");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_cmpgt_epi8(a, b) -> <32 x i8>  — VPCMPGTB
    if (intrinsic_name == "avx2_cmpgt_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <32 x i1> " + cmp + " to <32 x i8>");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_cmpgt_epi16(a, b) -> <16 x i16>  — VPCMPGTW
    if (intrinsic_name == "avx2_cmpgt_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i16>");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_cmpgt_epi32(a, b) -> <8 x i32>  — VPCMPGTD
    if (intrinsic_name == "avx2_cmpgt_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i32>");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Bitwise & Movemask Intrinsics (4.4)
    // ============================================================================

    // avx2_and_si256(a, b)  — VPAND
    if (intrinsic_name == "avx2_and_si256") {
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

    // avx2_or_si256(a, b)  — VPOR
    if (intrinsic_name == "avx2_or_si256") {
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

    // avx2_xor_si256(a, b)  — VPXOR
    if (intrinsic_name == "avx2_xor_si256") {
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

    // avx2_movemask_epi8(v: <32 x i8>) -> I32  — VPMOVMSKB
    if (intrinsic_name == "avx2_movemask_epi8") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.avx2.pmovmskb(<32 x i8> " + vec + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Shuffle & Permute Intrinsics (4.5)
    // ============================================================================

    // avx2_shuffle_epi8(a, b) -> <32 x i8>  — VPSHUFB (in-lane byte shuffle)
    if (intrinsic_name == "avx2_shuffle_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <32 x i8> @llvm.x86.avx2.pshuf.b(<32 x i8> " + a +
                      ", <32 x i8> " + b + ")");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_permute4x64_epi64(v, idx) -> <4 x i64>  — VPERMQ via VPERMD
    // idx is a <4 x i64> vector of lane indices (each 0-3)
    // This is the variable-index form; LLVM lowers to VPERMQ when possible.
    if (intrinsic_name == "avx2_permute4x64_epi64") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string idx = gen_expr(*call.args[1]);
            // Use VPERMD on the i32 view: bitcast <4 x i64> to <8 x i32>,
            // expand index vector to <8 x i32>, then bitcast back.
            // For simplicity, use @llvm.x86.avx2.permd which does VPERMD.
            std::string bc_vec = fresh_reg();
            emit_line("  " + bc_vec + " = bitcast <4 x i64> " + vec + " to <8 x i32>");
            std::string bc_idx = fresh_reg();
            emit_line("  " + bc_idx + " = bitcast <4 x i64> " + idx + " to <8 x i32>");
            std::string permd = fresh_reg();
            emit_line("  " + permd + " = call <8 x i32> @llvm.x86.avx2.permd(<8 x i32> " + bc_vec +
                      ", <8 x i32> " + bc_idx + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = bitcast <8 x i32> " + permd + " to <4 x i64>");
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // avx2_permute2x128_si256(a, b, imm) -> <4 x i64>  — VPERM2I128
    // imm8 selects which 128-bit halves to place in the result.
    // LLVM doesn't have a direct intrinsic; use shufflevector patterns.
    // imm8 = 0x20: [a_lo, b_lo], 0x31: [a_hi, b_hi], 0x03: [b_lo, a_hi], etc.
    if (intrinsic_name == "avx2_permute2x128_si256") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string imm = gen_expr(*call.args[2]);
            // For the common case imm=0x20 (interleave low halves):
            // shufflevector <4 x i64> a, <4 x i64> b, <i32 0, i32 1, i32 4, i32 5>
            // Since we need runtime imm support, emit a fallback using
            // extractelement/insertelement. For now, just pass through as identity (will be
            // extended later). Use inline asm for exact VPERM2I128 behavior:
            std::string result = fresh_reg();
            // Emit as a generic bitwise operation placeholder that LLVM can optimize
            // This is a simplified version — for now return the first operand
            emit_line("  " + result + " = shufflevector <4 x i64> " + a + ", <4 x i64> " + b +
                      ", <4 x i32> <i32 0, i32 1, i32 4, i32 5>");
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Horizontal & Pack Intrinsics (4.6)
    // ============================================================================

    // avx2_hadd_epi16(a, b) -> <16 x i16>  — VPHADDW (horizontal add pairs)
    if (intrinsic_name == "avx2_hadd_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i16> @llvm.x86.avx2.phadd.w(<16 x i16> " + a +
                      ", <16 x i16> " + b + ")");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_hadd_epi32(a, b) -> <8 x i32>  — VPHADDD (horizontal add pairs)
    if (intrinsic_name == "avx2_hadd_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x i32> @llvm.x86.avx2.phadd.d(<8 x i32> " + a +
                      ", <8 x i32> " + b + ")");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_packs_epi16(a, b) -> <32 x i8>  — VPACKSSWB (i16->i8 signed saturation)
    if (intrinsic_name == "avx2_packs_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <32 x i8> @llvm.x86.avx2.packsswb(<16 x i16> " + a +
                      ", <16 x i16> " + b + ")");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_packs_epi32(a, b) -> <16 x i16>  — VPACKSSDW (i32->i16 signed saturation)
    if (intrinsic_name == "avx2_packs_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i16> @llvm.x86.avx2.packssdw(<8 x i32> " + a +
                      ", <8 x i32> " + b + ")");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_packus_epi16(a, b) -> <32 x i8>  — VPACKUSWB (i16->u8 unsigned saturation)
    if (intrinsic_name == "avx2_packus_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <32 x i8> @llvm.x86.avx2.packuswb(<16 x i16> " + a +
                      ", <16 x i16> " + b + ")");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_packus_epi32(a, b) -> <16 x i16>  — VPACKUSDW (i32->u16 unsigned saturation)
    if (intrinsic_name == "avx2_packus_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i16> @llvm.x86.avx2.packusdw(<8 x i32> " + a +
                      ", <8 x i32> " + b + ")");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Gather Intrinsics (4.7)
    // ============================================================================

    // avx2_gather_epi32(base_ptr, indices, mask, scale) -> <8 x i32>  — VPGATHERDD
    // Uses llvm.masked.gather (target-independent; LLVM lowers to VPGATHERDD with +avx2)
    if (intrinsic_name == "avx2_gather_epi32") {
        if (call.args.size() >= 4) {
            std::string base = gen_expr(*call.args[0]);     // ptr
            std::string indices = gen_expr(*call.args[1]);  // <8 x i32>
            std::string mask_vec = gen_expr(*call.args[2]); // <8 x i32> (-1 = active)
            // scale arg consumed by GEP element size (i32 = 4 bytes)
            // Build vector of pointers: GEP base by each index
            std::string ptrs = fresh_reg();
            emit_line("  " + ptrs + " = getelementptr i32, ptr " + base + ", <8 x i32> " + indices);
            // Convert <8 x i32> mask to <8 x i1>
            std::string mask_cmp = fresh_reg();
            emit_line("  " + mask_cmp + " = icmp ne <8 x i32> " + mask_vec + ", zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = call <8 x i32> @llvm.masked.gather.v8i32.v8p0("
                      "<8 x ptr> " +
                      ptrs + ", i32 4, <8 x i1> " + mask_cmp + ", <8 x i32> zeroinitializer)");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_gather_epi64(base_ptr, indices, mask, scale) -> <4 x i64>  — VPGATHERDQ
    if (intrinsic_name == "avx2_gather_epi64") {
        if (call.args.size() >= 4) {
            std::string base = gen_expr(*call.args[0]);     // ptr
            std::string indices = gen_expr(*call.args[1]);  // <4 x i32>
            std::string mask_vec = gen_expr(*call.args[2]); // <4 x i64> (-1 = active)
            // sext <4 x i32> indices to <4 x i64> for GEP
            std::string idx64 = fresh_reg();
            emit_line("  " + idx64 + " = sext <4 x i32> " + indices + " to <4 x i64>");
            std::string ptrs = fresh_reg();
            emit_line("  " + ptrs + " = getelementptr i64, ptr " + base + ", <4 x i64> " + idx64);
            std::string mask_cmp = fresh_reg();
            emit_line("  " + mask_cmp + " = icmp ne <4 x i64> " + mask_vec + ", zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = call <4 x i64> @llvm.masked.gather.v4i64.v4p0("
                      "<4 x ptr> " +
                      ptrs + ", i32 8, <4 x i1> " + mask_cmp + ", <4 x i64> zeroinitializer)");
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // avx2_gather_ps(base_ptr, indices, mask, scale) -> <8 x float>  — VGATHERDPS
    if (intrinsic_name == "avx2_gather_ps") {
        if (call.args.size() >= 4) {
            std::string base = gen_expr(*call.args[0]);     // ptr
            std::string indices = gen_expr(*call.args[1]);  // <8 x i32>
            std::string mask_vec = gen_expr(*call.args[2]); // <8 x i32> (as int mask)
            std::string ptrs = fresh_reg();
            emit_line("  " + ptrs + " = getelementptr float, ptr " + base + ", <8 x i32> " +
                      indices);
            std::string mask_cmp = fresh_reg();
            emit_line("  " + mask_cmp + " = icmp ne <8 x i32> " + mask_vec + ", zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = call <8 x float> @llvm.masked.gather.v8f32.v8p0("
                      "<8 x ptr> " +
                      ptrs + ", i32 4, <8 x i1> " + mask_cmp + ", <8 x float> zeroinitializer)");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Variable Shift Intrinsics (4.8)
    // ============================================================================

    // avx2_sllv_epi32(a, shift) -> <8 x i32>  — VPSLLVD (per-lane shift left)
    if (intrinsic_name == "avx2_sllv_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <8 x i32> " + a + ", " + b);
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_sllv_epi64(a, shift) -> <4 x i64>  — VPSLLVQ (per-lane shift left)
    if (intrinsic_name == "avx2_sllv_epi64") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <4 x i64> " + a + ", " + b);
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // avx2_srlv_epi32(a, shift) -> <8 x i32>  — VPSRLVD (per-lane shift right)
    if (intrinsic_name == "avx2_srlv_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <8 x i32> " + a + ", " + b);
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_srlv_epi64(a, shift) -> <4 x i64>  — VPSRLVQ (per-lane shift right)
    if (intrinsic_name == "avx2_srlv_epi64") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <4 x i64> " + a + ", " + b);
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // FMA Intrinsics (4.9) — Fused Multiply-Add
    // ============================================================================

    // fma_fmadd_ps(a, b, c) -> <8 x float>  — VFMADD (a*b + c)
    if (intrinsic_name == "fma_fmadd_ps") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x float> @llvm.fma.v8f32(<8 x float> " + a +
                      ", <8 x float> " + b + ", <8 x float> " + c + ")");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // fma_fmadd_pd(a, b, c) -> <4 x double>  — VFMADD (a*b + c)
    if (intrinsic_name == "fma_fmadd_pd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <4 x double> @llvm.fma.v4f64(<4 x double> " + a +
                      ", <4 x double> " + b + ", <4 x double> " + c + ")");
            last_expr_type_ = "<4 x double>";
            return result;
        }
        return "0";
    }

    // fma_fmsub_ps(a, b, c) -> <8 x float>  — VFMSUB (a*b - c = fma(a, b, -c))
    if (intrinsic_name == "fma_fmsub_ps") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_c = fresh_reg();
            emit_line("  " + neg_c + " = fneg <8 x float> " + c);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x float> @llvm.fma.v8f32(<8 x float> " + a +
                      ", <8 x float> " + b + ", <8 x float> " + neg_c + ")");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // fma_fmsub_pd(a, b, c) -> <4 x double>  — VFMSUB (a*b - c)
    if (intrinsic_name == "fma_fmsub_pd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_c = fresh_reg();
            emit_line("  " + neg_c + " = fneg <4 x double> " + c);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <4 x double> @llvm.fma.v4f64(<4 x double> " + a +
                      ", <4 x double> " + b + ", <4 x double> " + neg_c + ")");
            last_expr_type_ = "<4 x double>";
            return result;
        }
        return "0";
    }

    // fma_fnmadd_ps(a, b, c) -> <8 x float>  — VFNMADD (-a*b + c = fma(-a, b, c))
    if (intrinsic_name == "fma_fnmadd_ps") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_a = fresh_reg();
            emit_line("  " + neg_a + " = fneg <8 x float> " + a);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x float> @llvm.fma.v8f32(<8 x float> " + neg_a +
                      ", <8 x float> " + b + ", <8 x float> " + c + ")");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // fma_fnmadd_pd(a, b, c) -> <4 x double>  — VFNMADD (-a*b + c)
    if (intrinsic_name == "fma_fnmadd_pd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_a = fresh_reg();
            emit_line("  " + neg_a + " = fneg <4 x double> " + a);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <4 x double> @llvm.fma.v4f64(<4 x double> " + neg_a +
                      ", <4 x double> " + b + ", <4 x double> " + c + ")");
            last_expr_type_ = "<4 x double>";
            return result;
        }
        return "0";
    }

    // fma_fmadd_ss(a, b, c) -> F32  — Scalar FMA (a*b + c)
    if (intrinsic_name == "fma_fmadd_ss") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call float @llvm.fma.f32(float " + a + ", float " + b +
                      ", float " + c + ")");
            last_expr_type_ = "float";
            return result;
        }
        return "0";
    }

    // fma_fmadd_sd(a, b, c) -> F64  — Scalar FMA (a*b + c)
    if (intrinsic_name == "fma_fmadd_sd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @llvm.fma.f64(double " + a + ", double " + b +
                      ", double " + c + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
