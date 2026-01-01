// LLVM IR generator - Primitive type methods
// Handles: Integer, Float, Bool methods (add, sub, mul, div, to_string, hash, cmp, etc.)

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_primitive_method(const parser::MethodCallExpr& call,
                                     const std::string& receiver, const std::string& receiver_ptr,
                                     types::TypePtr receiver_type) -> std::optional<std::string> {
    if (!receiver_type || !receiver_type->is<types::PrimitiveType>()) {
        return std::nullopt;
    }

    const std::string& method = call.method;
    const auto& prim = receiver_type->as<types::PrimitiveType>();
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

    // to_string() -> Str
    if (method == "to_string") {
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
        } else if (kind == types::PrimitiveKind::Str) {
            last_expr_type_ = "ptr";
            return receiver;
        } else {
            std::string ext = fresh_reg();
            emit_line("  " + ext + " = sext " + llvm_ty + " " + receiver + " to i64");
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

    return std::nullopt;
}

} // namespace tml::codegen
