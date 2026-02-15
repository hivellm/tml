//! # LLVM IR Generator - Primitive Type Methods (Part 2)
//!
//! This file is the continuation of method_primitive.cpp, implementing:
//! - Checked arithmetic (checked_add, checked_sub, checked_mul, checked_div, etc.)
//! - Hash method
//! - Str-specific methods (len, contains, split, trim, etc.)
//! - User-defined impl method lookup for primitive types
//! - Inline optimizations (is_zero, is_one, checked_shl, checked_shr)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_primitive_method_ext(const parser::MethodCallExpr& call,
                                         const std::string& receiver,
                                         const std::string& receiver_ptr,
                                         types::TypePtr receiver_type, types::TypePtr inner_type,
                                         types::PrimitiveKind kind, bool is_integer, bool is_signed,
                                         bool is_float, const std::string& llvm_ty)
    -> std::optional<std::string> {
    (void)receiver_type;
    const std::string& method = call.method;

    // ========================================================================
    // Checked arithmetic (returns Maybe[Self])
    // ========================================================================

    if ((method == "checked_add" || method == "checked_sub" || method == "checked_mul") &&
        is_integer) {
        std::string behavior_name = method == "checked_add"   ? "CheckedAdd"
                                    : method == "checked_sub" ? "CheckedSub"
                                                              : "CheckedMul";
        emit_coverage(behavior_name + "::" + method);
        if (call.args.empty()) {
            report_error(method + "() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);

        // Determine LLVM overflow op
        std::string op;
        if (method == "checked_add")
            op = is_signed ? "sadd" : "uadd";
        else if (method == "checked_sub")
            op = is_signed ? "ssub" : "usub";
        else
            op = is_signed ? "smul" : "umul";

        // Instantiate Maybe[T] enum
        std::vector<types::TypePtr> maybe_type_args = {inner_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        // Call overflow intrinsic: returns { T, i1 }
        std::string overflow_type = "{ " + llvm_ty + ", i1 }";
        std::string ov_result = fresh_reg();
        emit_line("  " + ov_result + " = call " + overflow_type + " @llvm." + op +
                  ".with.overflow." + llvm_ty + "(" + llvm_ty + " " + receiver + ", " + llvm_ty +
                  " " + other + ")");

        // Extract value and overflow flag
        std::string value = fresh_reg();
        std::string overflow = fresh_reg();
        emit_line("  " + value + " = extractvalue " + overflow_type + " " + ov_result + ", 0");
        emit_line("  " + overflow + " = extractvalue " + overflow_type + " " + ov_result + ", 1");

        // Extend value to i64 for enum payload
        std::string store_value = value;
        if (llvm_ty != "i64") {
            store_value = fresh_reg();
            if (is_signed) {
                emit_line("  " + store_value + " = sext " + llvm_ty + " " + value + " to i64");
            } else {
                emit_line("  " + store_value + " = zext " + llvm_ty + " " + value + " to i64");
            }
        }

        // Build Maybe[T] with alloca/store pattern
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + maybe_type);

        std::string label_just = "checked.just." + std::to_string(label_counter_++);
        std::string label_nothing = "checked.nothing." + std::to_string(label_counter_++);
        std::string label_end = "checked.end." + std::to_string(label_counter_++);

        emit_line("  br i1 " + overflow + ", label %" + label_nothing + ", label %" + label_just);

        // Just branch: tag=0, store value
        emit_line(label_just + ":");
        std::string tag_ptr_j = fresh_reg();
        emit_line("  " + tag_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + tag_ptr_j);
        std::string data_ptr_j = fresh_reg();
        emit_line("  " + data_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        emit_line("  store i64 " + store_value + ", ptr " + data_ptr_j);
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

    if (method == "checked_div" && is_integer) {
        emit_coverage("CheckedDiv::checked_div");
        if (call.args.empty()) {
            report_error("checked_div() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);

        // Instantiate Maybe[T] enum
        std::vector<types::TypePtr> maybe_type_args = {inner_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        // Check for division by zero
        std::string is_zero = fresh_reg();
        emit_line("  " + is_zero + " = icmp eq " + llvm_ty + " " + other + ", 0");

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + maybe_type);

        std::string label_ok = "checked.div.ok." + std::to_string(label_counter_++);
        std::string label_zero = "checked.div.zero." + std::to_string(label_counter_++);
        std::string label_end = "checked.div.end." + std::to_string(label_counter_++);

        emit_line("  br i1 " + is_zero + ", label %" + label_zero + ", label %" + label_ok);

        // OK branch: compute division, return Just(result)
        emit_line(label_ok + ":");
        std::string div_result = fresh_reg();
        if (is_signed) {
            emit_line("  " + div_result + " = sdiv " + llvm_ty + " " + receiver + ", " + other);
        } else {
            emit_line("  " + div_result + " = udiv " + llvm_ty + " " + receiver + ", " + other);
        }
        std::string store_val = div_result;
        if (llvm_ty != "i64") {
            store_val = fresh_reg();
            if (is_signed) {
                emit_line("  " + store_val + " = sext " + llvm_ty + " " + div_result + " to i64");
            } else {
                emit_line("  " + store_val + " = zext " + llvm_ty + " " + div_result + " to i64");
            }
        }
        std::string tag_ptr_j = fresh_reg();
        emit_line("  " + tag_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + tag_ptr_j);
        std::string data_ptr_j = fresh_reg();
        emit_line("  " + data_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        emit_line("  store i64 " + store_val + ", ptr " + data_ptr_j);
        emit_line("  br label %" + label_end);

        // Zero branch: return Nothing
        emit_line(label_zero + ":");
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

    if (method == "checked_rem" && is_integer) {
        emit_coverage("CheckedRem::checked_rem");
        if (call.args.empty()) {
            report_error("checked_rem() requires an argument", call.span, "C008");
            return "0";
        }
        std::string other = gen_expr(*call.args[0]);

        std::vector<types::TypePtr> maybe_type_args = {inner_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        std::string is_zero = fresh_reg();
        emit_line("  " + is_zero + " = icmp eq " + llvm_ty + " " + other + ", 0");

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + maybe_type);

        std::string label_ok = "checked.rem.ok." + std::to_string(label_counter_++);
        std::string label_zero = "checked.rem.zero." + std::to_string(label_counter_++);
        std::string label_end = "checked.rem.end." + std::to_string(label_counter_++);

        emit_line("  br i1 " + is_zero + ", label %" + label_zero + ", label %" + label_ok);

        emit_line(label_ok + ":");
        std::string rem_result = fresh_reg();
        if (is_signed) {
            emit_line("  " + rem_result + " = srem " + llvm_ty + " " + receiver + ", " + other);
        } else {
            emit_line("  " + rem_result + " = urem " + llvm_ty + " " + receiver + ", " + other);
        }
        std::string store_val = rem_result;
        if (llvm_ty != "i64") {
            store_val = fresh_reg();
            if (is_signed) {
                emit_line("  " + store_val + " = sext " + llvm_ty + " " + rem_result + " to i64");
            } else {
                emit_line("  " + store_val + " = zext " + llvm_ty + " " + rem_result + " to i64");
            }
        }
        std::string tag_ptr_j = fresh_reg();
        emit_line("  " + tag_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + tag_ptr_j);
        std::string data_ptr_j = fresh_reg();
        emit_line("  " + data_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        emit_line("  store i64 " + store_val + ", ptr " + data_ptr_j);
        emit_line("  br label %" + label_end);

        emit_line(label_zero + ":");
        std::string tag_ptr_n = fresh_reg();
        emit_line("  " + tag_ptr_n + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + tag_ptr_n);
        emit_line("  br label %" + label_end);

        emit_line(label_end + ":");
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + maybe_type + ", ptr " + alloca_reg);
        last_expr_type_ = maybe_type;
        return result;
    }

    if (method == "checked_neg" && is_integer) {
        emit_coverage("CheckedNeg::checked_neg");
        // For signed: overflow only when value == MIN
        // For unsigned: overflow unless value == 0
        std::vector<types::TypePtr> maybe_type_args = {inner_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        std::string is_overflow = fresh_reg();
        if (is_signed) {
            // Signed overflow: value == MIN (e.g., -128 for i8)
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
            emit_line("  " + is_overflow + " = icmp eq " + llvm_ty + " " + receiver + ", " +
                      std::to_string(min_val));
        } else {
            // Unsigned overflow: value != 0 (negating any nonzero unsigned overflows)
            emit_line("  " + is_overflow + " = icmp ne " + llvm_ty + " " + receiver + ", 0");
        }

        // Compute negation (even if it would overflow, we need the value for Just branch)
        std::string neg_result = fresh_reg();
        emit_line("  " + neg_result + " = sub " + llvm_ty + " 0, " + receiver);

        std::string store_val = neg_result;
        if (llvm_ty != "i64") {
            store_val = fresh_reg();
            if (is_signed) {
                emit_line("  " + store_val + " = sext " + llvm_ty + " " + neg_result + " to i64");
            } else {
                emit_line("  " + store_val + " = zext " + llvm_ty + " " + neg_result + " to i64");
            }
        }

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + maybe_type);

        std::string label_ok = "checked.neg.ok." + std::to_string(label_counter_++);
        std::string label_overflow = "checked.neg.overflow." + std::to_string(label_counter_++);
        std::string label_end = "checked.neg.end." + std::to_string(label_counter_++);

        emit_line("  br i1 " + is_overflow + ", label %" + label_overflow + ", label %" + label_ok);

        // OK branch: return Just(negated)
        emit_line(label_ok + ":");
        std::string tag_ptr_j = fresh_reg();
        emit_line("  " + tag_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + tag_ptr_j);
        std::string data_ptr_j = fresh_reg();
        emit_line("  " + data_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        emit_line("  store i64 " + store_val + ", ptr " + data_ptr_j);
        emit_line("  br label %" + label_end);

        // Overflow branch: return Nothing
        emit_line(label_overflow + ":");
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
            std::string start_type = last_expr_type_;
            std::string end = gen_expr(*call.args[1]);
            std::string end_type = last_expr_type_;
            // Ensure arguments are i64 to match str_slice(ptr, i64, i64) declaration
            if (start_type == "i32") {
                std::string ext = fresh_reg();
                emit_line("  " + ext + " = sext i32 " + start + " to i64");
                start = ext;
            }
            if (end_type == "i32") {
                std::string ext = fresh_reg();
                emit_line("  " + ext + " = sext i32 " + end + " to i64");
                end = ext;
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_slice(ptr " + receiver + ", i64 " + start +
                      ", i64 " + end + ")");
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

        // parse_i32() -> Maybe[I32]
        if (method == "parse_i32") {
            emit_coverage("Str::parse_i32");
            std::string value = fresh_reg();
            emit_line("  " + value + " = call i32 @str_parse_i32(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = insertvalue %struct.Maybe__I32 { i32 0, i32 undef }, i32 " + value +
                      ", 1");
            last_expr_type_ = "%struct.Maybe__I32";
            return result;
        }

        // parse_f64() -> F64
        if (method == "parse_f64") {
            emit_coverage("Str::parse_f64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @str_parse_f64(ptr " + receiver + ")");
            last_expr_type_ = "double";
            return result;
        }

        // parse_bool() -> Maybe[Bool]
        if (method == "parse_bool") {
            emit_coverage("Str::parse_bool");
            // Compare against "true" and "false" using str_eq (returns i32: 0 or 1)
            std::string true_lit = add_string_literal("true");
            std::string false_lit = add_string_literal("false");
            std::string is_true = fresh_reg();
            emit_line("  " + is_true + " = call i32 @str_eq(ptr " + receiver + ", ptr " + true_lit +
                      ")");
            std::string is_false = fresh_reg();
            emit_line("  " + is_false + " = call i32 @str_eq(ptr " + receiver + ", ptr " +
                      false_lit + ")");
            std::string is_valid = fresh_reg();
            emit_line("  " + is_valid + " = or i32 " + is_true + ", " + is_false);
            std::string is_valid_bool = fresh_reg();
            emit_line("  " + is_valid_bool + " = icmp ne i32 " + is_valid + ", 0");
            // tag: 0=Just, 1=Nothing
            std::string tag = fresh_reg();
            emit_line("  " + tag + " = select i1 " + is_valid_bool + ", i32 0, i32 1");
            // value: is_true as i1
            std::string val_bool = fresh_reg();
            emit_line("  " + val_bool + " = icmp ne i32 " + is_true + ", 0");
            std::string result = fresh_reg();
            emit_line("  " + result + " = insertvalue { i32, i1 } undef, i32 " + tag + ", 0");
            std::string result2 = fresh_reg();
            emit_line("  " + result2 + " = insertvalue { i32, i1 } " + result + ", i1 " + val_bool +
                      ", 1");
            last_expr_type_ = "%struct.Maybe__Bool";
            return result2;
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

    // checked_shl / checked_shr: check if shift amount >= bit width
    if (is_integer && (method == "checked_shl" || method == "checked_shr")) {
        emit_coverage("overflow::" + method);

        if (call.args.empty()) {
            report_error(method + "() requires one argument", call.span, "C008");
            return "0";
        }
        std::string rhs = gen_expr(*call.args[0]);

        // Get bit width
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

        // Instantiate Maybe[T] enum
        std::vector<types::TypePtr> maybe_type_args = {inner_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        // Check if shift >= bit_width (overflow)
        std::string is_overflow = fresh_reg();
        emit_line("  " + is_overflow + " = icmp uge " + llvm_ty + " " + rhs + ", " +
                  std::to_string(bits));

        // Safe shift
        std::string value = fresh_reg();
        std::string safe_rhs = fresh_reg();
        emit_line("  " + safe_rhs + " = select i1 " + is_overflow + ", " + llvm_ty + " 0, " +
                  llvm_ty + " " + rhs);
        if (method == "checked_shl") {
            emit_line("  " + value + " = shl " + llvm_ty + " " + receiver + ", " + safe_rhs);
        } else {
            if (is_signed)
                emit_line("  " + value + " = ashr " + llvm_ty + " " + receiver + ", " + safe_rhs);
            else
                emit_line("  " + value + " = lshr " + llvm_ty + " " + receiver + ", " + safe_rhs);
        }

        // Build Maybe[T] using proper named type
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + maybe_type);

        std::string label_ok = "checked.shift.ok." + std::to_string(label_counter_++);
        std::string label_overflow = "checked.shift.overflow." + std::to_string(label_counter_++);
        std::string label_end = "checked.shift.end." + std::to_string(label_counter_++);

        emit_line("  br i1 " + is_overflow + ", label %" + label_overflow + ", label %" + label_ok);

        // OK branch: return Just(result)
        emit_line(label_ok + ":");
        std::string store_val = value;
        if (llvm_ty != "i64") {
            store_val = fresh_reg();
            if (is_signed)
                emit_line("  " + store_val + " = sext " + llvm_ty + " " + value + " to i64");
            else
                emit_line("  " + store_val + " = zext " + llvm_ty + " " + value + " to i64");
        }
        std::string tag_ptr_j = fresh_reg();
        emit_line("  " + tag_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + tag_ptr_j);
        std::string data_ptr_j = fresh_reg();
        emit_line("  " + data_ptr_j + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        emit_line("  store i64 " + store_val + ", ptr " + data_ptr_j);
        emit_line("  br label %" + label_end);

        // Overflow branch: return Nothing
        emit_line(label_overflow + ":");
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
