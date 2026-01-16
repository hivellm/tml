//! # LLVM IR Generator - Primitive Type Methods
//!
//! This file implements method calls on primitive types.
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

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_primitive_method(const parser::MethodCallExpr& call,
                                     const std::string& receiver, const std::string& receiver_ptr,
                                     types::TypePtr receiver_type) -> std::optional<std::string> {
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
            if (call.args.empty()) {
                report_error("add() requires an argument", call.span);
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
            if (call.args.empty()) {
                report_error("sub() requires an argument", call.span);
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
            if (call.args.empty()) {
                report_error("mul() requires an argument", call.span);
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
            if (call.args.empty()) {
                report_error("div() requires an argument", call.span);
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
            if (call.args.empty()) {
                report_error("rem() requires an argument", call.span);
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
            if (call.args.empty()) {
                report_error("cmp() requires an argument", call.span);
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

        if (method == "max") {
            if (call.args.empty()) {
                report_error("max() requires an argument", call.span);
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
            if (call.args.empty()) {
                report_error("min() requires an argument", call.span);
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
    }

    // Bool operations
    if (kind == types::PrimitiveKind::Bool) {
        if (method == "negate") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor i1 " + receiver + ", true");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // duplicate() -> Self (copy semantics for primitives)
    if (method == "duplicate") {
        last_expr_type_ = llvm_ty;
        return receiver;
    }

    // to_owned() -> Self
    if (method == "to_owned") {
        last_expr_type_ = llvm_ty;
        return receiver;
    }

    // borrow() -> ref Self
    if (method == "borrow") {
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
        std::string result = fresh_reg();
        if (kind == types::PrimitiveKind::Bool) {
            std::string ext = fresh_reg();
            emit_line("  " + ext + " = zext i1 " + receiver + " to i32");
            emit_line("  " + result + " = call ptr @bool_to_string(i32 " + ext + ")");
        } else if (kind == types::PrimitiveKind::I32) {
            emit_line("  " + result + " = call ptr @i32_to_string(i32 " + receiver + ")");
        } else if (kind == types::PrimitiveKind::I64) {
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

    // hash() -> I64
    if (method == "hash") {
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
            std::string len32 = fresh_reg();
            emit_line("  " + len32 + " = call i32 @str_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext i32 " + len32 + " to i64");
            last_expr_type_ = "i64";
            return result;
        }

        // is_empty() -> Bool
        if (method == "is_empty") {
            std::string len32 = fresh_reg();
            emit_line("  " + len32 + " = call i32 @str_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp eq i32 " + len32 + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // as_bytes() -> ref [U8] (returns pointer to string data)
        if (method == "as_bytes") {
            // For now, return the string pointer itself as a slice
            // In TML, strings are already represented as pointers to their data
            last_expr_type_ = "ptr";
            return receiver;
        }

        // char_at(index: I64) -> I32
        if (method == "char_at") {
            if (call.args.empty()) {
                report_error("char_at() requires an index argument", call.span);
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
            if (call.args.size() < 2) {
                report_error("slice_str() requires start and end arguments", call.span);
                return "0";
            }
            std::string start = gen_expr(*call.args[0]);
            std::string end = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_substring(ptr " + receiver + ", i64 " +
                      start + ", i64 " + end + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // to_uppercase() -> Str
        if (method == "to_uppercase") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_to_upper(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // to_lowercase() -> Str
        if (method == "to_lowercase") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_to_lower(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // starts_with(prefix: Str) -> Bool
        if (method == "starts_with") {
            if (call.args.empty()) {
                report_error("starts_with() requires a prefix argument", call.span);
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
            if (call.args.empty()) {
                report_error("ends_with() requires a suffix argument", call.span);
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
            if (call.args.empty()) {
                report_error("contains() requires a pattern argument", call.span);
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
            if (call.args.empty()) {
                report_error("find() requires a pattern argument", call.span);
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
            if (call.args.empty()) {
                report_error("rfind() requires a pattern argument", call.span);
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
            if (call.args.empty()) {
                report_error("split() requires a delimiter argument", call.span);
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
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_chars(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // trim() -> Str
        if (method == "trim") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_trim(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // trim_start() -> Str
        if (method == "trim_start") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_trim_start(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // trim_end() -> Str
        if (method == "trim_end") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_trim_end(ptr " + receiver + ")");
            last_expr_type_ = "ptr";
            return result;
        }

        // parse_i64() -> Maybe[I64]
        if (method == "parse_i64") {
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

        // parse_u16() -> Maybe[U16]
        if (method == "parse_u16") {
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
            if (call.args.size() < 2) {
                report_error("replace() requires 'from' and 'to' arguments", call.span);
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
