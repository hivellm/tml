//! MIR Codegen - Method Call Instruction
//!
//! This file handles the emit_method_call_inst method for the MIR-based
//! code generator. It handles virtual dispatch, behavior method lookup,
//! generic method instantiation, and closure calls.
//!
//! Extracted from instructions.cpp to reduce file size.

#include "codegen/mir_codegen.hpp"

#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace tml::codegen {

// ============================================================================
// Method Call Instruction
// ============================================================================

void MirCodegen::emit_method_call_inst(const mir::MethodCallInst& i, const std::string& result_reg,
                                       const mir::InstructionData& inst) {
    std::string recv_type = i.receiver_type.empty() ? "Unknown" : i.receiver_type;
    std::string receiver = get_value_reg(i.receiver);

    // ==========================================================================
    // Inline primitive behavior methods (cmp, partial_cmp)
    // ==========================================================================
    static const std::unordered_set<std::string> signed_int_types = {"I8", "I16", "I32", "I64",
                                                                     "I128"};
    static const std::unordered_set<std::string> unsigned_int_types = {"U8", "U16", "U32", "U64",
                                                                       "U128"};
    static const std::unordered_set<std::string> float_types = {"F32", "F64"};

    bool is_signed = signed_int_types.count(recv_type) > 0;
    bool is_unsigned = unsigned_int_types.count(recv_type) > 0;
    bool is_float = float_types.count(recv_type) > 0;
    bool is_numeric = is_signed || is_unsigned || is_float;

    // Handle partial_cmp inline for numeric types - returns Maybe[Ordering]
    if (i.method_name == "partial_cmp" && is_numeric && i.args.size() == 1 && !result_reg.empty()) {
        std::string id = std::to_string(temp_counter_++);

        // Get the LLVM type for this primitive
        std::string llvm_ty;
        if (recv_type == "I8")
            llvm_ty = "i8";
        else if (recv_type == "I16")
            llvm_ty = "i16";
        else if (recv_type == "I32")
            llvm_ty = "i32";
        else if (recv_type == "I64")
            llvm_ty = "i64";
        else if (recv_type == "I128")
            llvm_ty = "i128";
        else if (recv_type == "U8")
            llvm_ty = "i8";
        else if (recv_type == "U16")
            llvm_ty = "i16";
        else if (recv_type == "U32")
            llvm_ty = "i32";
        else if (recv_type == "U64")
            llvm_ty = "i64";
        else if (recv_type == "U128")
            llvm_ty = "i128";
        else if (recv_type == "F32")
            llvm_ty = "float";
        else if (recv_type == "F64")
            llvm_ty = "double";

        // Load other value from ref
        std::string other_ref = get_value_reg(i.args[0]);
        emitln("    %other." + id + " = load " + llvm_ty + ", ptr " + other_ref);

        // Compare and determine ordering
        if (is_float) {
            emitln("    %cmp_lt." + id + " = fcmp olt " + llvm_ty + " " + receiver + ", %other." +
                   id);
            emitln("    %cmp_gt." + id + " = fcmp ogt " + llvm_ty + " " + receiver + ", %other." +
                   id);
        } else if (is_signed) {
            emitln("    %cmp_lt." + id + " = icmp slt " + llvm_ty + " " + receiver + ", %other." +
                   id);
            emitln("    %cmp_gt." + id + " = icmp sgt " + llvm_ty + " " + receiver + ", %other." +
                   id);
        } else {
            emitln("    %cmp_lt." + id + " = icmp ult " + llvm_ty + " " + receiver + ", %other." +
                   id);
            emitln("    %cmp_gt." + id + " = icmp ugt " + llvm_ty + " " + receiver + ", %other." +
                   id);
        }

        // Build Ordering value: Less=0, Equal=1, Greater=2
        emitln("    %tag_1." + id + " = select i1 %cmp_lt." + id + ", i32 0, i32 1");
        emitln("    %tag_2." + id + " = select i1 %cmp_gt." + id + ", i32 2, i32 %tag_1." + id);

        // Build Ordering struct on stack
        emitln("    %ordering_alloca." + id + " = alloca %struct.Ordering, align 4");
        emitln("    %ordering_tag_ptr." + id +
               " = getelementptr inbounds %struct.Ordering, ptr %ordering_alloca." + id +
               ", i32 0, i32 0");
        emitln("    store i32 %tag_2." + id + ", ptr %ordering_tag_ptr." + id);
        emitln("    %ordering." + id + " = load %struct.Ordering, ptr %ordering_alloca." + id);

        // Build Maybe[Ordering] = Just(ordering)
        // Maybe[Ordering] layout: { i32 tag, [4 x i8] payload } where tag 0 = Just, 1 = Nothing
        std::string maybe_type = "%struct.Maybe__Ordering";
        emitln("    %maybe_alloca." + id + " = alloca " + maybe_type + ", align 8");
        emitln("    %maybe_tag_ptr." + id + " = getelementptr inbounds " + maybe_type +
               ", ptr %maybe_alloca." + id + ", i32 0, i32 0");
        emitln("    store i32 0, ptr %maybe_tag_ptr." + id); // 0 = Just
        emitln("    %maybe_payload_ptr." + id + " = getelementptr inbounds " + maybe_type +
               ", ptr %maybe_alloca." + id + ", i32 0, i32 1");
        emitln("    store %struct.Ordering %ordering." + id + ", ptr %maybe_payload_ptr." + id);
        emitln("    " + result_reg + " = load " + maybe_type + ", ptr %maybe_alloca." + id);

        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = maybe_type;
        }
        return;
    }

    // Handle cmp inline for integer types - returns Ordering directly
    if (i.method_name == "cmp" && (is_signed || is_unsigned) && i.args.size() == 1 &&
        !result_reg.empty()) {
        std::string id = std::to_string(temp_counter_++);

        // Get the LLVM type for this primitive
        std::string llvm_ty;
        if (recv_type == "I8" || recv_type == "U8")
            llvm_ty = "i8";
        else if (recv_type == "I16" || recv_type == "U16")
            llvm_ty = "i16";
        else if (recv_type == "I32" || recv_type == "U32")
            llvm_ty = "i32";
        else if (recv_type == "I64" || recv_type == "U64")
            llvm_ty = "i64";
        else if (recv_type == "I128" || recv_type == "U128")
            llvm_ty = "i128";

        // Load other value from ref
        std::string other_ref = get_value_reg(i.args[0]);
        emitln("    %other." + id + " = load " + llvm_ty + ", ptr " + other_ref);

        // Compare and determine ordering
        if (is_signed) {
            emitln("    %cmp_lt." + id + " = icmp slt " + llvm_ty + " " + receiver + ", %other." +
                   id);
            emitln("    %cmp_gt." + id + " = icmp sgt " + llvm_ty + " " + receiver + ", %other." +
                   id);
        } else {
            emitln("    %cmp_lt." + id + " = icmp ult " + llvm_ty + " " + receiver + ", %other." +
                   id);
            emitln("    %cmp_gt." + id + " = icmp ugt " + llvm_ty + " " + receiver + ", %other." +
                   id);
        }

        // Build Ordering value: Less=0, Equal=1, Greater=2
        emitln("    %tag_1." + id + " = select i1 %cmp_lt." + id + ", i32 0, i32 1");
        emitln("    %tag_2." + id + " = select i1 %cmp_gt." + id + ", i32 2, i32 %tag_1." + id);

        // Build Ordering struct
        emitln("    " + result_reg + " = insertvalue %struct.Ordering undef, i32 %tag_2." + id +
               ", 0");

        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "%struct.Ordering";
        }
        return;
    }

    // V8-style optimization: Inline simple Text methods to avoid FFI overhead
    // This is critical for performance - each FFI call has ~10ns overhead
    // Uses select instruction for branchless code that LLVM can optimize well
    if (recv_type == "Text") {
        // Text struct layout (32 bytes total):
        // Heap mode (flags & 1 == 0):
        //   offset 0: ptr data
        //   offset 8: i64 len
        //   offset 16: i64 cap
        //   offset 24: i8 flags
        // SSO mode (flags & 1 == 1):
        //   offset 0-22: [23 x i8] data
        //   offset 23: i8 len
        //   offset 24: i8 flags

        if (i.method_name == "len" && !result_reg.empty()) {
            // Inline Text::len() using branchless select
            std::string id = std::to_string(temp_counter_++);

            // Load flags and check SSO bit
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_sso." + id + " = trunc i8 %flags." + id + " to i1");

            // Load SSO len (offset 23, i8 -> i64)
            emitln("    %sso_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 23");
            emitln("    %sso_len_i8." + id + " = load i8, ptr %sso_len_ptr." + id);
            emitln("    %sso_len." + id + " = zext i8 %sso_len_i8." + id + " to i64");

            // Load heap len (offset 8, i64)
            emitln("    %heap_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %heap_len." + id + " = load i64, ptr %heap_len_ptr." + id);

            // Branchless select: is_sso ? sso_len : heap_len
            emitln("    " + result_reg + " = select i1 %is_sso." + id + ", i64 %sso_len." + id +
                   ", i64 %heap_len." + id);

            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = "i64";
            }
            return;
        }

        if (i.method_name == "clear") {
            // Inline Text::clear() - branchless using conditional stores
            // For simplicity, we store to both locations (one will be ignored based on mode)
            // LLVM will optimize this if the mode is known
            std::string id = std::to_string(temp_counter_++);

            // Store 0 to SSO len location (offset 23)
            emitln("    %sso_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 23");
            emitln("    store i8 0, ptr %sso_len_ptr." + id);

            // Store 0 to heap len location (offset 8)
            emitln("    %heap_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    store i64 0, ptr %heap_len_ptr." + id);

            return;
        }

        if (i.method_name == "is_empty" && !result_reg.empty()) {
            // Inline Text::is_empty() using branchless select
            std::string id = std::to_string(temp_counter_++);

            // Load flags and check SSO bit
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_sso." + id + " = trunc i8 %flags." + id + " to i1");

            // Check SSO empty (offset 23)
            emitln("    %sso_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 23");
            emitln("    %sso_len." + id + " = load i8, ptr %sso_len_ptr." + id);
            emitln("    %sso_empty." + id + " = icmp eq i8 %sso_len." + id + ", 0");

            // Check heap empty (offset 8)
            emitln("    %heap_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %heap_len." + id + " = load i64, ptr %heap_len_ptr." + id);
            emitln("    %heap_empty." + id + " = icmp eq i64 %heap_len." + id + ", 0");

            // Branchless select
            emitln("    " + result_reg + " = select i1 %is_sso." + id + ", i1 %sso_empty." + id +
                   ", i1 %heap_empty." + id);

            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = "i1";
            }
            return;
        }

        if (i.method_name == "capacity" && !result_reg.empty()) {
            // Inline Text::capacity() using branchless select
            std::string id = std::to_string(temp_counter_++);

            // Load flags and check SSO bit
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_sso." + id + " = trunc i8 %flags." + id + " to i1");

            // Load heap capacity (offset 16)
            emitln("    %heap_cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %heap_cap." + id + " = load i64, ptr %heap_cap_ptr." + id);

            // Branchless select: is_sso ? 23 : heap_cap
            emitln("    " + result_reg + " = select i1 %is_sso." + id + ", i64 23, i64 %heap_cap." +
                   id);

            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = "i64";
            }
            return;
        }

        if (i.method_name == "push" && i.args.size() == 1) {
            // Inline Text::push() with fast path for heap mode
            // This is critical - push() is called millions of times in tight loops
            // Fast path: heap mode with space available -> direct store
            // Slow path: SSO mode or need realloc -> call FFI
            std::string id = std::to_string(temp_counter_++);
            std::string byte_val = get_value_reg(i.args[0]);

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %push_heap." + id + ", label %push_slow." +
                   id);

            // Heap fast path: check capacity and store directly
            emitln("  push_heap." + id + ":");
            // Load data ptr, len, cap
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);
            // Check if has space
            emitln("    %has_space." + id + " = icmp ult i64 %len." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %push_fast." + id +
                   ", label %push_slow." + id);

            // Fast store path
            emitln("  push_fast." + id + ":");
            // Truncate i32 byte to i8
            emitln("    %byte_i8." + id + " = trunc i32 " + byte_val + " to i8");
            // Store byte at data[len]
            emitln("    %store_ptr." + id + " = getelementptr i8, ptr %data_ptr." + id +
                   ", i64 %len." + id);
            emitln("    store i8 %byte_i8." + id + ", ptr %store_ptr." + id);
            // Increment len
            emitln("    %new_len." + id + " = add i64 %len." + id + ", 1");
            emitln("    store i64 %new_len." + id + ", ptr %len_ptr." + id);
            emitln("    br label %push_done." + id);

            // Slow path: SSO mode or needs realloc - call FFI
            emitln("  push_slow." + id + ":");
            emitln("    call void @tml_text_push(ptr " + receiver + ", i32 " + byte_val + ")");
            emitln("    br label %push_done." + id);

            emitln("  push_done." + id + ":");
            return;
        }

        if (i.method_name == "push_str" && i.args.size() == 1) {
            // Inline Text::push_str() with fast path for heap mode
            // push_str takes a Str argument, we need to:
            // 1. Get string ptr and len
            // 2. Check heap mode with sufficient capacity
            // 3. memcpy and update len (fast path) or call FFI (slow path)
            std::string id = std::to_string(temp_counter_++);
            std::string str_arg = get_value_reg(i.args[0]);

            // Check if argument is a constant string (compile-time length)
            auto const_it = value_string_contents_.find(i.args[0].id);
            if (const_it != value_string_contents_.end()) {
                // Constant string - use compile-time length (no FFI call!)
                size_t const_len = const_it->second.size();
                emitln("    %str_len." + id + " = add i64 0, " + std::to_string(const_len));
            } else {
                // Non-constant string - call @str_len at runtime
                emitln("    %str_len_i32." + id + " = call i32 @str_len(ptr " + str_arg + ")");
                emitln("    %str_len." + id + " = zext i32 %str_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %pstr_heap." + id + ", label %pstr_slow." +
                   id);

            // Heap path: check capacity
            emitln("  pstr_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Check if len + str_len <= cap
            emitln("    %new_len." + id + " = add i64 %len." + id + ", %str_len." + id);
            emitln("    %has_space." + id + " = icmp ule i64 %new_len." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %pstr_fast." + id +
                   ", label %pstr_slow." + id);

            // Fast memcpy path
            emitln("  pstr_fast." + id + ":");
            // dst = data + len
            emitln("    %dst." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            // memcpy(dst, str, str_len)
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst." + id + ", ptr " + str_arg +
                   ", i64 %str_len." + id + ", i1 false)");
            // Update len
            emitln("    store i64 %new_len." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pstr_done." + id);

            // Slow path: SSO mode or needs realloc - call FFI
            emitln("  pstr_slow." + id + ":");
            emitln("    call void @tml_text_push_str_len(ptr " + receiver + ", ptr " + str_arg +
                   ", i64 %str_len." + id + ")");
            emitln("    br label %pstr_done." + id);

            emitln("  pstr_done." + id + ":");
            return;
        }

        if (i.method_name == "push_i64" && i.args.size() == 1) {
            // Inline Text::push_i64() with fully inline fast path for small non-negative integers
            // Uses lookup table for direct conversion, avoiding FFI for values 0-9999
            std::string id = std::to_string(temp_counter_++);
            std::string int_val = get_value_reg(i.args[0]);

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %pi64_heap." + id + ", label %pi64_slow." +
                   id);

            // Heap path: check capacity and value range for inline conversion
            emitln("  pi64_heap." + id + ":");
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            // Check if value is in range [0, 9999] for inline conversion
            emitln("    %is_small." + id + " = icmp ult i64 " + int_val + ", 10000");
            emitln("    %is_non_neg." + id + " = icmp sge i64 " + int_val + ", 0");
            emitln("    %can_inline." + id + " = and i1 %is_small." + id + ", %is_non_neg." + id);
            // Need at most 5 bytes for values 0-9999
            emitln("    %needed." + id + " = add i64 %len." + id + ", 5");
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    %do_inline." + id + " = and i1 %can_inline." + id + ", %has_space." + id);
            emitln("    br i1 %do_inline." + id + ", label %pi64_inline." + id +
                   ", label %pi64_ffi." + id);

            // Inline fast path: direct conversion using lookup table
            emitln("  pi64_inline." + id + ":");
            // Get destination pointer
            emitln("    %dst." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            // Truncate to i32 for easier arithmetic
            emitln("    %n32." + id + " = trunc i64 " + int_val + " to i32");

            // Check digit count and branch accordingly
            emitln("    %lt10." + id + " = icmp ult i32 %n32." + id + ", 10");
            emitln("    br i1 %lt10." + id + ", label %pi64_1d." + id + ", label %pi64_ge10." + id);

            // 1 digit: n < 10
            emitln("  pi64_1d." + id + ":");
            emitln("    %d1." + id + " = add i32 %n32." + id + ", 48"); // '0' = 48
            emitln("    %d1_8." + id + " = trunc i32 %d1." + id + " to i8");
            emitln("    store i8 %d1_8." + id + ", ptr %dst." + id);
            emitln("    %newlen1." + id + " = add i64 %len." + id + ", 1");
            emitln("    store i64 %newlen1." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // >= 10: check if < 100
            emitln("  pi64_ge10." + id + ":");
            emitln("    %lt100." + id + " = icmp ult i32 %n32." + id + ", 100");
            emitln("    br i1 %lt100." + id + ", label %pi64_2d." + id + ", label %pi64_ge100." +
                   id);

            // 2 digits: 10 <= n < 100, use lookup table
            emitln("  pi64_2d." + id + ":");
            emitln("    %idx2." + id + " = mul i32 %n32." + id + ", 2");
            emitln("    %idx2_64." + id + " = zext i32 %idx2." + id + " to i64");
            emitln("    %pair2_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx2_64." + id);
            emitln("    %pair2." + id + " = load i16, ptr %pair2_ptr." + id);
            emitln("    store i16 %pair2." + id + ", ptr %dst." + id);
            emitln("    %newlen2." + id + " = add i64 %len." + id + ", 2");
            emitln("    store i64 %newlen2." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // >= 100: check if < 1000
            emitln("  pi64_ge100." + id + ":");
            emitln("    %lt1000." + id + " = icmp ult i32 %n32." + id + ", 1000");
            emitln("    br i1 %lt1000." + id + ", label %pi64_3d." + id + ", label %pi64_4d." + id);

            // 3 digits: 100 <= n < 1000
            emitln("  pi64_3d." + id + ":");
            emitln("    %q3." + id + " = udiv i32 %n32." + id + ", 100"); // First digit
            emitln("    %r3." + id + " = urem i32 %n32." + id + ", 100"); // Last 2 digits
            // Write first digit
            emitln("    %d3_first." + id + " = add i32 %q3." + id + ", 48");
            emitln("    %d3_first_8." + id + " = trunc i32 %d3_first." + id + " to i8");
            emitln("    store i8 %d3_first_8." + id + ", ptr %dst." + id);
            // Write last 2 digits from lookup
            emitln("    %idx3." + id + " = mul i32 %r3." + id + ", 2");
            emitln("    %idx3_64." + id + " = zext i32 %idx3." + id + " to i64");
            emitln("    %pair3_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx3_64." + id);
            emitln("    %pair3." + id + " = load i16, ptr %pair3_ptr." + id);
            emitln("    %dst3_1." + id + " = getelementptr i8, ptr %dst." + id + ", i64 1");
            emitln("    store i16 %pair3." + id + ", ptr %dst3_1." + id);
            emitln("    %newlen3." + id + " = add i64 %len." + id + ", 3");
            emitln("    store i64 %newlen3." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // 4 digits: 1000 <= n < 10000
            emitln("  pi64_4d." + id + ":");
            emitln("    %q4." + id + " = udiv i32 %n32." + id + ", 100"); // First 2 digits
            emitln("    %r4." + id + " = urem i32 %n32." + id + ", 100"); // Last 2 digits
            // Write first 2 digits from lookup
            emitln("    %idx4a." + id + " = mul i32 %q4." + id + ", 2");
            emitln("    %idx4a_64." + id + " = zext i32 %idx4a." + id + " to i64");
            emitln("    %pair4a_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx4a_64." + id);
            emitln("    %pair4a." + id + " = load i16, ptr %pair4a_ptr." + id);
            emitln("    store i16 %pair4a." + id + ", ptr %dst." + id);
            // Write last 2 digits from lookup
            emitln("    %idx4b." + id + " = mul i32 %r4." + id + ", 2");
            emitln("    %idx4b_64." + id + " = zext i32 %idx4b." + id + " to i64");
            emitln("    %pair4b_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx4b_64." + id);
            emitln("    %pair4b." + id + " = load i16, ptr %pair4b_ptr." + id);
            emitln("    %dst4_2." + id + " = getelementptr i8, ptr %dst." + id + ", i64 2");
            emitln("    store i16 %pair4b." + id + ", ptr %dst4_2." + id);
            emitln("    %newlen4." + id + " = add i64 %len." + id + ", 4");
            emitln("    store i64 %newlen4." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // FFI path: call unsafe version for large/negative values
            emitln("  pi64_ffi." + id + ":");
            // Check if we have capacity for FFI path (need 20 bytes max)
            emitln("    %needed_ffi." + id + " = add i64 %len." + id + ", 20");
            emitln("    %has_space_ffi." + id + " = icmp ule i64 %needed_ffi." + id + ", %cap." +
                   id);
            emitln("    br i1 %has_space_ffi." + id + ", label %pi64_ffi_fast." + id +
                   ", label %pi64_slow." + id);

            emitln("  pi64_ffi_fast." + id + ":");
            emitln("    %written." + id + " = call i64 @tml_text_push_i64_unsafe(ptr " + receiver +
                   ", i64 " + int_val + ")");
            emitln("    br label %pi64_done." + id);

            // Slow path: call regular FFI (handles reallocation)
            emitln("  pi64_slow." + id + ":");
            emitln("    call void @tml_text_push_i64(ptr " + receiver + ", i64 " + int_val + ")");
            emitln("    br label %pi64_done." + id);

            emitln("  pi64_done." + id + ":");
            return;
        }

        if (i.method_name == "push_formatted" && i.args.size() == 3) {
            // Inline Text::push_formatted(prefix, value, suffix) with fast path
            // Pattern: prefix_str + int + suffix_str
            std::string id = std::to_string(temp_counter_++);
            std::string prefix = get_value_reg(i.args[0]);
            std::string int_val = get_value_reg(i.args[1]);
            std::string suffix = get_value_reg(i.args[2]);

            // Check for constant strings (compile-time length - no FFI!)
            auto prefix_const = value_string_contents_.find(i.args[0].id);
            auto suffix_const = value_string_contents_.find(i.args[2].id);

            // Check if all strings are constants for optimized capacity check
            bool prefix_is_const = prefix_const != value_string_contents_.end();
            bool suffix_is_const = suffix_const != value_string_contents_.end();
            bool all_const = prefix_is_const && suffix_is_const;
            size_t total_const_len = 0;
            if (all_const) {
                total_const_len = prefix_const->second.size() + suffix_const->second.size() + 20;
            }

            if (prefix_is_const) {
                size_t len = prefix_const->second.size();
                emitln("    %prefix_len." + id + " = add i64 0, " + std::to_string(len));
            } else {
                emitln("    %prefix_len_i32." + id + " = call i32 @str_len(ptr " + prefix + ")");
                emitln("    %prefix_len." + id + " = zext i32 %prefix_len_i32." + id + " to i64");
            }

            if (suffix_is_const) {
                size_t len = suffix_const->second.size();
                emitln("    %suffix_len." + id + " = add i64 0, " + std::to_string(len));
            } else {
                emitln("    %suffix_len_i32." + id + " = call i32 @str_len(ptr " + suffix + ")");
                emitln("    %suffix_len." + id + " = zext i32 %suffix_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %pfmt_heap." + id + ", label %pfmt_slow." +
                   id);

            // Heap path: check capacity for prefix + 20 (max int) + suffix
            emitln("  pfmt_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Calculate needed space: len + prefix_len + 20 + suffix_len
            // OPTIMIZED: if all strings are constant, use single add with precomputed total
            if (all_const) {
                emitln("    %needed." + id + " = add i64 %len." + id + ", " +
                       std::to_string(total_const_len));
            } else {
                emitln("    %need1." + id + " = add i64 %len." + id + ", %prefix_len." + id);
                emitln("    %need2." + id + " = add i64 %need1." + id + ", 20");
                emitln("    %needed." + id + " = add i64 %need2." + id + ", %suffix_len." + id);
            }
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %pfmt_fast." + id +
                   ", label %pfmt_slow." + id);

            // Fast path: memcpy prefix, inline int-to-string, memcpy suffix
            // OPTIMIZED: keep length in registers, only store once at end
            emitln("  pfmt_fast." + id + ":");
            // Copy prefix - use literal size for constant strings
            std::string prefix_size =
                prefix_is_const ? std::to_string(prefix_const->second.size()) : "%prefix_len." + id;
            emitln("    %dst1." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst1." + id + ", ptr " + prefix +
                   ", i64 " + prefix_size + ", i1 false)");
            // Update len after prefix (keep in register)
            emitln("    %len2." + id + " = add i64 %len." + id + ", " + prefix_size);
            // NO store - keep in register
            // Inline int-to-string conversion (skip_store=true)
            std::string int_id = id + ".i";
            std::string len_after_int =
                emit_inline_int_to_string(int_id, int_val, "%data_ptr." + id, "%len_ptr." + id,
                                          "%len2." + id, receiver, "", true);
            // Continue directly - use returned value
            // Copy suffix - use literal size for constant strings
            std::string suffix_size =
                suffix_is_const ? std::to_string(suffix_const->second.size()) : "%suffix_len." + id;
            emitln("    %dst2." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 " +
                   len_after_int);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst2." + id + ", ptr " + suffix +
                   ", i64 " + suffix_size + ", i1 false)");
            // Update final len and store only once
            emitln("    %len4." + id + " = add i64 " + len_after_int + ", " + suffix_size);
            emitln("    store i64 %len4." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pfmt_done." + id);

            // Slow path: call FFI - also use literal sizes when available
            emitln("  pfmt_slow." + id + ":");
            emitln("    call void @tml_text_push_formatted(ptr " + receiver + ", ptr " + prefix +
                   ", i64 " + prefix_size + ", i64 " + int_val + ", ptr " + suffix + ", i64 " +
                   suffix_size + ")");
            emitln("    br label %pfmt_done." + id);

            emitln("  pfmt_done." + id + ":");
            return;
        }

        if (i.method_name == "push_log" && i.args.size() == 7) {
            // Inline Text::push_log(s1, n1, s2, n2, s3, n3, s4) with fast path
            // Pattern: s1 + n1 + s2 + n2 + s3 + n3 + s4
            std::string id = std::to_string(temp_counter_++);
            std::string s1 = get_value_reg(i.args[0]);
            std::string n1 = get_value_reg(i.args[1]);
            std::string s2 = get_value_reg(i.args[2]);
            std::string n2 = get_value_reg(i.args[3]);
            std::string s3 = get_value_reg(i.args[4]);
            std::string n3 = get_value_reg(i.args[5]);
            std::string s4 = get_value_reg(i.args[6]);

            // Check for constant strings (compile-time length - no FFI!)
            auto s1_const = value_string_contents_.find(i.args[0].id);
            auto s2_const = value_string_contents_.find(i.args[2].id);
            auto s3_const = value_string_contents_.find(i.args[4].id);
            auto s4_const = value_string_contents_.find(i.args[6].id);

            // Check if all strings are constants for optimized capacity check
            bool all_const = (s1_const != value_string_contents_.end()) &&
                             (s2_const != value_string_contents_.end()) &&
                             (s3_const != value_string_contents_.end()) &&
                             (s4_const != value_string_contents_.end());
            size_t total_const_len = 0;
            if (all_const) {
                total_const_len = s1_const->second.size() + s2_const->second.size() +
                                  s3_const->second.size() + s4_const->second.size() + 60;
            }

            // Get lengths for all 4 strings
            if (s1_const != value_string_contents_.end()) {
                emitln("    %s1_len." + id + " = add i64 0, " +
                       std::to_string(s1_const->second.size()));
            } else {
                emitln("    %s1_len_i32." + id + " = call i32 @str_len(ptr " + s1 + ")");
                emitln("    %s1_len." + id + " = zext i32 %s1_len_i32." + id + " to i64");
            }
            if (s2_const != value_string_contents_.end()) {
                emitln("    %s2_len." + id + " = add i64 0, " +
                       std::to_string(s2_const->second.size()));
            } else {
                emitln("    %s2_len_i32." + id + " = call i32 @str_len(ptr " + s2 + ")");
                emitln("    %s2_len." + id + " = zext i32 %s2_len_i32." + id + " to i64");
            }
            if (s3_const != value_string_contents_.end()) {
                emitln("    %s3_len." + id + " = add i64 0, " +
                       std::to_string(s3_const->second.size()));
            } else {
                emitln("    %s3_len_i32." + id + " = call i32 @str_len(ptr " + s3 + ")");
                emitln("    %s3_len." + id + " = zext i32 %s3_len_i32." + id + " to i64");
            }
            if (s4_const != value_string_contents_.end()) {
                emitln("    %s4_len." + id + " = add i64 0, " +
                       std::to_string(s4_const->second.size()));
            } else {
                emitln("    %s4_len_i32." + id + " = call i32 @str_len(ptr " + s4 + ")");
                emitln("    %s4_len." + id + " = zext i32 %s4_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %plog_heap." + id + ", label %plog_slow." +
                   id);

            // Heap path: check capacity for all strings + 60 (3 ints max 20 each)
            emitln("  plog_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Calculate needed: len + s1 + s2 + s3 + s4 + 60
            // OPTIMIZED: if all strings are constant, use single add with precomputed total
            if (all_const) {
                emitln("    %needed." + id + " = add i64 %len." + id + ", " +
                       std::to_string(total_const_len));
            } else {
                emitln("    %need1." + id + " = add i64 %len." + id + ", %s1_len." + id);
                emitln("    %need2." + id + " = add i64 %need1." + id + ", %s2_len." + id);
                emitln("    %need3." + id + " = add i64 %need2." + id + ", %s3_len." + id);
                emitln("    %need4." + id + " = add i64 %need3." + id + ", %s4_len." + id);
                emitln("    %needed." + id + " = add i64 %need4." + id + ", 60");
            }
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %plog_fast." + id +
                   ", label %plog_slow." + id);

            // Fast path: inline all memcpy and int-to-string
            // OPTIMIZED: keep length in registers, only store once at end
            // OPTIMIZED: use literal constants for memcpy size when strings are constant
            emitln("  plog_fast." + id + ":");

            // Helper to get memcpy size (literal for const, register for dynamic)
            auto get_memcpy_size = [&](auto const_iter, const std::string& len_reg,
                                       bool is_const) -> std::string {
                if (is_const) {
                    return std::to_string(const_iter->second.size());
                }
                return len_reg;
            };

            // s1
            emitln("    %dst1." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            std::string s1_size = get_memcpy_size(s1_const, "%s1_len." + id,
                                                  s1_const != value_string_contents_.end());
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst1." + id + ", ptr " + s1 +
                   ", i64 " + s1_size + ", i1 false)");
            emitln("    %len1." + id + " = add i64 %len." + id + ", %s1_len." + id);
            // NO store - keep in register

            // n1 (inline int-to-string, skip_store=true)
            std::string len_after_n1 =
                emit_inline_int_to_string(id + ".n1", n1, "%data_ptr." + id, "%len_ptr." + id,
                                          "%len1." + id, receiver, "", true);

            // s2
            emitln("    %dst2." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 " +
                   len_after_n1);
            std::string s2_size = get_memcpy_size(s2_const, "%s2_len." + id,
                                                  s2_const != value_string_contents_.end());
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst2." + id + ", ptr " + s2 +
                   ", i64 " + s2_size + ", i1 false)");
            emitln("    %len3." + id + " = add i64 " + len_after_n1 + ", %s2_len." + id);
            // NO store - keep in register

            // n2 (inline int-to-string, skip_store=true)
            std::string len_after_n2 =
                emit_inline_int_to_string(id + ".n2", n2, "%data_ptr." + id, "%len_ptr." + id,
                                          "%len3." + id, receiver, "", true);

            // s3
            emitln("    %dst3." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 " +
                   len_after_n2);
            std::string s3_size = get_memcpy_size(s3_const, "%s3_len." + id,
                                                  s3_const != value_string_contents_.end());
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst3." + id + ", ptr " + s3 +
                   ", i64 " + s3_size + ", i1 false)");
            emitln("    %len5." + id + " = add i64 " + len_after_n2 + ", %s3_len." + id);
            // NO store - keep in register

            // n3 (inline int-to-string, skip_store=true)
            std::string len_after_n3 =
                emit_inline_int_to_string(id + ".n3", n3, "%data_ptr." + id, "%len_ptr." + id,
                                          "%len5." + id, receiver, "", true);

            // s4
            emitln("    %dst4." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 " +
                   len_after_n3);
            std::string s4_size = get_memcpy_size(s4_const, "%s4_len." + id,
                                                  s4_const != value_string_contents_.end());
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst4." + id + ", ptr " + s4 +
                   ", i64 " + s4_size + ", i1 false)");
            emitln("    %len7." + id + " = add i64 " + len_after_n3 + ", %s4_len." + id);
            // ONLY store final length once
            emitln("    store i64 %len7." + id + ", ptr %len_ptr." + id);

            emitln("    br label %plog_done." + id);

            // Slow path: call FFI
            emitln("  plog_slow." + id + ":");
            emitln("    call void @tml_text_push_log(ptr " + receiver + ", ptr " + s1 +
                   ", i64 %s1_len." + id + ", i64 " + n1 + ", ptr " + s2 + ", i64 %s2_len." + id +
                   ", i64 " + n2 + ", ptr " + s3 + ", i64 %s3_len." + id + ", i64 " + n3 +
                   ", ptr " + s4 + ", i64 %s4_len." + id + ")");
            emitln("    br label %plog_done." + id);

            emitln("  plog_done." + id + ":");
            return;
        }

        if (i.method_name == "push_path" && i.args.size() == 5) {
            // Inline Text::push_path(s1, n1, s2, n2, s3) with fast path
            // Pattern: s1 + n1 + s2 + n2 + s3
            std::string id = std::to_string(temp_counter_++);
            std::string s1 = get_value_reg(i.args[0]);
            std::string n1 = get_value_reg(i.args[1]);
            std::string s2 = get_value_reg(i.args[2]);
            std::string n2 = get_value_reg(i.args[3]);
            std::string s3 = get_value_reg(i.args[4]);

            // Check for constant strings (compile-time length - no FFI!)
            auto s1_const = value_string_contents_.find(i.args[0].id);
            auto s2_const = value_string_contents_.find(i.args[2].id);
            auto s3_const = value_string_contents_.find(i.args[4].id);

            // Check if all strings are constants for optimized capacity check
            bool all_const = (s1_const != value_string_contents_.end()) &&
                             (s2_const != value_string_contents_.end()) &&
                             (s3_const != value_string_contents_.end());
            size_t total_const_len = 0;
            if (all_const) {
                total_const_len = s1_const->second.size() + s2_const->second.size() +
                                  s3_const->second.size() + 40;
            }

            // Get lengths for all 3 strings
            if (s1_const != value_string_contents_.end()) {
                emitln("    %s1_len." + id + " = add i64 0, " +
                       std::to_string(s1_const->second.size()));
            } else {
                emitln("    %s1_len_i32." + id + " = call i32 @str_len(ptr " + s1 + ")");
                emitln("    %s1_len." + id + " = zext i32 %s1_len_i32." + id + " to i64");
            }
            if (s2_const != value_string_contents_.end()) {
                emitln("    %s2_len." + id + " = add i64 0, " +
                       std::to_string(s2_const->second.size()));
            } else {
                emitln("    %s2_len_i32." + id + " = call i32 @str_len(ptr " + s2 + ")");
                emitln("    %s2_len." + id + " = zext i32 %s2_len_i32." + id + " to i64");
            }
            if (s3_const != value_string_contents_.end()) {
                emitln("    %s3_len." + id + " = add i64 0, " +
                       std::to_string(s3_const->second.size()));
            } else {
                emitln("    %s3_len_i32." + id + " = call i32 @str_len(ptr " + s3 + ")");
                emitln("    %s3_len." + id + " = zext i32 %s3_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %ppath_heap." + id +
                   ", label %ppath_slow." + id);

            // Heap path: check capacity for all strings + 40 (2 ints max 20 each)
            emitln("  ppath_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Calculate needed: len + s1 + s2 + s3 + 40
            // OPTIMIZED: if all strings are constant, use single add with precomputed total
            if (all_const) {
                emitln("    %needed." + id + " = add i64 %len." + id + ", " +
                       std::to_string(total_const_len));
            } else {
                emitln("    %need1." + id + " = add i64 %len." + id + ", %s1_len." + id);
                emitln("    %need2." + id + " = add i64 %need1." + id + ", %s2_len." + id);
                emitln("    %need3." + id + " = add i64 %need2." + id + ", %s3_len." + id);
                emitln("    %needed." + id + " = add i64 %need3." + id + ", 40");
            }
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %ppath_fast." + id +
                   ", label %ppath_slow." + id);

            // Fast path: inline all memcpy and int-to-string
            // OPTIMIZED: keep length in registers, only store once at end
            // OPTIMIZED: use literal constants for memcpy size when strings are constant
            emitln("  ppath_fast." + id + ":");

            // Helper to get memcpy size (literal for const, register for dynamic)
            auto get_memcpy_size = [&](auto const_iter, const std::string& len_reg,
                                       bool is_const) -> std::string {
                if (is_const) {
                    return std::to_string(const_iter->second.size());
                }
                return len_reg;
            };

            // s1
            emitln("    %dst1." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            std::string s1_size = get_memcpy_size(s1_const, "%s1_len." + id,
                                                  s1_const != value_string_contents_.end());
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst1." + id + ", ptr " + s1 +
                   ", i64 " + s1_size + ", i1 false)");
            emitln("    %len1." + id + " = add i64 %len." + id + ", %s1_len." + id);
            // NO store - keep in register

            // n1 (inline int-to-string, skip_store=true)
            std::string len_after_n1 =
                emit_inline_int_to_string(id + ".n1", n1, "%data_ptr." + id, "%len_ptr." + id,
                                          "%len1." + id, receiver, "", true);
            // Continue directly - no branch/label needed, use returned value

            // s2
            emitln("    %dst2." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 " +
                   len_after_n1);
            std::string s2_size = get_memcpy_size(s2_const, "%s2_len." + id,
                                                  s2_const != value_string_contents_.end());
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst2." + id + ", ptr " + s2 +
                   ", i64 " + s2_size + ", i1 false)");
            emitln("    %len3." + id + " = add i64 " + len_after_n1 + ", %s2_len." + id);
            // NO store - keep in register

            // n2 (inline int-to-string, skip_store=true)
            std::string len_after_n2 =
                emit_inline_int_to_string(id + ".n2", n2, "%data_ptr." + id, "%len_ptr." + id,
                                          "%len3." + id, receiver, "", true);
            // Continue directly - no branch/label needed, use returned value

            // s3
            emitln("    %dst3." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 " +
                   len_after_n2);
            std::string s3_size = get_memcpy_size(s3_const, "%s3_len." + id,
                                                  s3_const != value_string_contents_.end());
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst3." + id + ", ptr " + s3 +
                   ", i64 " + s3_size + ", i1 false)");
            emitln("    %len5." + id + " = add i64 " + len_after_n2 + ", %s3_len." + id);
            // ONLY store final length once
            emitln("    store i64 %len5." + id + ", ptr %len_ptr." + id);

            emitln("    br label %ppath_done." + id);

            // Slow path: call FFI
            emitln("  ppath_slow." + id + ":");
            emitln("    call void @tml_text_push_path(ptr " + receiver + ", ptr " + s1 +
                   ", i64 %s1_len." + id + ", i64 " + n1 + ", ptr " + s2 + ", i64 %s2_len." + id +
                   ", i64 " + n2 + ", ptr " + s3 + ", i64 %s3_len." + id + ")");
            emitln("    br label %ppath_done." + id);

            emitln("  ppath_done." + id + ":");
            return;
        }
    }

    // Normal method call path (non-inlined)
    mir::MirTypePtr ret_ptr = i.return_type;
    if (!ret_ptr && inst.result != mir::INVALID_VALUE) {
        ret_ptr = mir::make_ptr_type();
    } else if (!ret_ptr) {
        ret_ptr = mir::make_unit_type();
    }
    if (i.method_name == "to_string" && !result_reg.empty()) {
        ret_ptr = mir::make_ptr_type();
    }
    std::string ret_type = mir_type_to_llvm(ret_ptr);

    // Determine the actual LLVM type of the receiver value
    // Priority: value_types_ (what the register actually holds) > i.receiver.type (MIR type)
    std::string receiver_actual_type;
    auto vt_it = value_types_.find(i.receiver.id);
    if (vt_it != value_types_.end() && !vt_it->second.empty() && vt_it->second != "ptr") {
        // Use the actual type from value_types_ (what the register holds)
        receiver_actual_type = vt_it->second;
    } else if (i.receiver.type) {
        receiver_actual_type = mir_type_to_llvm(i.receiver.type);
    }
    // Final fallback
    if (receiver_actual_type.empty()) {
        receiver_actual_type = "ptr";
    }

    static const std::unordered_set<std::string> primitive_tml_types = {
        "I8",  "I16", "I32",  "I64", "I128", "U8",   "U16",
        "U32", "U64", "U128", "F32", "F64",  "Bool", "Char"};
    bool is_primitive_tml = primitive_tml_types.count(recv_type) > 0;
    bool is_struct_type = receiver_actual_type.find("%struct.") == 0;

    std::string receiver_type_for_call = receiver_actual_type;

    if (is_primitive_tml) {
        receiver_type_for_call = receiver_actual_type;
    } else if (is_struct_type) {
        // Struct value needs to be spilled to memory - methods expect a pointer
        std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
        emitln("    " + spill_ptr + " = alloca " + receiver_actual_type);
        emitln("    store " + receiver_actual_type + " " + receiver + ", ptr " + spill_ptr);
        receiver = spill_ptr;
        receiver_type_for_call = "ptr";
    } else if (receiver_actual_type == "ptr" || receiver_actual_type.empty()) {
        receiver_type_for_call = "ptr";
    } else {
        std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
        emitln("    " + spill_ptr + " = alloca " + receiver_actual_type);
        emitln("    store " + receiver_actual_type + " " + receiver + ", ptr " + spill_ptr);
        receiver = spill_ptr;
        receiver_type_for_call = "ptr";
    }

    if (ret_type != "void" && !result_reg.empty()) {
        emit("    " + result_reg + " = ");
    } else {
        emit("    ");
    }

    std::string func_name;
    if (is_primitive_tml) {
        std::string lower_type = recv_type;
        for (auto& c : lower_type)
            c = static_cast<char>(std::tolower(c));
        func_name = lower_type + "_" + i.method_name;
    } else {
        // Non-primitive methods use the tml_ prefix and single underscore
        // Example: RangeIterI64.next() -> tml_RangeIterI64_next
        func_name = "tml_" + recv_type + "_" + i.method_name;
    }

    emit("call " + ret_type + " @" + func_name + "(");
    emit(receiver_type_for_call + " " + receiver);
    for (size_t j = 0; j < i.args.size(); ++j) {
        emit(", ");
        mir::MirTypePtr arg_ptr =
            (j < i.arg_types.size() && i.arg_types[j]) ? i.arg_types[j] : i.args[j].type;
        if (!arg_ptr) {
            arg_ptr = mir::make_i32_type();
        }
        std::string arg_type = mir_type_to_llvm(arg_ptr);
        std::string arg = get_value_reg(i.args[j]);
        emit(arg_type + " " + arg);
    }
    emitln(")");

    if (inst.result != mir::INVALID_VALUE && ret_type != "void") {
        value_types_[inst.result] = ret_type;
    }
}

} // namespace tml::codegen
