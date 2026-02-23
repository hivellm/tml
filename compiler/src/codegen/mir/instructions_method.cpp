TML_MODULE("codegen_x86")

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

    // Text type: now pure TML — all methods compiled from text.tml through normal codegen
    // (V8-style inline optimizations removed — SSO layout no longer applies)

    // ========================================================================
    // Char to_string / debug_string — inline emission (Phase 47)
    // Char is represented as i32 but needs char-to-string conversion.
    // Alloc 2 bytes, write char byte + null terminator.
    // ========================================================================
    if (recv_type == "Char" && (i.method_name == "to_string" || i.method_name == "debug_string")) {
        std::string id = std::to_string(temp_counter_++);
        // Truncate i32 to i8 (ASCII)
        emitln("    %char_byte." + id + " = trunc i32 " + receiver + " to i8");
        // Allocate 2 bytes for single-char string + null
        emitln("    %char_buf." + id + " = call ptr @mem_alloc(i64 2)");
        emitln("    store i8 %char_byte." + id + ", ptr %char_buf." + id);
        emitln("    %char_p1." + id + " = getelementptr i8, ptr %char_buf." + id + ", i64 1");
        emitln("    store i8 0, ptr %char_p1." + id);

        if (i.method_name == "debug_string") {
            // Wrap in single quotes: "'" + c + "'"
            // @.str.sq is declared in emit_preamble()
            emitln("    %sq_tmp." + id +
                   " = call ptr @str_concat_opt(ptr @.str.sq, ptr %char_buf." + id + ")");
            emitln("    " + result_reg + " = call ptr @str_concat_opt(ptr %sq_tmp." + id +
                   ", ptr @.str.sq)");
        } else {
            // to_string: just return the buffer
            emitln("    " + result_reg + " = bitcast ptr %char_buf." + id + " to ptr");
        }

        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "ptr";
        }
        return;
    }

    // ========================================================================
    // Str to_string / debug_string — inline emission (Phase 47)
    // Str::to_string is identity, Str::debug_string wraps in quotes.
    // ========================================================================
    if (recv_type == "Str" && (i.method_name == "to_string" || i.method_name == "debug_string")) {
        if (i.method_name == "to_string") {
            // Identity — string is already a string
            emitln("    " + result_reg + " = bitcast ptr " + receiver + " to ptr");
        } else {
            // debug_string wraps in quotes: "\"" + s + "\""
            // @.str.dq is declared in emit_preamble()
            std::string id = std::to_string(temp_counter_++);
            emitln("    %dq_tmp." + id + " = call ptr @str_concat_opt(ptr @.str.dq, ptr " +
                   receiver + ")");
            emitln("    " + result_reg + " = call ptr @str_concat_opt(ptr %dq_tmp." + id +
                   ", ptr @.str.dq)");
        }

        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "ptr";
        }
        return;
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
