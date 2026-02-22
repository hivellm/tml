//! # LLVM IR Generator - Outcome[T, E] Methods
//!
//! This file implements methods for the `Outcome[T, E]` result type.
//!
//! ## Methods
//!
//! | Method       | Signature                 | Description             |
//! |--------------|---------------------------|-------------------------|
//! | `is_ok`      | `() -> Bool`              | Check if Ok variant     |
//! | `is_err`     | `() -> Bool`              | Check if Err variant    |
//! | `unwrap`     | `() -> T`                 | Extract value or panic  |
//! | `unwrap_err` | `() -> E`                 | Extract error or panic  |
//! | `ok`         | `() -> Maybe[T]`          | Convert to Maybe        |
//! | `err`        | `() -> Maybe[E]`          | Get error as Maybe      |

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Helper: extract the effective value expression from a closure body.
// See method_maybe.cpp for the canonical version.
static const parser::Expr& get_closure_value_expr_outcome(const parser::Expr& body) {
    if (body.is<parser::BlockExpr>()) {
        const auto& block = body.as<parser::BlockExpr>();
        if (block.stmts.size() == 1 && !block.expr.has_value()) {
            const auto& stmt = *block.stmts[0];
            if (stmt.is<parser::ExprStmt>()) {
                const auto& expr_stmt = stmt.as<parser::ExprStmt>();
                if (expr_stmt.expr->is<parser::ReturnExpr>()) {
                    const auto& ret = expr_stmt.expr->as<parser::ReturnExpr>();
                    if (ret.value.has_value()) {
                        return *ret.value.value();
                    }
                }
            }
        }
        if (block.stmts.empty() && block.expr.has_value()) {
            // If the trailing expression is itself a ReturnExpr, unwrap it
            if (block.expr.value()->is<parser::ReturnExpr>()) {
                const auto& ret = block.expr.value()->as<parser::ReturnExpr>();
                if (ret.value.has_value()) {
                    return *ret.value.value();
                }
            }
            return *block.expr.value();
        }
    }
    if (body.is<parser::ReturnExpr>()) {
        const auto& ret = body.as<parser::ReturnExpr>();
        if (ret.value.has_value()) {
            return *ret.value.value();
        }
    }
    return body;
}

