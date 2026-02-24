TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Compiler Intrinsics (Extended)
//!
//! This file implements the second half of compiler intrinsics that map directly
//! to LLVM instructions. Split from intrinsics.cpp for maintainability.
//!
//! ## Sections in this file
//!
//! - Compiler Hints (unreachable, assume, likely, unlikely, fence, drop)
//! - Checked Arithmetic (checked_add, checked_sub, checked_mul, checked_div)
//! - Saturating Arithmetic (saturating_add, saturating_sub, saturating_mul)
//! - Bit Manipulation (ctlz, cttz, ctpop, bswap, bitreverse)
//! - Math Intrinsics (sqrt, sin, cos, log, exp, pow, floor, ceil, round, trunc, fma, etc.)
//! - Reflection Intrinsics (field_count, variant_count, field_name, field_type_id, etc.)

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <unordered_set>

namespace tml::codegen {

auto LLVMIRGen::try_gen_intrinsic_extended(const std::string& intrinsic_name,
                                           const parser::CallExpr& call, const std::string& fn_name)
    -> std::optional<std::string> {

    // Function signature lookup (used by some intrinsics for type info)
    auto func_sig = env_.lookup_func(fn_name);

    // ============================================================================
    // Compiler Hints
    // ============================================================================

    // unreachable() -> Unit
    if (intrinsic_name == "unreachable") {
        emit_line("  unreachable");
        block_terminated_ = true;
        last_expr_type_ = "void";
        return "0";
    }

    // assume(cond: Bool)
    if (intrinsic_name == "assume") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            emit_line("  call void @llvm.assume(i1 " + cond + ")");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // likely(cond: Bool) -> Bool
    if (intrinsic_name == "likely") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @llvm.expect.i1(i1 " + cond + ", i1 true)");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // unlikely(cond: Bool) -> Bool
    if (intrinsic_name == "unlikely") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @llvm.expect.i1(i1 " + cond + ", i1 false)");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // fence()
    if (intrinsic_name == "fence") {
        emit_line("  fence seq_cst");
        last_expr_type_ = "void";
        return "0";
    }

    // compiler_fence() — prevents compiler reordering without hardware fence
    if (intrinsic_name == "compiler_fence") {
        // LLVM's compiler fence is just a barrier for the optimizer
        // In LLVM IR, this is expressed with "fence singlethread seq_cst"
        emit_line("  fence syncscope(\"singlethread\") seq_cst");
        last_expr_type_ = "void";
        return "0";
    }

    // drop[T](val: T) -> Unit
    // Explicitly drops a value, calling its destructor if it has one.
    // This intrinsic correctly handles generic type substitution.
    if (intrinsic_name == "drop") {
        if (!call.args.empty()) {
            // CRITICAL: Mark the variable as consumed to prevent double-drop
            // When a variable is explicitly dropped, the automatic drop at scope exit
            // should NOT drop it again. This fixes hangs caused by unlocking mutexes twice.
            if (call.args[0]->is<parser::IdentExpr>()) {
                const auto& ident = call.args[0]->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
            }

            // Infer the type of the argument, applying current type substitutions
            types::TypePtr arg_semantic_type = infer_expr_type(*call.args[0]);

            // Apply current type substitutions to resolve generic types
            if (arg_semantic_type && arg_semantic_type->is<types::NamedType>()) {
                const auto& named = arg_semantic_type->as<types::NamedType>();
                // Check if this is a generic type parameter that needs substitution
                auto sub_it = current_type_subs_.find(named.name);
                if (sub_it != current_type_subs_.end()) {
                    arg_semantic_type = sub_it->second;
                }
            }

            std::string arg_val = gen_expr(*call.args[0]);
            std::string arg_type = last_expr_type_;

            // For primitive types (i8, i16, i32, i64, float, double, ptr, i1),
            // drop is a no-op
            if (arg_type == "i8" || arg_type == "i16" || arg_type == "i32" || arg_type == "i64" ||
                arg_type == "i128" || arg_type == "float" || arg_type == "double" ||
                arg_type == "ptr" || arg_type == "i1" || arg_type == "void") {
                last_expr_type_ = "void";
                return "0";
            }

            // For struct types, check if a drop function exists before calling
            if (arg_type.starts_with("%struct.") || arg_type.starts_with("%class.")) {
                // Extract the type name (e.g., "Counter" from "%struct.Counter")
                std::string type_name;
                if (arg_type.starts_with("%struct.")) {
                    type_name = arg_type.substr(8); // Skip "%struct."
                } else {
                    type_name = arg_type.substr(7); // Skip "%class."
                }

                // Check if this type has a Drop implementation
                // Look for drop method in impl blocks or generated functions
                std::string drop_fn_name = "tml_" + type_name + "_drop";
                bool has_drop = generated_functions_.count("@" + drop_fn_name) > 0 ||
                                generated_impl_methods_.count(drop_fn_name) > 0;

                // Also check in the environment for types that have Drop trait
                if (!has_drop && arg_semantic_type) {
                    // Check if there's a drop impl for this type in pending_generic_impls_
                    // or if it's a known type with automatic drop (like Arc, Box, etc.)
                    if (arg_semantic_type->is<types::NamedType>()) {
                        const auto& named = arg_semantic_type->as<types::NamedType>();
                        // Types that have automatic drop implementations
                        static const std::unordered_set<std::string> auto_drop_types = {
                            "Arc",
                            "Rc",
                            "Box",
                            "Heap",
                            "Shared",
                            "Sync",
                            "Mutex",
                            "RwLock",
                            "MutexGuard",
                            "RwLockReadGuard",
                            "RwLockWriteGuard",
                            "Text",
                            "List",
                            "HashMap",
                            "Buffer",
                            "LockFreeQueue",
                            "LockFreeStack"};
                        // Check for base type (without generic args in mangled name)
                        std::string base_type = named.name;
                        auto pos = type_name.find("__");
                        if (pos != std::string::npos) {
                            base_type = type_name.substr(0, pos);
                        }
                        has_drop = auto_drop_types.count(base_type) > 0;
                    }
                }

                if (has_drop) {
                    // Generate the drop function name
                    std::string drop_fn = "@tml_" + type_name + "_drop";

                    // Store the value to get a pointer (drop functions take pointers)
                    std::string temp_alloca = fresh_reg();
                    emit_line("  " + temp_alloca + " = alloca " + arg_type);
                    emit_line("  store " + arg_type + " " + arg_val + ", ptr " + temp_alloca);

                    // Call the drop function with the pointer
                    emit_line("  call void " + drop_fn + "(ptr " + temp_alloca + ")");
                }
                // If no drop impl, it's a no-op

                last_expr_type_ = "void";
                return "0";
            }

            // Fallback: just ignore the drop for unknown types
            last_expr_type_ = "void";
            return "0";
        }
        last_expr_type_ = "void";
        return "0";
    }

    // ============================================================================
    // Checked Arithmetic Intrinsics
    // ============================================================================

    // checked_add[T](a: T, b: T) -> Maybe[T]
    // checked_sub[T](a: T, b: T) -> Maybe[T]
    // checked_mul[T](a: T, b: T) -> Maybe[T]
    if (intrinsic_name == "checked_add" || intrinsic_name == "checked_sub" ||
        intrinsic_name == "checked_mul") {
        if (call.args.size() >= 2) {
            // Resolve the target type from the generic type argument [T] on the call,
            // not from argument evaluation (which may produce wrong types for large literals)
            std::string target_type = "i32"; // default
            types::TypePtr type_arg = types::make_i32();

            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.generics && !path_expr.generics->args.empty()) {
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        type_arg =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        target_type = llvm_type_from_semantic(type_arg);
                    }
                }
            }

