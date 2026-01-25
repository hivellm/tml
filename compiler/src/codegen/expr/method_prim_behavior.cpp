//! # LLVM IR Generator - Primitive Type Behavior Methods
//!
//! This file handles behavior implementations on primitive types
//! like partial_cmp, cmp, debug_string on I32, F64, etc.
//! Extracted from method.cpp for maintainability.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_primitive_behavior_method(
    const parser::MethodCallExpr& call, const std::string& receiver, types::TypePtr receiver_type,
    const std::string& receiver_type_name, bool receiver_was_ref) -> std::optional<std::string> {
    const std::string& method = call.method;

    if (!(receiver_type && receiver_type->is<types::PrimitiveType>() &&
          !receiver_type_name.empty())) {
        return std::nullopt;
    }

    const auto& prim = receiver_type->as<types::PrimitiveType>();
    std::string llvm_ty = llvm_type_from_semantic(receiver_type);

    // Handle partial_cmp inline for numeric types - returns Maybe[Ordering]
    // For numeric types, partial_cmp always returns Just(cmp(self, other))
    if (method == "partial_cmp") {
        bool is_signed =
            (prim.kind == types::PrimitiveKind::I8 || prim.kind == types::PrimitiveKind::I16 ||
             prim.kind == types::PrimitiveKind::I32 || prim.kind == types::PrimitiveKind::I64 ||
             prim.kind == types::PrimitiveKind::I128);
        bool is_unsigned =
            (prim.kind == types::PrimitiveKind::U8 || prim.kind == types::PrimitiveKind::U16 ||
             prim.kind == types::PrimitiveKind::U32 || prim.kind == types::PrimitiveKind::U64 ||
             prim.kind == types::PrimitiveKind::U128);
        bool is_float =
            (prim.kind == types::PrimitiveKind::F32 || prim.kind == types::PrimitiveKind::F64);

        if ((is_signed || is_unsigned || is_float) && call.args.size() == 1) {
            // Ensure Maybe[Ordering] struct type is defined
            // Create an Ordering semantic type for the type argument
            auto ordering_type = std::make_shared<types::Type>();
            ordering_type->kind = types::NamedType{"Ordering", "", {}};
            std::vector<types::TypePtr> maybe_type_args = {ordering_type};
            std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
            std::string maybe_type = "%struct." + maybe_mangled;

            // Load other value from ref
            std::string other_ref = gen_expr(*call.args[0]);
            std::string other = fresh_reg();
            emit_line("  " + other + " = load " + llvm_ty + ", ptr " + other_ref);

            // If receiver was originally a reference, we need to load through the pointer
            std::string receiver_val = receiver;
            if (receiver_was_ref) {
                receiver_val = fresh_reg();
                emit_line("  " + receiver_val + " = load " + llvm_ty + ", ptr " + receiver);
            }

            // Compare and determine ordering
            std::string cmp_lt = fresh_reg();
            std::string cmp_gt = fresh_reg();
            if (is_float) {
                emit_line("  " + cmp_lt + " = fcmp olt " + llvm_ty + " " + receiver_val + ", " +
                          other);
                emit_line("  " + cmp_gt + " = fcmp ogt " + llvm_ty + " " + receiver_val + ", " +
                          other);
            } else if (is_signed) {
                emit_line("  " + cmp_lt + " = icmp slt " + llvm_ty + " " + receiver_val + ", " +
                          other);
                emit_line("  " + cmp_gt + " = icmp sgt " + llvm_ty + " " + receiver_val + ", " +
                          other);
            } else {
                emit_line("  " + cmp_lt + " = icmp ult " + llvm_ty + " " + receiver_val + ", " +
                          other);
                emit_line("  " + cmp_gt + " = icmp ugt " + llvm_ty + " " + receiver_val + ", " +
                          other);
            }

            // Build Ordering value: Less=0, Equal=1, Greater=2
            std::string tag_1 = fresh_reg();
            std::string tag_2 = fresh_reg();
            emit_line("  " + tag_1 + " = select i1 " + cmp_lt + ", i32 0, i32 1");
            emit_line("  " + tag_2 + " = select i1 " + cmp_gt + ", i32 2, i32 " + tag_1);

            // Build Ordering struct on stack
            std::string ordering_alloca = fresh_reg();
            emit_line("  " + ordering_alloca + " = alloca %struct.Ordering, align 4");
            std::string ordering_tag_ptr = fresh_reg();
            emit_line("  " + ordering_tag_ptr + " = getelementptr inbounds %struct.Ordering, ptr " +
                      ordering_alloca + ", i32 0, i32 0");
            emit_line("  store i32 " + tag_2 + ", ptr " + ordering_tag_ptr);

            // Load Ordering value
            std::string ordering = fresh_reg();
            emit_line("  " + ordering + " = load %struct.Ordering, ptr " + ordering_alloca);

            // Build Maybe[Ordering] = Just(ordering) using alloca/store pattern
            // Tag 0 = Just, Tag 1 = Nothing
            std::string enum_alloca = fresh_reg();
            emit_line("  " + enum_alloca + " = alloca " + maybe_type + ", align 8");

            // Set tag (field 0) to 0 (Just)
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      enum_alloca + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr);

            // Set payload (field 1) - store Ordering into payload area
            std::string payload_ptr = fresh_reg();
            emit_line("  " + payload_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      enum_alloca + ", i32 0, i32 1");
            emit_line("  store %struct.Ordering " + ordering + ", ptr " + payload_ptr);

            // Load the complete Maybe[Ordering] value
            std::string maybe_final = fresh_reg();
            emit_line("  " + maybe_final + " = load " + maybe_type + ", ptr " + enum_alloca);

            last_expr_type_ = maybe_type;
            return maybe_final;
        }
    }

    // Handle cmp inline for numeric types - returns Ordering directly
    if (method == "cmp") {
        bool is_signed =
            (prim.kind == types::PrimitiveKind::I8 || prim.kind == types::PrimitiveKind::I16 ||
             prim.kind == types::PrimitiveKind::I32 || prim.kind == types::PrimitiveKind::I64 ||
             prim.kind == types::PrimitiveKind::I128);
        bool is_unsigned =
            (prim.kind == types::PrimitiveKind::U8 || prim.kind == types::PrimitiveKind::U16 ||
             prim.kind == types::PrimitiveKind::U32 || prim.kind == types::PrimitiveKind::U64 ||
             prim.kind == types::PrimitiveKind::U128);

        if ((is_signed || is_unsigned) && call.args.size() == 1) {
            // Load other value from ref
            std::string other_ref = gen_expr(*call.args[0]);
            std::string other = fresh_reg();
            emit_line("  " + other + " = load " + llvm_ty + ", ptr " + other_ref);

            // If receiver was originally a reference, we need to load through the pointer
            std::string receiver_val = receiver;
            if (receiver_was_ref) {
                receiver_val = fresh_reg();
                emit_line("  " + receiver_val + " = load " + llvm_ty + ", ptr " + receiver);
            }

            // Compare and determine ordering
            std::string cmp_lt = fresh_reg();
            std::string cmp_gt = fresh_reg();
            if (is_signed) {
                emit_line("  " + cmp_lt + " = icmp slt " + llvm_ty + " " + receiver_val + ", " +
                          other);
                emit_line("  " + cmp_gt + " = icmp sgt " + llvm_ty + " " + receiver_val + ", " +
                          other);
            } else {
                emit_line("  " + cmp_lt + " = icmp ult " + llvm_ty + " " + receiver_val + ", " +
                          other);
                emit_line("  " + cmp_gt + " = icmp ugt " + llvm_ty + " " + receiver_val + ", " +
                          other);
            }

            // Build Ordering value: Less=0, Equal=1, Greater=2
            std::string tag_1 = fresh_reg();
            std::string tag_2 = fresh_reg();
            emit_line("  " + tag_1 + " = select i1 " + cmp_lt + ", i32 0, i32 1");
            emit_line("  " + tag_2 + " = select i1 " + cmp_gt + ", i32 2, i32 " + tag_1);

            // Build Ordering struct
            std::string ordering = fresh_reg();
            emit_line("  " + ordering + " = insertvalue %struct.Ordering undef, i32 " + tag_2 +
                      ", 0");

            last_expr_type_ = "%struct.Ordering";
            return ordering;
        }
    }

    // Look for impl methods on primitive types (e.g., impl PartialOrd for I64)
    std::string qualified_name = receiver_type_name + "::" + method;
    types::FuncSig func_sig_value;
    types::FuncSig* func_sig = nullptr;
    bool is_from_library = false;

    // Search module registry for the method
    if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto func_it = mod.functions.find(qualified_name);
            if (func_it != mod.functions.end()) {
                func_sig_value = func_it->second;
                func_sig = &func_sig_value;
                is_from_library = true;
                break;
            }
        }
    }

    // Also try local lookup
    if (!func_sig) {
        auto local_sig = env_.lookup_func(qualified_name);
        if (local_sig) {
            func_sig_value = *local_sig;
            func_sig = &func_sig_value;
        }
    }

    if (func_sig) {
        // Look up in functions_ to get the correct LLVM name
        std::string method_lookup_key = receiver_type_name + "_" + method;
        auto method_it = functions_.find(method_lookup_key);
        std::string fn_name;
        if (method_it != functions_.end()) {
            fn_name = method_it->second.llvm_name;
        } else {
            // Only use suite prefix for test-local functions, not library methods
            std::string prefix = is_from_library ? "" : get_suite_prefix();
            fn_name = "@tml_" + prefix + receiver_type_name + "_" + method;
        }
        std::string recv_llvm_ty = llvm_type_from_semantic(receiver_type);

        // Build arguments - this (by value for primitives), then args
        std::vector<std::pair<std::string, std::string>> typed_args;
        typed_args.push_back({recv_llvm_ty, receiver});

        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            std::string arg_type = "i32";
            if (i + 1 < func_sig->params.size()) {
                arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
            }
            typed_args.push_back({arg_type, val});
        }

        // Use registered function's return type if available (handles value class by-value
        // returns)
        std::string ret_type = llvm_type_from_semantic(func_sig->return_type);
        if (method_it != functions_.end() && !method_it->second.ret_type.empty()) {
            ret_type = method_it->second.ret_type;
        }

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