auto LLVMIRGen::gen_outcome_method(const parser::MethodCallExpr& call, const std::string& receiver,
                                   const std::string& enum_type_name, const std::string& tag_val,
                                   const types::NamedType& named) -> std::optional<std::string> {
    const std::string& method = call.method;

    // is_ok() -> Bool (tag == 0)
    if (method == "is_ok") {
        emit_coverage("Outcome::is_ok");
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
        last_expr_type_ = "i1";
        return result;
    }

    // is_err() -> Bool (tag == 1)
    if (method == "is_err") {
        emit_coverage("Outcome::is_err");
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
        last_expr_type_ = "i1";
        return result;
    }

    types::TypePtr ok_type = named.type_args[0];
    types::TypePtr err_type = named.type_args[1];
    std::string ok_llvm_type = llvm_type_from_semantic(ok_type, true);
    std::string err_llvm_type = llvm_type_from_semantic(err_type, true);

    // unwrap() -> T (get the Ok value)
    if (method == "unwrap" || method == "expect") {
        emit_coverage(method == "expect" ? "Outcome::expect" : "Outcome::unwrap");
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + ok_llvm_type + ", ptr " + data_ptr);
        last_expr_type_ = ok_llvm_type;
        return result;
    }

    // unwrap_err() -> E (get the Err value)
    if (method == "unwrap_err" || method == "expect_err") {
        emit_coverage(method == "expect_err" ? "Outcome::expect_err" : "Outcome::unwrap_err");
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + err_llvm_type + ", ptr " + data_ptr);
        last_expr_type_ = err_llvm_type;
        return result;
    }

    // unwrap_or(default) -> T
    if (method == "unwrap_or") {
        emit_coverage("Outcome::unwrap_or");
        if (call.args.empty()) {
            report_error("unwrap_or requires an argument", call.span, "C015");
            return "0";
        }
        std::string default_val = gen_expr(*call.args[0]);

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_ok + ", " + ok_llvm_type + " " + ok_val +
                  ", " + ok_llvm_type + " " + default_val);
        last_expr_type_ = ok_llvm_type;
        return result;
    }

    // unwrap_or_default() -> T
    if (method == "unwrap_or_default") {
        emit_coverage("Outcome::unwrap_or_default");
        std::string default_val;
        if (ok_llvm_type == "i8" || ok_llvm_type == "i16" || ok_llvm_type == "i32" ||
            ok_llvm_type == "i64" || ok_llvm_type == "i128") {
            default_val = "0";
        } else if (ok_llvm_type == "float" || ok_llvm_type == "double") {
            default_val = "0.0";
        } else if (ok_llvm_type == "i1") {
            default_val = "false";
        } else {
            default_val = "zeroinitializer";
        }

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_ok + ", " + ok_llvm_type + " " + ok_val +
                  ", " + ok_llvm_type + " " + default_val);
        last_expr_type_ = ok_llvm_type;
        return result;
    }

    // ok() -> Maybe[T]
    if (method == "ok") {
        emit_coverage("Outcome::ok");
        std::vector<types::TypePtr> maybe_type_args = {ok_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        std::string is_ok_label = fresh_label("is_ok");
        std::string is_err_label = fresh_label("is_err");
        std::string end_label = fresh_label("ok_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string just_alloca = fresh_reg();
        emit_line("  " + just_alloca + " = alloca " + maybe_type);
        std::string just_tag_ptr = fresh_reg();
        emit_line("  " + just_tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  just_alloca + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + just_tag_ptr);
        std::string just_data_ptr = fresh_reg();
        emit_line("  " + just_data_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  just_alloca + ", i32 0, i32 1");
        emit_line("  store " + ok_llvm_type + " " + ok_val + ", ptr " + just_data_ptr);
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + maybe_type + ", ptr " + just_alloca);
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + maybe_type);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_val = fresh_reg();
        emit_line("  " + nothing_val + " = load " + maybe_type + ", ptr " + nothing_alloca);
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + maybe_type + " [ " + just_val + ", %" + is_ok_label +
                  " ], [ " + nothing_val + ", %" + is_err_label + " ]");
        last_expr_type_ = maybe_type;
        return result;
    }

    // err() -> Maybe[E]
    if (method == "err") {
        emit_coverage("Outcome::err");
        std::vector<types::TypePtr> maybe_type_args = {err_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string err_val = fresh_reg();
        emit_line("  " + err_val + " = load " + err_llvm_type + ", ptr " + data_ptr);

        std::string is_err_label = fresh_label("is_err");
        std::string is_ok_label = fresh_label("is_ok");
        std::string end_label = fresh_label("err_end");

        std::string is_err = fresh_reg();
        emit_line("  " + is_err + " = icmp eq i32 " + tag_val + ", 1");
        emit_line("  br i1 " + is_err + ", label %" + is_err_label + ", label %" + is_ok_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string just_alloca = fresh_reg();
        emit_line("  " + just_alloca + " = alloca " + maybe_type);
        std::string just_tag_ptr = fresh_reg();
        emit_line("  " + just_tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  just_alloca + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + just_tag_ptr);
        std::string just_data_ptr = fresh_reg();
        emit_line("  " + just_data_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  just_alloca + ", i32 0, i32 1");
        emit_line("  store " + err_llvm_type + " " + err_val + ", ptr " + just_data_ptr);
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + maybe_type + ", ptr " + just_alloca);
        emit_line("  br label %" + end_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + maybe_type);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_val = fresh_reg();
        emit_line("  " + nothing_val + " = load " + maybe_type + ", ptr " + nothing_alloca);
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + maybe_type + " [ " + just_val + ", %" + is_err_label +
                  " ], [ " + nothing_val + ", %" + is_ok_label + " ]");
        last_expr_type_ = maybe_type;
        return result;
    }

    // contains(ref value) -> Bool
    if (method == "contains") {
        emit_coverage("Outcome::contains");
        if (call.args.empty()) {
            report_error("contains requires an argument", call.span, "C015");
            return "false";
        }
        std::string cmp_val = gen_expr(*call.args[0]);
        std::string cmp_val_type = last_expr_type_;

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");

        std::string is_ok_label = fresh_label("contains_ok_check");
        std::string is_not_ok_label = fresh_label("contains_ok_false");
        std::string end_label = fresh_label("contains_ok_end");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_not_ok_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        // If cmp_val is a pointer (ref argument), load the value
        // We always need to dereference the ref, even for Str (ptr) types
        std::string cmp_val_deref = cmp_val;
        if (cmp_val_type == "ptr") {
            cmp_val_deref = fresh_reg();
            emit_line("  " + cmp_val_deref + " = load " + ok_llvm_type + ", ptr " + cmp_val);
        }

        std::string values_eq = fresh_reg();
        if (ok_llvm_type == "ptr") {
            // str_eq returns i32, convert to i1
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + ok_val + ", ptr " +
                      cmp_val_deref + ")");
            emit_line("  " + values_eq + " = icmp ne i32 " + eq_i32 + ", 0");
        } else {
            emit_line("  " + values_eq + " = icmp eq " + ok_llvm_type + " " + ok_val + ", " +
                      cmp_val_deref);
        }
        emit_line("  br label %" + end_label);

        emit_line(is_not_ok_label + ":");
        current_block_ = is_not_ok_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi i1 [ " + values_eq + ", %" + is_ok_label +
                  " ], [ false, %" + is_not_ok_label + " ]");
        last_expr_type_ = "i1";
        return result;
    }

    // contains_err(ref value) -> Bool
    if (method == "contains_err") {
        emit_coverage("Outcome::contains_err");
        if (call.args.empty()) {
            report_error("contains_err requires an argument", call.span, "C015");
            return "false";
        }
        std::string cmp_val = gen_expr(*call.args[0]);
        std::string cmp_val_type = last_expr_type_;

        std::string is_err = fresh_reg();
        emit_line("  " + is_err + " = icmp eq i32 " + tag_val + ", 1");

        std::string is_err_label = fresh_label("contains_err_check");
        std::string is_not_err_label = fresh_label("contains_err_false");
        std::string end_label = fresh_label("contains_err_end");
        emit_line("  br i1 " + is_err + ", label %" + is_err_label + ", label %" +
                  is_not_err_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string err_val = fresh_reg();
        emit_line("  " + err_val + " = load " + err_llvm_type + ", ptr " + data_ptr);

        // If cmp_val is a pointer (ref argument), load the value
        // We always need to dereference the ref, even for Str (ptr) types
        std::string cmp_val_deref = cmp_val;
        if (cmp_val_type == "ptr") {
            cmp_val_deref = fresh_reg();
            emit_line("  " + cmp_val_deref + " = load " + err_llvm_type + ", ptr " + cmp_val);
        }

        std::string values_eq = fresh_reg();
        if (err_llvm_type == "ptr") {
            // str_eq returns i32, convert to i1
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + err_val + ", ptr " +
                      cmp_val_deref + ")");
            emit_line("  " + values_eq + " = icmp ne i32 " + eq_i32 + ", 0");
        } else {
            emit_line("  " + values_eq + " = icmp eq " + err_llvm_type + " " + err_val + ", " +
                      cmp_val_deref);
        }
        emit_line("  br label %" + end_label);

        emit_line(is_not_err_label + ":");
        current_block_ = is_not_err_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi i1 [ " + values_eq + ", %" + is_err_label +
                  " ], [ false, %" + is_not_err_label + " ]");
        last_expr_type_ = "i1";
        return result;
    }

    // alt(other) -> Outcome[T, E]
    if (method == "alt") {
        emit_coverage("Outcome::alt");
        if (call.args.empty()) {
            report_error("alt requires an argument", call.span, "C015");
            return receiver;
        }
        std::string other = gen_expr(*call.args[0]);

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_ok + ", " + enum_type_name + " " + receiver +
                  ", " + enum_type_name + " " + other);
        last_expr_type_ = enum_type_name;
        return result;
    }

    // also(other) -> Outcome[U, E]
    if (method == "also") {
        emit_coverage("Outcome::also");
        if (call.args.empty()) {
            report_error("also requires an argument", call.span, "C015");
            return receiver;
        }
        std::string other = gen_expr(*call.args[0]);
        std::string other_type = last_expr_type_;

        std::string is_ok_label = fresh_label("also_ok");
        std::string is_err_label = fresh_label("also_err");
        std::string end_label = fresh_label("also_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string err_val = fresh_reg();
        emit_line("  " + err_val + " = load " + err_llvm_type + ", ptr " + data_ptr);

        std::string err_alloca = fresh_reg();
        emit_line("  " + err_alloca + " = alloca " + other_type);
        std::string err_tag_ptr = fresh_reg();
        emit_line("  " + err_tag_ptr + " = getelementptr inbounds " + other_type + ", ptr " +
                  err_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + err_tag_ptr);
        std::string err_data_ptr = fresh_reg();
        emit_line("  " + err_data_ptr + " = getelementptr inbounds " + other_type + ", ptr " +
                  err_alloca + ", i32 0, i32 1");
        emit_line("  store " + err_llvm_type + " " + err_val + ", ptr " + err_data_ptr);
        std::string err_result = fresh_reg();
        emit_line("  " + err_result + " = load " + other_type + ", ptr " + err_alloca);
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + other_type + " [ " + other + ", %" + is_ok_label +
                  " ], [ " + err_result + ", %" + is_err_label + " ]");
        last_expr_type_ = other_type;
        return result;
    }

    // is_ok_and(predicate) -> Bool
    if (method == "is_ok_and") {
        emit_coverage("Outcome::is_ok_and");
        if (call.args.empty()) {
            report_error("is_ok_and requires a predicate argument", call.span, "C017");
            return "false";
        }
        bool is_closure = call.args[0]->is<parser::ClosureExpr>();
        bool is_func_ref = call.args[0]->is<parser::IdentExpr>();
        if (!is_closure && !is_func_ref) {
            report_error("is_ok_and requires a closure or function reference", call.span, "C021");
            return "false";
        }

        std::string is_ok_label = fresh_label("is_ok_and_ok");
        std::string is_err_label = fresh_label("is_ok_and_err");
        std::string end_label = fresh_label("is_ok_and_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        std::string pred_result;
        if (is_closure) {
            auto& closure = call.args[0]->as<parser::ClosureExpr>();
            std::string param_name = "_";
            if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
                param_name = closure.params[0].first->as<parser::IdentPattern>().name;
            }
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca " + ok_llvm_type);
            emit_line("  store " + ok_llvm_type + " " + ok_val + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, ok_llvm_type, nullptr, std::nullopt};

            std::string merge = fresh_label("is_ok_and_merge");
            std::string ret_alloca = fresh_reg();
            emit_line("  " + ret_alloca + " = alloca i1");
            std::string saved_ca = closure_return_alloca_;
            std::string saved_ct = closure_return_type_;
            std::string saved_cl = closure_return_label_;
            closure_return_alloca_ = ret_alloca;
            closure_return_type_ = "i1";
            closure_return_label_ = merge;

            pred_result = gen_expr(get_closure_value_expr_outcome(*closure.body));

            closure_return_alloca_ = saved_ca;
            closure_return_type_ = saved_ct;
            closure_return_label_ = saved_cl;

            if (!block_terminated_) {
                emit_line("  store i1 " + pred_result + ", ptr " + ret_alloca);
                emit_line("  br label %" + merge);
            }
            emit_line(merge + ":");
            current_block_ = merge;
            block_terminated_ = false;
            pred_result = fresh_reg();
            emit_line("  " + pred_result + " = load i1, ptr " + ret_alloca);
            locals_.erase(param_name);
        } else {
            // Function reference - generate a call
            auto& ident = call.args[0]->as<parser::IdentExpr>();
            std::string fn_name = "@tml_" + get_suite_prefix() + ident.name;
            pred_result = fresh_reg();
            emit_line("  " + pred_result + " = call i1 " + fn_name + "(" + ok_llvm_type + " " +
                      ok_val + ")");
        }
        std::string ok_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi i1 [ " + pred_result + ", %" + ok_end_block +
                  " ], [ false, %" + is_err_label + " ]");
        last_expr_type_ = "i1";
        return result;
    }

    // is_err_and(predicate) -> Bool
    if (method == "is_err_and") {
        emit_coverage("Outcome::is_err_and");
        if (call.args.empty()) {
            report_error("is_err_and requires a predicate argument", call.span, "C017");
            return "false";
        }
        bool is_closure = call.args[0]->is<parser::ClosureExpr>();
        bool is_func_ref = call.args[0]->is<parser::IdentExpr>();
        if (!is_closure && !is_func_ref) {
            report_error("is_err_and requires a closure or function reference", call.span, "C021");
            return "false";
        }

        std::string is_err_label = fresh_label("is_err_and_err");
        std::string is_ok_label = fresh_label("is_err_and_ok");
        std::string end_label = fresh_label("is_err_and_end");

        std::string is_err = fresh_reg();
        emit_line("  " + is_err + " = icmp eq i32 " + tag_val + ", 1");
        emit_line("  br i1 " + is_err + ", label %" + is_err_label + ", label %" + is_ok_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string err_val = fresh_reg();
        emit_line("  " + err_val + " = load " + err_llvm_type + ", ptr " + data_ptr);

        std::string pred_result;
        if (is_closure) {
            auto& closure = call.args[0]->as<parser::ClosureExpr>();
            std::string param_name = "_";
            if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
                param_name = closure.params[0].first->as<parser::IdentPattern>().name;
            }
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca " + err_llvm_type);
            emit_line("  store " + err_llvm_type + " " + err_val + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, err_llvm_type, nullptr, std::nullopt};

            std::string merge = fresh_label("is_err_and_merge");
            std::string ret_alloca = fresh_reg();
            emit_line("  " + ret_alloca + " = alloca i1");
            std::string saved_ca = closure_return_alloca_;
            std::string saved_ct = closure_return_type_;
            std::string saved_cl = closure_return_label_;
            closure_return_alloca_ = ret_alloca;
            closure_return_type_ = "i1";
            closure_return_label_ = merge;

            pred_result = gen_expr(get_closure_value_expr_outcome(*closure.body));

            closure_return_alloca_ = saved_ca;
            closure_return_type_ = saved_ct;
            closure_return_label_ = saved_cl;

            if (!block_terminated_) {
                emit_line("  store i1 " + pred_result + ", ptr " + ret_alloca);
                emit_line("  br label %" + merge);
            }
            emit_line(merge + ":");
            current_block_ = merge;
            block_terminated_ = false;
            pred_result = fresh_reg();
            emit_line("  " + pred_result + " = load i1, ptr " + ret_alloca);
            locals_.erase(param_name);
        } else {
            // Function reference - generate a call
            auto& ident = call.args[0]->as<parser::IdentExpr>();
            std::string fn_name = "@tml_" + get_suite_prefix() + ident.name;
            pred_result = fresh_reg();
            emit_line("  " + pred_result + " = call i1 " + fn_name + "(" + err_llvm_type + " " +
                      err_val + ")");
        }
        std::string err_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi i1 [ " + pred_result + ", %" + err_end_block +
                  " ], [ false, %" + is_ok_label + " ]");
        last_expr_type_ = "i1";
        return result;
    }

    // unwrap_or_else(f) -> T
    if (method == "unwrap_or_else") {
        emit_coverage("Outcome::unwrap_or_else");
        if (call.args.empty()) {
            report_error("unwrap_or_else requires a function argument", call.span, "C017");
            return "0";
        }
        bool is_closure = call.args[0]->is<parser::ClosureExpr>();
        bool is_func_ref = call.args[0]->is<parser::IdentExpr>();
        if (!is_closure && !is_func_ref) {
            report_error("unwrap_or_else requires a closure or function reference", call.span,
                         "C021");
            return "0";
        }

        std::string is_ok_label = fresh_label("unwrap_else_ok");
        std::string is_err_label = fresh_label("unwrap_else_err");
        std::string end_label = fresh_label("unwrap_else_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string alloca_reg2 = fresh_reg();
        emit_line("  " + alloca_reg2 + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg2);
        std::string data_ptr2 = fresh_reg();
        emit_line("  " + data_ptr2 + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg2 + ", i32 0, i32 1");
        std::string err_val = fresh_reg();
        emit_line("  " + err_val + " = load " + err_llvm_type + ", ptr " + data_ptr2);

        std::string closure_result;
        if (is_closure) {
            auto& closure = call.args[0]->as<parser::ClosureExpr>();
            std::string param_name = "_";
            if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
                param_name = closure.params[0].first->as<parser::IdentPattern>().name;
            }
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca " + err_llvm_type);
            emit_line("  store " + err_llvm_type + " " + err_val + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, err_llvm_type, nullptr, std::nullopt};

            std::string merge = fresh_label("unwrap_else_merge");
            std::string ret_alloca = fresh_reg();
            emit_line("  " + ret_alloca + " = alloca " + ok_llvm_type);
            std::string saved_ca = closure_return_alloca_;
            std::string saved_ct = closure_return_type_;
            std::string saved_cl = closure_return_label_;
            closure_return_alloca_ = ret_alloca;
            closure_return_type_ = ok_llvm_type;
            closure_return_label_ = merge;

            closure_result = gen_expr(get_closure_value_expr_outcome(*closure.body));

            closure_return_alloca_ = saved_ca;
            closure_return_type_ = saved_ct;
            closure_return_label_ = saved_cl;

            if (!block_terminated_) {
                emit_line("  store " + ok_llvm_type + " " + closure_result + ", ptr " + ret_alloca);
                emit_line("  br label %" + merge);
            }
            emit_line(merge + ":");
            current_block_ = merge;
            block_terminated_ = false;
            closure_result = fresh_reg();
            emit_line("  " + closure_result + " = load " + ok_llvm_type + ", ptr " + ret_alloca);
            locals_.erase(param_name);
        } else {
            auto& ident = call.args[0]->as<parser::IdentExpr>();
            std::string fn_name = "@tml_" + get_suite_prefix() + ident.name;
            closure_result = fresh_reg();
            emit_line("  " + closure_result + " = call " + ok_llvm_type + " " + fn_name + "(" +
                      err_llvm_type + " " + err_val + ")");
        }
        std::string err_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + ok_llvm_type + " [ " + ok_val + ", %" + is_ok_label +
                  " ], [ " + closure_result + ", %" + err_end_block + " ]");
        last_expr_type_ = ok_llvm_type;
        return result;
    }

    // map(f) -> Outcome[U, E]
    if (method == "map") {
        emit_coverage("Outcome::map");
        if (call.args.empty()) {
            report_error("map requires a function argument", call.span, "C017");
            return receiver;
        }
        bool is_closure = call.args[0]->is<parser::ClosureExpr>();
        bool is_func_ref = call.args[0]->is<parser::IdentExpr>();
        if (!is_closure && !is_func_ref) {
            report_error("map requires a closure or function reference", call.span, "C021");
            return receiver;
        }

        std::string is_ok_label = fresh_label("map_ok");
        std::string is_err_label = fresh_label("map_err");
        std::string end_label = fresh_label("map_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        std::string mapped_val;
        std::string mapped_type;
        if (is_closure) {
            auto& closure = call.args[0]->as<parser::ClosureExpr>();
            std::string param_name = "_";
            if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
                param_name = closure.params[0].first->as<parser::IdentPattern>().name;
            }
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca " + ok_llvm_type);
            emit_line("  store " + ok_llvm_type + " " + ok_val + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, ok_llvm_type, nullptr, std::nullopt};

            std::string merge = fresh_label("map_ok_merge");
            std::string ret_alloca = fresh_reg();
            emit_line("  " + ret_alloca + " = alloca " + ok_llvm_type);
            std::string saved_ca = closure_return_alloca_;
            std::string saved_ct = closure_return_type_;
            std::string saved_cl = closure_return_label_;
            closure_return_alloca_ = ret_alloca;
            closure_return_type_ = ok_llvm_type;
            closure_return_label_ = merge;

            mapped_val = gen_expr(get_closure_value_expr_outcome(*closure.body));
            mapped_type = last_expr_type_;

            closure_return_alloca_ = saved_ca;
            closure_return_type_ = saved_ct;
            closure_return_label_ = saved_cl;

            if (!block_terminated_) {
                emit_line("  store " + mapped_type + " " + mapped_val + ", ptr " + ret_alloca);
                emit_line("  br label %" + merge);
            }
            emit_line(merge + ":");
            current_block_ = merge;
            block_terminated_ = false;
            mapped_val = fresh_reg();
            emit_line("  " + mapped_val + " = load " + mapped_type + ", ptr " + ret_alloca);
            locals_.erase(param_name);
        } else {
            // Function reference - we need to infer the return type
            auto& ident = call.args[0]->as<parser::IdentExpr>();
            std::string fn_name = "@tml_" + get_suite_prefix() + ident.name;
            // For now, assume the mapped type is the same as ok_type
            // The type checker should have already validated this
            mapped_type = ok_llvm_type;
            mapped_val = fresh_reg();
            emit_line("  " + mapped_val + " = call " + mapped_type + " " + fn_name + "(" +
                      ok_llvm_type + " " + ok_val + ")");
        }

        std::string result_type_name = enum_type_name;
        if (mapped_type != ok_llvm_type) {
            types::TypePtr mapped_semantic_type = semantic_type_from_llvm(mapped_type);
            std::vector<types::TypePtr> new_type_args = {mapped_semantic_type, err_type};
            std::string new_mangled = require_enum_instantiation("Outcome", new_type_args);
            result_type_name = "%struct." + new_mangled;
        }

        std::string ok_alloca = fresh_reg();
        emit_line("  " + ok_alloca + " = alloca " + result_type_name);
        std::string ok_tag_ptr = fresh_reg();
        emit_line("  " + ok_tag_ptr + " = getelementptr inbounds " + result_type_name + ", ptr " +
                  ok_alloca + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + ok_tag_ptr);
        std::string ok_data_ptr = fresh_reg();
        emit_line("  " + ok_data_ptr + " = getelementptr inbounds " + result_type_name + ", ptr " +
                  ok_alloca + ", i32 0, i32 1");
        emit_line("  store " + mapped_type + " " + mapped_val + ", ptr " + ok_data_ptr);
        std::string ok_result = fresh_reg();
        emit_line("  " + ok_result + " = load " + result_type_name + ", ptr " + ok_alloca);
        std::string ok_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string err_result;
        if (result_type_name != enum_type_name) {
            std::string err_alloca_orig = fresh_reg();
            emit_line("  " + err_alloca_orig + " = alloca " + enum_type_name);
            emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + err_alloca_orig);
            std::string err_data_ptr_orig = fresh_reg();
            emit_line("  " + err_data_ptr_orig + " = getelementptr inbounds " + enum_type_name +
                      ", ptr " + err_alloca_orig + ", i32 0, i32 1");
            std::string err_val_orig = fresh_reg();
            emit_line("  " + err_val_orig + " = load " + err_llvm_type + ", ptr " +
                      err_data_ptr_orig);

            std::string new_err_alloca = fresh_reg();
            emit_line("  " + new_err_alloca + " = alloca " + result_type_name);
            std::string new_err_tag_ptr = fresh_reg();
            emit_line("  " + new_err_tag_ptr + " = getelementptr inbounds " + result_type_name +
                      ", ptr " + new_err_alloca + ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + new_err_tag_ptr);
            std::string new_err_data_ptr = fresh_reg();
            emit_line("  " + new_err_data_ptr + " = getelementptr inbounds " + result_type_name +
                      ", ptr " + new_err_alloca + ", i32 0, i32 1");
            emit_line("  store " + err_llvm_type + " " + err_val_orig + ", ptr " +
                      new_err_data_ptr);
            err_result = fresh_reg();
            emit_line("  " + err_result + " = load " + result_type_name + ", ptr " +
                      new_err_alloca);
        } else {
            err_result = receiver;
        }
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + result_type_name + " [ " + ok_result + ", %" +
                  ok_end_block + " ], [ " + err_result + ", %" + is_err_label + " ]");
        last_expr_type_ = result_type_name;
        return result;
    }

    // map_or(default, f) -> U
    if (method == "map_or") {
        emit_coverage("Outcome::map_or");
        if (call.args.size() < 2) {
            report_error("map_or requires a default value and a function", call.span, "C018");
            return "0";
        }
        bool is_closure = call.args[1]->is<parser::ClosureExpr>();
        bool is_func_ref = call.args[1]->is<parser::IdentExpr>();
        if (!is_closure && !is_func_ref) {
            report_error("map_or requires a closure or function reference", call.span, "C021");
            return "0";
        }

        std::string default_val = gen_expr(*call.args[0]);
        std::string default_type = last_expr_type_;

        std::string is_ok_label = fresh_label("map_or_ok");
        std::string is_err_label = fresh_label("map_or_err");
        std::string end_label = fresh_label("map_or_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        std::string mapped_val;
        if (is_closure) {
            auto& closure = call.args[1]->as<parser::ClosureExpr>();
            std::string param_name = "_";
            if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
                param_name = closure.params[0].first->as<parser::IdentPattern>().name;
            }
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca " + ok_llvm_type);
            emit_line("  store " + ok_llvm_type + " " + ok_val + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, ok_llvm_type, nullptr, std::nullopt};

            std::string merge = fresh_label("map_or_ok_merge");
            std::string ret_alloca = fresh_reg();
            emit_line("  " + ret_alloca + " = alloca " + default_type);
            std::string saved_ca = closure_return_alloca_;
            std::string saved_ct = closure_return_type_;
            std::string saved_cl = closure_return_label_;
            closure_return_alloca_ = ret_alloca;
            closure_return_type_ = default_type;
            closure_return_label_ = merge;

            mapped_val = gen_expr(get_closure_value_expr_outcome(*closure.body));

            closure_return_alloca_ = saved_ca;
            closure_return_type_ = saved_ct;
            closure_return_label_ = saved_cl;

            if (!block_terminated_) {
                emit_line("  store " + default_type + " " + mapped_val + ", ptr " + ret_alloca);
                emit_line("  br label %" + merge);
            }
            emit_line(merge + ":");
            current_block_ = merge;
            block_terminated_ = false;
            mapped_val = fresh_reg();
            emit_line("  " + mapped_val + " = load " + default_type + ", ptr " + ret_alloca);
            locals_.erase(param_name);
        } else {
            auto& ident = call.args[1]->as<parser::IdentExpr>();
            std::string fn_name = "@tml_" + get_suite_prefix() + ident.name;
            mapped_val = fresh_reg();
            emit_line("  " + mapped_val + " = call " + default_type + " " + fn_name + "(" +
                      ok_llvm_type + " " + ok_val + ")");
        }
        std::string ok_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + default_type + " [ " + mapped_val + ", %" +
                  ok_end_block + " ], [ " + default_val + ", %" + is_err_label + " ]");
        last_expr_type_ = default_type;
        return result;
    }

    // and_then(f) -> Outcome[U, E]
    if (method == "and_then") {
        emit_coverage("Outcome::and_then");
        if (call.args.empty()) {
            report_error("and_then requires a function argument", call.span, "C017");
            return receiver;
        }
        bool is_closure = call.args[0]->is<parser::ClosureExpr>();
        bool is_func_ref = call.args[0]->is<parser::IdentExpr>();
        if (!is_closure && !is_func_ref) {
            report_error("and_then requires a closure or function reference", call.span, "C021");
            return receiver;
        }

        std::string is_ok_label = fresh_label("and_then_ok");
        std::string is_err_label = fresh_label("and_then_err");
        std::string end_label = fresh_label("and_then_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + data_ptr);

        std::string closure_result;
        if (is_closure) {
            auto& closure = call.args[0]->as<parser::ClosureExpr>();
            std::string param_name = "_";
            if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
                param_name = closure.params[0].first->as<parser::IdentPattern>().name;
            }
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca " + ok_llvm_type);
            emit_line("  store " + ok_llvm_type + " " + ok_val + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, ok_llvm_type, nullptr, std::nullopt};

            std::string merge = fresh_label("and_then_merge");
            std::string ret_alloca = fresh_reg();
            emit_line("  " + ret_alloca + " = alloca " + enum_type_name);
            std::string saved_ca = closure_return_alloca_;
            std::string saved_ct = closure_return_type_;
            std::string saved_cl = closure_return_label_;
            closure_return_alloca_ = ret_alloca;
            closure_return_type_ = enum_type_name;
            closure_return_label_ = merge;

            closure_result = gen_expr(get_closure_value_expr_outcome(*closure.body));

            closure_return_alloca_ = saved_ca;
            closure_return_type_ = saved_ct;
            closure_return_label_ = saved_cl;

            if (!block_terminated_) {
                emit_line("  store " + enum_type_name + " " + closure_result + ", ptr " +
                          ret_alloca);
                emit_line("  br label %" + merge);
            }
            emit_line(merge + ":");
            current_block_ = merge;
            block_terminated_ = false;
            closure_result = fresh_reg();
            emit_line("  " + closure_result + " = load " + enum_type_name + ", ptr " + ret_alloca);
            locals_.erase(param_name);
        } else {
            auto& ident = call.args[0]->as<parser::IdentExpr>();
            std::string fn_name = "@tml_" + get_suite_prefix() + ident.name;
            // For and_then, the function returns the same Outcome type as the receiver
            closure_result = fresh_reg();
            emit_line("  " + closure_result + " = call " + enum_type_name + " " + fn_name + "(" +
                      ok_llvm_type + " " + ok_val + ")");
        }
        std::string ok_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + closure_result + ", %" +
                  ok_end_block + " ], [ " + receiver + ", %" + is_err_label + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // or_else(f) -> Outcome[T, F]
    if (method == "or_else") {
        emit_coverage("Outcome::or_else");
        if (call.args.empty()) {
            report_error("or_else requires a function argument", call.span, "C017");
            return receiver;
        }
        bool is_closure = call.args[0]->is<parser::ClosureExpr>();
        bool is_func_ref = call.args[0]->is<parser::IdentExpr>();
        if (!is_closure && !is_func_ref) {
            report_error("or_else requires a closure or function reference", call.span, "C021");
            return receiver;
        }

        std::string is_ok_label = fresh_label("or_else_ok");
        std::string is_err_label = fresh_label("or_else_err");
        std::string end_label = fresh_label("or_else_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        emit_line("  br label %" + end_label);

        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string err_val = fresh_reg();
        emit_line("  " + err_val + " = load " + err_llvm_type + ", ptr " + data_ptr);

        std::string closure_result;
        if (is_closure) {
            auto& closure = call.args[0]->as<parser::ClosureExpr>();
            std::string param_name = "_";
            if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
                param_name = closure.params[0].first->as<parser::IdentPattern>().name;
            }
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca " + err_llvm_type);
            emit_line("  store " + err_llvm_type + " " + err_val + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, err_llvm_type, nullptr, std::nullopt};

            std::string merge = fresh_label("or_else_merge");
            std::string ret_alloca = fresh_reg();
            emit_line("  " + ret_alloca + " = alloca " + enum_type_name);
            std::string saved_ca = closure_return_alloca_;
            std::string saved_ct = closure_return_type_;
            std::string saved_cl = closure_return_label_;
            closure_return_alloca_ = ret_alloca;
            closure_return_type_ = enum_type_name;
            closure_return_label_ = merge;

            closure_result = gen_expr(get_closure_value_expr_outcome(*closure.body));

            closure_return_alloca_ = saved_ca;
            closure_return_type_ = saved_ct;
            closure_return_label_ = saved_cl;

            if (!block_terminated_) {
                emit_line("  store " + enum_type_name + " " + closure_result + ", ptr " +
                          ret_alloca);
                emit_line("  br label %" + merge);
            }
            emit_line(merge + ":");
            current_block_ = merge;
            block_terminated_ = false;
            closure_result = fresh_reg();
            emit_line("  " + closure_result + " = load " + enum_type_name + ", ptr " + ret_alloca);
            locals_.erase(param_name);
        } else {
            auto& ident = call.args[0]->as<parser::IdentExpr>();
            std::string fn_name = "@tml_" + get_suite_prefix() + ident.name;
            // For or_else, the function returns the same Outcome type as the receiver
            closure_result = fresh_reg();
            emit_line("  " + closure_result + " = call " + enum_type_name + " " + fn_name + "(" +
                      err_llvm_type + " " + err_val + ")");
        }
        std::string err_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + receiver + ", %" +
                  is_ok_label + " ], [ " + closure_result + ", %" + err_end_block + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // duplicate() -> Outcome[T, E] (copy semantics)
    if (method == "duplicate") {
        emit_coverage("Outcome::duplicate");
        // For value types (primitives), just return the receiver as-is
        // since it's already passed by value
        last_expr_type_ = enum_type_name;
        return receiver;
    }

    // to_string() -> Str / debug_string() -> Str
    if (method == "to_string" || method == "debug_string") {
        emit_coverage("Outcome::" + method);

        std::string is_ok_label = fresh_label("outcome_tostr_ok");
        std::string is_err_label = fresh_label("outcome_tostr_err");
        std::string end_label = fresh_label("outcome_tostr_end");

        std::string is_ok = fresh_reg();
        emit_line("  " + is_ok + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_ok + ", label %" + is_ok_label + ", label %" + is_err_label);

        // Helper lambda to convert a value to string via TML behavior dispatch
        // (Phase 44: replaced hardcoded IR function calls with TML Display/Debug impls)
        auto val_to_string = [&](const std::string& val, const std::string& llvm_type,
                                 const types::TypePtr& val_type) -> std::string {
            std::string str_val;
            if (val_type && val_type->is<types::PrimitiveType>()) {
                auto& prim = val_type->as<types::PrimitiveType>();
                if (prim.kind == types::PrimitiveKind::Str) {
                    if (method == "to_string") {
                        str_val = val; // Str::to_string is identity
                    } else {
                        // Str::debug_string wraps in quotes
                        std::string quote = add_string_literal("\"");
                        std::string tmp = fresh_reg();
                        emit_line("  " + tmp + " = call ptr @str_concat_opt(ptr " + quote +
                                  ", ptr " + val + ")");
                        str_val = fresh_reg();
                        emit_line("  " + str_val + " = call ptr @str_concat_opt(ptr " + tmp +
                                  ", ptr " + quote + ")");
                    }
                } else {
                    // All other primitives: call @tml_<Type>_<method>(<llvm_type> %val)
                    std::string type_name = types::primitive_kind_to_string(prim.kind);
                    std::string fn_name = "@tml_" + type_name + "_" + method;
                    str_val = fresh_reg();
                    emit_line("  " + str_val + " = call ptr " + fn_name + "(" + llvm_type + " " +
                              val + ")");
                }
            } else {
                str_val = add_string_literal("...");
            }
            return str_val;
        };

        // Ok block: "Ok(value)"
        emit_line(is_ok_label + ":");
        current_block_ = is_ok_label;
        std::string ok_alloca = fresh_reg();
        emit_line("  " + ok_alloca + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + ok_alloca);
        std::string ok_data_ptr = fresh_reg();
        emit_line("  " + ok_data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  ok_alloca + ", i32 0, i32 1");
        std::string ok_val = fresh_reg();
        emit_line("  " + ok_val + " = load " + ok_llvm_type + ", ptr " + ok_data_ptr);

        std::string ok_str = val_to_string(ok_val, ok_llvm_type, ok_type);
        std::string ok_prefix = add_string_literal("Ok(");
        std::string ok_suffix = add_string_literal(")");
        std::string ok_with_prefix = fresh_reg();
        emit_line("  " + ok_with_prefix + " = call ptr @str_concat_opt(ptr " + ok_prefix +
                  ", ptr " + ok_str + ")");
        std::string ok_result = fresh_reg();
        emit_line("  " + ok_result + " = call ptr @str_concat_opt(ptr " + ok_with_prefix +
                  ", ptr " + ok_suffix + ")");
        emit_line("  br label %" + end_label);

        // Err block: "Err(error)"
        emit_line(is_err_label + ":");
        current_block_ = is_err_label;
        std::string err_alloca = fresh_reg();
        emit_line("  " + err_alloca + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + err_alloca);
        std::string err_data_ptr = fresh_reg();
        emit_line("  " + err_data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  err_alloca + ", i32 0, i32 1");
        std::string err_val = fresh_reg();
        emit_line("  " + err_val + " = load " + err_llvm_type + ", ptr " + err_data_ptr);

        std::string err_str = val_to_string(err_val, err_llvm_type, err_type);
        std::string err_prefix = add_string_literal("Err(");
        std::string err_with_prefix = fresh_reg();
        emit_line("  " + err_with_prefix + " = call ptr @str_concat_opt(ptr " + err_prefix +
                  ", ptr " + err_str + ")");
        std::string err_result = fresh_reg();
        emit_line("  " + err_result + " = call ptr @str_concat_opt(ptr " + err_with_prefix +
                  ", ptr " + ok_suffix + ")");
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi ptr [ " + ok_result + ", %" + is_ok_label + " ], [ " +
                  err_result + ", %" + is_err_label + " ]");
        last_expr_type_ = "ptr";
        return result;
    }

    // Method not handled
    return std::nullopt;
}

} // namespace tml::codegen