            // Generate args and truncate/extend to target type if needed
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            if (a_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + a_type + " " + a + " to " + target_type);
                a = cast;
            }

            std::string b = gen_expr(*call.args[1]);
            std::string b_type = last_expr_type_;
            if (b_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + b_type + " " + b + " to " + target_type);
                b = cast;
            }

            // Map to LLVM overflow intrinsic operation
            std::string op;
            if (intrinsic_name == "checked_add")
                op = "sadd";
            else if (intrinsic_name == "checked_sub")
                op = "ssub";
            else
                op = "smul";

            // Ensure Maybe[T] enum type is instantiated
            std::vector<types::TypePtr> maybe_type_args = {type_arg};
            std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
            std::string maybe_type = "%struct." + maybe_mangled;

            // Call LLVM overflow intrinsic: returns { T, i1 }
            std::string overflow_type = "{ " + target_type + ", i1 }";
            std::string ov_result = fresh_reg();
            emit_line("  " + ov_result + " = call " + overflow_type + " @llvm." + op +
                      ".with.overflow." + target_type + "(" + target_type + " " + a + ", " +
                      target_type + " " + b + ")");

            // Extract value and overflow flag
            std::string value = fresh_reg();
            std::string overflow = fresh_reg();
            emit_line("  " + value + " = extractvalue " + overflow_type + " " + ov_result + ", 0");
            emit_line("  " + overflow + " = extractvalue " + overflow_type + " " + ov_result +
                      ", 1");

            // Build Maybe[T] result using alloca/store pattern
            // If overflow: Nothing (tag=1), else: Just(value) (tag=0)
            std::string alloca_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + maybe_type);

            // Determine store type based on compact enum layout
            // With compact layout, Maybe[I32] is { i32, i32 } — store payload as-is
            // With [N x i64] layout, store as i64 for alignment
            std::string store_value = value;
            std::string store_type = target_type;
            auto payload_it = enum_payload_type_.find(maybe_type);
            if (payload_it != enum_payload_type_.end() && !payload_it->second.empty()) {
                // Compact layout: store in the payload's natural type
                store_type = payload_it->second;
                if (store_type != target_type) {
                    store_value = fresh_reg();
                    emit_line("  " + store_value + " = sext " + target_type + " " + value + " to " +
                              store_type);
                }
            } else {
                // Legacy [N x i64] layout: extend to i64
                store_type = "i64";
                if (target_type != "i64") {
                    store_value = fresh_reg();
                    emit_line("  " + store_value + " = sext " + target_type + " " + value +
                              " to i64");
                }
            }

            std::string label_just = "checked.just." + std::to_string(label_counter_++);
            std::string label_nothing = "checked.nothing." + std::to_string(label_counter_++);
            std::string label_end = "checked.end." + std::to_string(label_counter_++);

            emit_line("  br i1 " + overflow + ", label %" + label_nothing + ", label %" +
                      label_just);

            // Just branch: tag=0, store value
            emit_line(label_just + ":");
            std::string tag_ptr_j = fresh_reg();
            emit_line("  " + tag_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      alloca_reg + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr_j);
            std::string data_ptr_j = fresh_reg();
            emit_line("  " + data_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      alloca_reg + ", i32 0, i32 1");
            emit_line("  store " + store_type + " " + store_value + ", ptr " + data_ptr_j);
            emit_line("  br label %" + label_end);

            // Nothing branch: tag=1
            emit_line(label_nothing + ":");
            std::string tag_ptr_n = fresh_reg();
            emit_line("  " + tag_ptr_n + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      alloca_reg + ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + tag_ptr_n);
            emit_line("  br label %" + label_end);

            // End: load result
            emit_line(label_end + ":");
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + maybe_type + ", ptr " + alloca_reg);
            last_expr_type_ = maybe_type;
            return result;
        }
        return "0";
    }

    // checked_div[T](a: T, b: T) -> Maybe[T]
    // Division by zero returns Nothing, otherwise Just(a/b)
    if (intrinsic_name == "checked_div") {
        if (call.args.size() >= 2) {
            // Resolve target type from generic type argument [T]
            std::string target_type = "i32";
            types::TypePtr type_arg = types::make_i32();

            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.generics && !path_expr.generics->args.empty()) {
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        type_arg =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        target_type = llvm_type_from_semantic(type_arg);
                    }
                }
            }

            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            if (a_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + a_type + " " + a + " to " + target_type);
                a = cast;
            }

            std::string b = gen_expr(*call.args[1]);
            std::string b_type = last_expr_type_;
            if (b_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + b_type + " " + b + " to " + target_type);
                b = cast;
            }

            // Ensure Maybe[T] enum type is instantiated
            std::vector<types::TypePtr> maybe_type_args = {type_arg};
            std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
            std::string maybe_type = "%struct." + maybe_mangled;

            // Check if b == 0
            std::string is_zero = fresh_reg();
            emit_line("  " + is_zero + " = icmp eq " + target_type + " " + b + ", 0");

            std::string alloca_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + maybe_type);

            std::string label_ok = "cdiv.ok." + std::to_string(label_counter_++);
            std::string label_zero = "cdiv.zero." + std::to_string(label_counter_++);
            std::string label_end = "cdiv.end." + std::to_string(label_counter_++);

            emit_line("  br i1 " + is_zero + ", label %" + label_zero + ", label %" + label_ok);

            // Ok branch: divide and return Just(result)
            emit_line(label_ok + ":");
            std::string div_result = fresh_reg();
            emit_line("  " + div_result + " = sdiv " + target_type + " " + a + ", " + b);
            // Determine store type based on compact enum layout
            std::string store_val = div_result;
            std::string div_store_type = target_type;
            auto div_payload_it = enum_payload_type_.find(maybe_type);
            if (div_payload_it != enum_payload_type_.end() && !div_payload_it->second.empty()) {
                div_store_type = div_payload_it->second;
                if (div_store_type != target_type) {
                    store_val = fresh_reg();
                    emit_line("  " + store_val + " = sext " + target_type + " " + div_result +
                              " to " + div_store_type);
                }
            } else {
                div_store_type = "i64";
                if (target_type != "i64") {
                    store_val = fresh_reg();
                    emit_line("  " + store_val + " = sext " + target_type + " " + div_result +
                              " to i64");
                }
            }
            std::string tag_ptr_ok = fresh_reg();
            emit_line("  " + tag_ptr_ok + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      alloca_reg + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr_ok);
            std::string data_ptr_ok = fresh_reg();
            emit_line("  " + data_ptr_ok + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      alloca_reg + ", i32 0, i32 1");
            emit_line("  store " + div_store_type + " " + store_val + ", ptr " + data_ptr_ok);
            emit_line("  br label %" + label_end);

            // Zero branch: return Nothing (tag=1)
            emit_line(label_zero + ":");
            std::string tag_ptr_z = fresh_reg();
            emit_line("  " + tag_ptr_z + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      alloca_reg + ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + tag_ptr_z);
            emit_line("  br label %" + label_end);

            // End: load result
            emit_line(label_end + ":");
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + maybe_type + ", ptr " + alloca_reg);
            last_expr_type_ = maybe_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Saturating Arithmetic Intrinsics
    // ============================================================================

    // saturating_add[T](a: T, b: T) -> T
    // saturating_sub[T](a: T, b: T) -> T
    if (intrinsic_name == "saturating_add" || intrinsic_name == "saturating_sub") {
        if (call.args.size() >= 2) {
            // Resolve target type from generic type argument [T]
            std::string target_type = "i32";
            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.generics && !path_expr.generics->args.empty()) {
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        auto resolved =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        target_type = llvm_type_from_semantic(resolved);
                    }
                }
            }

            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            if (a_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + a_type + " " + a + " to " + target_type);
                a = cast;
            }

            std::string b = gen_expr(*call.args[1]);
            std::string b_type = last_expr_type_;
            if (b_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + b_type + " " + b + " to " + target_type);
                b = cast;
            }

            std::string llvm_op = (intrinsic_name == "saturating_add") ? "sadd" : "ssub";
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + target_type + " @llvm." + llvm_op + ".sat." +
                      target_type + "(" + target_type + " " + a + ", " + target_type + " " + b +
                      ")");
            last_expr_type_ = target_type;
            return result;
        }
        return "0";
    }

    // saturating_mul[T](a: T, b: T) -> T
    // LLVM doesn't have a saturating multiply intrinsic, so we use overflow check + select
    if (intrinsic_name == "saturating_mul") {
        if (call.args.size() >= 2) {
            // Resolve target type from generic type argument [T]
            std::string target_type = "i32";
            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.generics && !path_expr.generics->args.empty()) {
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        auto resolved =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        target_type = llvm_type_from_semantic(resolved);
                    }
                }
            }

            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            if (a_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + a_type + " " + a + " to " + target_type);
                a = cast;
            }

            std::string b = gen_expr(*call.args[1]);
            std::string b_type = last_expr_type_;
            if (b_type != target_type) {
                std::string cast = fresh_reg();
                emit_line("  " + cast + " = trunc " + b_type + " " + b + " to " + target_type);
                b = cast;
            }

            // Use overflow intrinsic to detect overflow
            std::string overflow_type = "{ " + target_type + ", i1 }";
            std::string ov_result = fresh_reg();
            emit_line("  " + ov_result + " = call " + overflow_type + " @llvm.smul.with.overflow." +
                      target_type + "(" + target_type + " " + a + ", " + target_type + " " + b +
                      ")");

            std::string value = fresh_reg();
            std::string overflow = fresh_reg();
            emit_line("  " + value + " = extractvalue " + overflow_type + " " + ov_result + ", 0");
            emit_line("  " + overflow + " = extractvalue " + overflow_type + " " + ov_result +
                      ", 1");

            // Determine saturation value based on sign of operands
            // If (a ^ b) < 0, result is negative overflow -> MIN, else -> MAX
            std::string xor_val = fresh_reg();
            emit_line("  " + xor_val + " = xor " + target_type + " " + a + ", " + b);
            std::string is_neg = fresh_reg();
            emit_line("  " + is_neg + " = icmp slt " + target_type + " " + xor_val + ", 0");

            // Compute min and max for the type
            std::string min_val, max_val;
            if (target_type == "i8") {
                min_val = "-128";
                max_val = "127";
            } else if (target_type == "i16") {
                min_val = "-32768";
                max_val = "32767";
            } else if (target_type == "i32") {
                min_val = "-2147483648";
                max_val = "2147483647";
            } else if (target_type == "i64") {
                min_val = "-9223372036854775808";
                max_val = "9223372036854775807";
            } else {
                min_val = "-2147483648";
                max_val = "2147483647";
            }

            // Select saturation value: negative overflow -> MIN, positive overflow -> MAX
            std::string sat_val = fresh_reg();
            emit_line("  " + sat_val + " = select i1 " + is_neg + ", " + target_type + " " +
                      min_val + ", " + target_type + " " + max_val);

            // If overflow, use saturation value; otherwise, use computed value
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + overflow + ", " + target_type + " " +
                      sat_val + ", " + target_type + " " + value);

            last_expr_type_ = target_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Bit Manipulation Intrinsics
    // ============================================================================

    // ctlz[T](val: T) -> T (count leading zeros)
    if (intrinsic_name == "ctlz") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.ctlz." + val_type + "(" +
                      val_type + " " + val + ", i1 false)");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // cttz[T](val: T) -> T (count trailing zeros)
    if (intrinsic_name == "cttz") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.cttz." + val_type + "(" +
                      val_type + " " + val + ", i1 false)");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // ctpop[T](val: T) -> T (population count)
    if (intrinsic_name == "ctpop") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.ctpop." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // bswap[T](val: T) -> T (byte swap)
    if (intrinsic_name == "bswap") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.bswap." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // bitreverse[T](val: T) -> T
    if (intrinsic_name == "bitreverse") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.bitreverse." + val_type +
                      "(" + val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Math Intrinsics
    // ============================================================================

    // sqrt[T](val: T) -> T
    if (intrinsic_name == "sqrt") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.sqrt." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // sin[T](val: T) -> T
    if (intrinsic_name == "sin") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.sin." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // cos[T](val: T) -> T
    if (intrinsic_name == "cos") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.cos." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // log[T](val: T) -> T
    if (intrinsic_name == "log") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.log." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // exp[T](val: T) -> T
    if (intrinsic_name == "exp") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.exp." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // pow[T](base: T, exp: T) -> T
    // Only use LLVM intrinsic when both args are floats
    // For integer exponent, fall through to math handler (uses @float_pow)
    if (intrinsic_name == "pow") {
        if (call.args.size() >= 2) {
            std::string base = gen_expr(*call.args[0]);
            std::string base_type = last_expr_type_;
            std::string exp = gen_expr(*call.args[1]);
            std::string exp_type = last_expr_type_;
            // Only use LLVM intrinsic if both arguments are the same float type
            if ((base_type == "float" || base_type == "double") && base_type == exp_type) {
                std::string result = fresh_reg();
                emit_line("  " + result + " = call " + base_type + " @llvm.pow." + base_type + "(" +
                          base_type + " " + base + ", " + base_type + " " + exp + ")");
                last_expr_type_ = base_type;
                return result;
            }
            // For integer exponent with float base, let the math handler deal with it
            return std::nullopt;
        }
        return "1.0";
    }

    // floor[T](val: T) -> T
    if (intrinsic_name == "floor") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.floor." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // ceil[T](val: T) -> T
    if (intrinsic_name == "ceil") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.ceil." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // round[T](val: T) -> T
    if (intrinsic_name == "round") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.round." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // trunc[T](val: T) -> T
    if (intrinsic_name == "trunc") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.trunc." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // fma[T](a: T, b: T, c: T) -> T (fused multiply-add)
    if (intrinsic_name == "fma") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.fma." + a_type + "(" + a_type +
                      " " + a + ", " + a_type + " " + b + ", " + a_type + " " + c + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // minnum[T](a: T, b: T) -> T
    if (intrinsic_name == "minnum") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.minnum." + a_type + "(" +
                      a_type + " " + a + ", " + a_type + " " + b + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // maxnum[T](a: T, b: T) -> T
    if (intrinsic_name == "maxnum") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.maxnum." + a_type + "(" +
                      a_type + " " + a + ", " + a_type + " " + b + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // fabs[T](val: T) -> T
    if (intrinsic_name == "fabs") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.fabs." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // copysign[T](a: T, b: T) -> T
    if (intrinsic_name == "copysign") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.copysign." + a_type + "(" +
                      a_type + " " + a + ", " + a_type + " " + b + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // ============================================================================
    // Reflection Intrinsics
    // ============================================================================

    // field_count[T]() -> USize
    // Returns the number of fields in a struct type, 0 for primitives
    if (intrinsic_name == "field_count") {
        std::string type_name;

        // Extract type argument from PathExpr generics (e.g., field_count[MyStruct]())
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Look up struct fields
        size_t count = 0;
        if (!type_name.empty()) {
            auto it = struct_fields_.find(type_name);
            if (it != struct_fields_.end()) {
                count = it->second.size();
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(count);
    }

    // variant_count[T]() -> USize
    // Returns the number of variants in an enum type, 0 for structs/primitives
    if (intrinsic_name == "variant_count") {
        std::string type_name;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Count variants by looking for enum_variants_ entries with this prefix
        size_t count = 0;
        if (!type_name.empty()) {
            std::string prefix = type_name + "::";
            for (const auto& [key, _] : enum_variants_) {
                if (key.starts_with(prefix)) {
                    count++;
                }
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(count);
    }

    // field_name[T](index: USize) -> Str
    // Returns the name of the field at the given index as a string literal
    if (intrinsic_name == "field_name") {
        std::string type_name;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Get the index argument (must be a compile-time constant or comptime loop variable)
        size_t index = 0;
        bool has_index = false;
        if (!call.args.empty()) {
            if (call.args[0]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[0]->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                    has_index = true;
                }
            } else if (call.args[0]->is<parser::IdentExpr>()) {
                // Check if this is the compile-time loop variable
                const auto& ident = call.args[0]->as<parser::IdentExpr>();
                if (!comptime_loop_var_.empty() && ident.name == comptime_loop_var_) {
                    index = static_cast<size_t>(comptime_loop_value_);
                    has_index = true;
                }
            }
        }

        // Look up the field name
        std::string field_name = "";
        if (!type_name.empty() && has_index) {
            auto it = struct_fields_.find(type_name);
            if (it != struct_fields_.end() && index < it->second.size()) {
                field_name = it->second[index].name;
            }
        }

        // Return as string literal
        std::string str_const = add_string_literal(field_name);
        last_expr_type_ = "ptr";
        return str_const;
    }

    // field_type_id[T](index: USize) -> U64
    // Returns the type ID of the field at the given index
    if (intrinsic_name == "field_type_id") {
        std::string type_name;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Get the index argument (must be a compile-time constant or comptime loop variable)
        size_t index = 0;
        bool has_index = false;
        if (!call.args.empty()) {
            if (call.args[0]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[0]->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                    has_index = true;
                }
            } else if (call.args[0]->is<parser::IdentExpr>()) {
                // Check if this is the compile-time loop variable
                const auto& ident = call.args[0]->as<parser::IdentExpr>();
                if (!comptime_loop_var_.empty() && ident.name == comptime_loop_var_) {
                    index = static_cast<size_t>(comptime_loop_value_);
                    has_index = true;
                }
            }
        }

        // Look up the field's semantic type and compute its type ID
        uint64_t type_id = 0;
        if (!type_name.empty() && has_index) {
            auto it = struct_fields_.find(type_name);
            if (it != struct_fields_.end() && index < it->second.size()) {
                const auto& field = it->second[index];
                if (field.semantic_type) {
                    // Compute FNV-1a hash of the mangled type name
                    std::string mangled = mangle_type(field.semantic_type);
                    type_id = 14695981039346656037ULL;
                    for (char c : mangled) {
                        type_id ^= static_cast<uint64_t>(c);
                        type_id *= 1099511628211ULL;
                    }
                }
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(type_id);
    }

    // type_name[T]() -> Str
    // Returns the name of the type as a string literal
    if (intrinsic_name == "type_name") {
        std::string type_name = "unknown";

        // Extract type argument from PathExpr generics (e.g., type_name[I32]())
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    // Get a human-readable type name
                    if (resolved->is<types::NamedType>()) {
                        const auto& named = resolved->as<types::NamedType>();
                        type_name = named.name;
                        // Include type arguments for generic types
                        if (!named.type_args.empty()) {
                            type_name += "[";
                            for (size_t i = 0; i < named.type_args.size(); ++i) {
                                if (i > 0)
                                    type_name += ", ";
                                type_name += types::type_to_string(named.type_args[i]);
                            }
                            type_name += "]";
                        }
                    } else if (resolved->is<types::PrimitiveType>()) {
                        const auto& prim = resolved->as<types::PrimitiveType>();
                        switch (prim.kind) {
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
                        case types::PrimitiveKind::Char:
                            type_name = "Char";
                            break;
                        case types::PrimitiveKind::Str:
                            type_name = "Str";
                            break;
                        case types::PrimitiveKind::Unit:
                            type_name = "Unit";
                            break;
                        default:
                            type_name = "unknown";
                            break;
                        }
                    } else if (resolved->is<types::PtrType>()) {
                        type_name =
                            "*" + types::type_to_string(resolved->as<types::PtrType>().inner);
                    } else if (resolved->is<types::RefType>()) {
                        const auto& ref = resolved->as<types::RefType>();
                        type_name =
                            (ref.is_mut ? "mut ref " : "ref ") + types::type_to_string(ref.inner);
                    }
                }
            }
        }

        // Return as string literal
        std::string str_const = add_string_literal(type_name);
        last_expr_type_ = "ptr";
        return str_const;
    }

    // field_offset[T](index: USize) -> USize
    // Returns the byte offset of the field at the given index
    if (intrinsic_name == "field_offset") {
        std::string type_name;
        std::string llvm_type;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    llvm_type = llvm_type_from_semantic(resolved);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Get the index argument (must be a compile-time constant or comptime loop variable)
        size_t index = 0;
        if (!call.args.empty()) {
            if (call.args[0]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[0]->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                }
            } else if (call.args[0]->is<parser::IdentExpr>()) {
                // Check if this is the compile-time loop variable
                const auto& ident = call.args[0]->as<parser::IdentExpr>();
                if (!comptime_loop_var_.empty() && ident.name == comptime_loop_var_) {
                    index = static_cast<size_t>(comptime_loop_value_);
                }
            }
        }

        // Use GEP trick to compute field offset at compile time
        if (!llvm_type.empty() &&
            (llvm_type.starts_with("%struct.") || llvm_type.starts_with("%class."))) {
            std::string offset_ptr = fresh_reg();
            std::string offset_val = fresh_reg();
            emit_line("  " + offset_ptr + " = getelementptr " + llvm_type +
                      ", ptr null, i32 0, i32 " + std::to_string(index));
            emit_line("  " + offset_val + " = ptrtoint ptr " + offset_ptr + " to i64");
            last_expr_type_ = "i64";
            return offset_val;
        }

        last_expr_type_ = "i64";
        return "0";
    }

    // Intrinsic not implemented - return nullopt to fall through
    return std::nullopt;
}

} // namespace tml::codegen
