TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Maybe[T] Methods
//!
//! This file implements methods for the `Maybe[T]` optional type.
//!
//! ## Methods
//!
//! | Method       | Signature               | Description             |
//! |--------------|-------------------------|-------------------------|
//! | `is_just`    | `() -> Bool`            | Check if Just variant   |
//! | `is_nothing` | `() -> Bool`            | Check if Nothing variant|
//! | `unwrap`     | `() -> T`               | Extract value or panic  |
//! | `unwrap_or`  | `(T) -> T`              | Extract or default      |
//! | `map`        | `(func(T)->U) -> Maybe[U]` | Transform if Just    |

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Helper: extract the effective value expression from a closure body.
// Handles cases like `do(x) { return x * 2 }` where the `return` would
// generate a function-level `ret` if inlined directly. Returns the inner
// return-value expression, or the body itself if no return is present.
static const parser::Expr& get_closure_value_expr(const parser::Expr& body) {
    // Case 1: body is a BlockExpr with a single ExprStmt containing a ReturnExpr
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
        // Case 2: body is a BlockExpr with a trailing expression (no stmts)
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
    // Case 3: body is a ReturnExpr directly
    if (body.is<parser::ReturnExpr>()) {
        const auto& ret = body.as<parser::ReturnExpr>();
        if (ret.value.has_value()) {
            return *ret.value.value();
        }
    }
    // Default: return the body as-is
    return body;
}

auto LLVMIRGen::gen_maybe_method(const parser::MethodCallExpr& call, const std::string& receiver,
                                 const std::string& enum_type_name, const std::string& tag_val,
                                 const types::NamedType& named) -> std::optional<std::string> {
    const std::string& method = call.method;

    // is_just() / is_some() -> Bool (tag == 0)
    if (method == "is_just" || method == "is_some") {
        emit_coverage("Maybe::is_just");
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
        last_expr_type_ = "i1";
        return result;
    }

    // is_nothing() / is_none() -> Bool (tag == 1)
    if (method == "is_nothing" || method == "is_none") {
        emit_coverage("Maybe::is_nothing");
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
        last_expr_type_ = "i1";
        return result;
    }

    // Get the inner type for value extraction
    types::TypePtr inner_type;
    if (!named.type_args.empty()) {
        inner_type = named.type_args[0];
    }
    std::string inner_llvm_type = inner_type ? llvm_type_from_semantic(inner_type, true) : "i32";

    // unwrap() -> T (get the value from Just, panics on Nothing)
    if (method == "unwrap" || method == "expect") {
        emit_coverage(method == "expect" ? "Maybe::expect" : "Maybe::unwrap");
        // Extract the data bytes as a pointer
        std::string data_ptr = fresh_reg();
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");

        // Load the value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + data_ptr);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // unwrap_or(default) -> T
    if (method == "unwrap_or") {
        emit_coverage("Maybe::unwrap_or");
        if (call.args.empty()) {
            report_error("unwrap_or() requires a default value", call.span, "C015");
            return "0";
        }

        // Generate the default value
        std::string default_val = gen_expr(*call.args[0]);

        // Extract the data from Maybe if Just
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        // Select: is_just ? just_val : default_val
        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_just + ", " + inner_llvm_type + " " +
                  just_val + ", " + inner_llvm_type + " " + default_val);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // unwrap_or_else(f) -> T
    if (method == "unwrap_or_else") {
        emit_coverage("Maybe::unwrap_or_else");
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("unwrap_or_else requires a closure argument", call.span, "C016");
            return "0";
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_unwrap_or_else_just");
        std::string is_nothing_label = fresh_label("maybe_unwrap_or_else_nothing");
        std::string end_label = fresh_label("maybe_unwrap_or_else_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        // just block: return the value
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);
        emit_line("  br label %" + end_label);

        // nothing block: call closure
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;

        // Set up closure return redirect for unwrap_or_else
        std::string uoe_merge = fresh_label("uoe_merge");
        std::string uoe_alloca = fresh_reg();
        emit_line("  " + uoe_alloca + " = alloca " + inner_llvm_type);
        std::string saved_ca = closure_return_alloca_;
        std::string saved_ct = closure_return_type_;
        std::string saved_cl = closure_return_label_;
        closure_return_alloca_ = uoe_alloca;
        closure_return_type_ = inner_llvm_type;
        closure_return_label_ = uoe_merge;

        std::string closure_result = gen_expr(get_closure_value_expr(*closure.body));

        closure_return_alloca_ = saved_ca;
        closure_return_type_ = saved_ct;
        closure_return_label_ = saved_cl;

        if (!block_terminated_) {
            emit_line("  store " + inner_llvm_type + " " + closure_result + ", ptr " + uoe_alloca);
            emit_line("  br label %" + uoe_merge);
        }
        emit_line(uoe_merge + ":");
        current_block_ = uoe_merge;
        block_terminated_ = false;
        std::string merged_uoe = fresh_reg();
        emit_line("  " + merged_uoe + " = load " + inner_llvm_type + ", ptr " + uoe_alloca);
        std::string nothing_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + inner_llvm_type + " [ " + just_val + ", %" +
                  is_just_label + " ], [ " + merged_uoe + ", %" + nothing_end_block + " ]");
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // unwrap_or_default() -> T
    if (method == "unwrap_or_default") {
        emit_coverage("Maybe::unwrap_or_default");
        // Determine default value based on type
        std::string default_val;
        if (inner_llvm_type == "i8" || inner_llvm_type == "i16" || inner_llvm_type == "i32" ||
            inner_llvm_type == "i64" || inner_llvm_type == "i128") {
            default_val = "0";
        } else if (inner_llvm_type == "float" || inner_llvm_type == "double") {
            default_val = "0.0";
        } else if (inner_llvm_type == "i1") {
            default_val = "false";
        } else {
            default_val = "zeroinitializer";
        }

        // Extract just value
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        // Select: is_just ? just_val : default
        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_just + ", " + inner_llvm_type + " " +
                  just_val + ", " + inner_llvm_type + " " + default_val);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // map(f) -> Maybe[U]
    if (method == "map") {
        emit_coverage("Maybe::map");
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("map requires a closure argument", call.span, "C016");
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_map_just");
        std::string is_nothing_label = fresh_label("maybe_map_nothing");
        std::string end_label = fresh_label("maybe_map_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        // just block: apply closure and wrap in Just
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        std::string param_alloca = fresh_reg();
        emit_line("  " + param_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + param_alloca);
        locals_[param_name] = VarInfo{param_alloca, inner_llvm_type, nullptr, std::nullopt};

        // Set up closure return redirect for `return` inside the map closure.
        // The closure returns a mapped value (same type as inner_llvm_type for T->T map).
        // We use inner_llvm_type as default; if the closure has a declared return type,
        // we could use that, but inner_llvm_type works for the common T->T case.
        std::string map_merge = fresh_label("map_closure_merge");
        std::string map_ret_alloca = fresh_reg();
        emit_line("  " + map_ret_alloca + " = alloca " + inner_llvm_type);
        std::string saved_closure_alloca = closure_return_alloca_;
        std::string saved_closure_type = closure_return_type_;
        std::string saved_closure_label = closure_return_label_;
        closure_return_alloca_ = map_ret_alloca;
        closure_return_type_ = inner_llvm_type;
        closure_return_label_ = map_merge;

        const auto& value_expr = get_closure_value_expr(*closure.body);
        std::string mapped_val = gen_expr(value_expr);
        std::string mapped_type = last_expr_type_;

        // Restore redirect state
        closure_return_alloca_ = saved_closure_alloca;
        closure_return_type_ = saved_closure_type;
        closure_return_label_ = saved_closure_label;

        // If the body produced a value normally (no return), store it
        if (!block_terminated_) {
            emit_line("  store " + mapped_type + " " + mapped_val + ", ptr " + map_ret_alloca);
            emit_line("  br label %" + map_merge);
        }
        emit_line(map_merge + ":");
        current_block_ = map_merge;
        block_terminated_ = false;
        mapped_val = fresh_reg();
        emit_line("  " + mapped_val + " = load " + mapped_type + ", ptr " + map_ret_alloca);
        locals_.erase(param_name);

        // Create Just(mapped_val)
        std::string result_type_name = enum_type_name;
        if (mapped_type != inner_llvm_type) {
            types::TypePtr mapped_semantic_type = semantic_type_from_llvm(mapped_type);
            std::vector<types::TypePtr> new_type_args = {mapped_semantic_type};
            std::string new_mangled = require_enum_instantiation("Maybe", new_type_args);
            result_type_name = "%struct." + new_mangled;
        }

        std::string just_alloca = fresh_reg();
        emit_line("  " + just_alloca + " = alloca " + result_type_name);
        std::string just_tag_ptr = fresh_reg();
        emit_line("  " + just_tag_ptr + " = getelementptr inbounds " + result_type_name + ", ptr " +
                  just_alloca + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + just_tag_ptr);
        std::string just_data_ptr = fresh_reg();
        emit_line("  " + just_data_ptr + " = getelementptr inbounds " + result_type_name +
                  ", ptr " + just_alloca + ", i32 0, i32 1");
        emit_line("  store " + mapped_type + " " + mapped_val + ", ptr " + just_data_ptr);
        std::string just_result = fresh_reg();
        emit_line("  " + just_result + " = load " + result_type_name + ", ptr " + just_alloca);
        std::string just_end_block = current_block_;
        emit_line("  br label %" + end_label);

        // nothing block: return Nothing
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + result_type_name);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + result_type_name +
                  ", ptr " + nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_result = fresh_reg();
        emit_line("  " + nothing_result + " = load " + result_type_name + ", ptr " +
                  nothing_alloca);
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + result_type_name + " [ " + just_result + ", %" +
                  just_end_block + " ], [ " + nothing_result + ", %" + is_nothing_label + " ]");
        last_expr_type_ = result_type_name;
        return result;
    }

    // and_then(f) -> Maybe[U]
    if (method == "and_then") {
        emit_coverage("Maybe::and_then");
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("and_then requires a closure argument", call.span, "C016");
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_and_then_just");
        std::string is_nothing_label = fresh_label("maybe_and_then_nothing");
        std::string end_label = fresh_label("maybe_and_then_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        // just block: call closure (which returns a Maybe)
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        std::string param_alloca = fresh_reg();
        emit_line("  " + param_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + param_alloca);
        locals_[param_name] = VarInfo{param_alloca, inner_llvm_type, nullptr, std::nullopt};

        // Set up closure return redirect so `return` inside the closure body
        // stores the value and branches instead of emitting function-level `ret`
        std::string closure_merge = fresh_label("closure_merge");
        std::string ret_alloca = fresh_reg();
        emit_line("  " + ret_alloca + " = alloca " + enum_type_name);
        std::string saved_closure_alloca = closure_return_alloca_;
        std::string saved_closure_type = closure_return_type_;
        std::string saved_closure_label = closure_return_label_;
        closure_return_alloca_ = ret_alloca;
        closure_return_type_ = enum_type_name;
        closure_return_label_ = closure_merge;

        std::string closure_result = gen_expr(get_closure_value_expr(*closure.body));
        std::string closure_type = last_expr_type_;

        // Restore redirect state
        closure_return_alloca_ = saved_closure_alloca;
        closure_return_type_ = saved_closure_type;
        closure_return_label_ = saved_closure_label;

        // If the body produced a value normally (no return), store it
        if (!block_terminated_) {
            emit_line("  store " + enum_type_name + " " + closure_result + ", ptr " + ret_alloca);
            emit_line("  br label %" + closure_merge);
        }
        emit_line(closure_merge + ":");
        current_block_ = closure_merge;
        block_terminated_ = false;
        std::string merged_result = fresh_reg();
        emit_line("  " + merged_result + " = load " + enum_type_name + ", ptr " + ret_alloca);
        std::string just_end_block = current_block_;
        locals_.erase(param_name);
        emit_line("  br label %" + end_label);

        // nothing block: propagate Nothing
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + merged_result + ", %" +
                  just_end_block + " ], [ " + receiver + ", %" + is_nothing_label + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // or_else(f) -> Maybe[T]
    if (method == "or_else") {
        emit_coverage("Maybe::or_else");
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("or_else requires a closure argument", call.span, "C016");
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_or_else_just");
        std::string is_nothing_label = fresh_label("maybe_or_else_nothing");
        std::string end_label = fresh_label("maybe_or_else_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        // just block: return self
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        emit_line("  br label %" + end_label);

        // nothing block: call closure (which returns a Maybe)
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;

        // Set up closure return redirect for or_else
        std::string or_merge = fresh_label("or_else_merge");
        std::string or_ret_alloca = fresh_reg();
        emit_line("  " + or_ret_alloca + " = alloca " + enum_type_name);
        std::string saved_ca = closure_return_alloca_;
        std::string saved_ct = closure_return_type_;
        std::string saved_cl = closure_return_label_;
        closure_return_alloca_ = or_ret_alloca;
        closure_return_type_ = enum_type_name;
        closure_return_label_ = or_merge;

        std::string closure_result = gen_expr(get_closure_value_expr(*closure.body));

        closure_return_alloca_ = saved_ca;
        closure_return_type_ = saved_ct;
        closure_return_label_ = saved_cl;

        if (!block_terminated_) {
            emit_line("  store " + enum_type_name + " " + closure_result + ", ptr " +
                      or_ret_alloca);
            emit_line("  br label %" + or_merge);
        }
        emit_line(or_merge + ":");
        current_block_ = or_merge;
        block_terminated_ = false;
        std::string merged_or = fresh_reg();
        emit_line("  " + merged_or + " = load " + enum_type_name + ", ptr " + or_ret_alloca);
        std::string nothing_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + receiver + ", %" +
                  is_just_label + " ], [ " + merged_or + ", %" + nothing_end_block + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // contains(value) -> Bool
    if (method == "contains") {
        emit_coverage("Maybe::contains");
        if (call.args.empty()) {
            report_error("contains requires an argument", call.span, "C015");
            return "false";
        }
        std::string cmp_val = gen_expr(*call.args[0]);

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");

        std::string is_just_label = fresh_label("maybe_contains_just");
        std::string is_nothing_label = fresh_label("maybe_contains_nothing");
        std::string end_label = fresh_label("maybe_contains_end");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string values_eq = fresh_reg();
        if (inner_llvm_type == "ptr") {
            // str_eq returns i32, convert to i1
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + just_val + ", ptr " + cmp_val +
                      ")");
            emit_line("  " + values_eq + " = icmp ne i32 " + eq_i32 + ", 0");
        } else {
            emit_line("  " + values_eq + " = icmp eq " + inner_llvm_type + " " + just_val + ", " +
                      cmp_val);
        }
        emit_line("  br label %" + end_label);

        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi i1 [ " + values_eq + ", %" + is_just_label +
                  " ], [ false, %" + is_nothing_label + " ]");
        last_expr_type_ = "i1";
        return result;
    }

    // filter(predicate) -> Maybe[T]
    if (method == "filter") {
        emit_coverage("Maybe::filter");
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("filter requires a closure argument", call.span, "C016");
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_filter_just");
        std::string is_nothing_label = fresh_label("maybe_filter_nothing");
        std::string keep_label = fresh_label("maybe_filter_keep");
        std::string discard_label = fresh_label("maybe_filter_discard");
        std::string end_label = fresh_label("maybe_filter_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        // just block: test predicate
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        // filter's closure takes `ref T`. For primitive types (i32, i64, etc.),
        // bind the value directly so comparisons like `x >= 5` work without
        // an extra dereference. For struct/ptr types, bind as a pointer.
        bool is_primitive =
            (inner_llvm_type == "i8" || inner_llvm_type == "i16" || inner_llvm_type == "i32" ||
             inner_llvm_type == "i64" || inner_llvm_type == "i128" || inner_llvm_type == "i1" ||
             inner_llvm_type == "float" || inner_llvm_type == "double");
        if (is_primitive) {
            std::string val_alloca = fresh_reg();
            emit_line("  " + val_alloca + " = alloca " + inner_llvm_type);
            emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + val_alloca);
            locals_[param_name] = VarInfo{val_alloca, inner_llvm_type, nullptr, std::nullopt};
        } else {
            // For struct types, create ref T indirection
            std::string val_alloca = fresh_reg();
            emit_line("  " + val_alloca + " = alloca " + inner_llvm_type);
            emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + val_alloca);
            std::string param_alloca = fresh_reg();
            emit_line("  " + param_alloca + " = alloca ptr");
            emit_line("  store ptr " + val_alloca + ", ptr " + param_alloca);
            locals_[param_name] = VarInfo{param_alloca, "ptr", nullptr, std::nullopt};
        }

        // Set up closure return redirect for filter (returns Bool = i1)
        std::string flt_merge = fresh_label("filter_merge");
        std::string flt_alloca = fresh_reg();
        emit_line("  " + flt_alloca + " = alloca i1");
        std::string saved_ca = closure_return_alloca_;
        std::string saved_ct = closure_return_type_;
        std::string saved_cl = closure_return_label_;
        closure_return_alloca_ = flt_alloca;
        closure_return_type_ = "i1";
        closure_return_label_ = flt_merge;

        std::string pred_result = gen_expr(get_closure_value_expr(*closure.body));

        closure_return_alloca_ = saved_ca;
        closure_return_type_ = saved_ct;
        closure_return_label_ = saved_cl;

        if (!block_terminated_) {
            emit_line("  store i1 " + pred_result + ", ptr " + flt_alloca);
            emit_line("  br label %" + flt_merge);
        }
        emit_line(flt_merge + ":");
        current_block_ = flt_merge;
        block_terminated_ = false;
        pred_result = fresh_reg();
        emit_line("  " + pred_result + " = load i1, ptr " + flt_alloca);
        std::string just_end_block = current_block_;
        locals_.erase(param_name);
        emit_line("  br i1 " + pred_result + ", label %" + keep_label + ", label %" +
                  discard_label);

        // keep block: return Just
        emit_line(keep_label + ":");
        current_block_ = keep_label;
        emit_line("  br label %" + end_label);

        // discard block: return Nothing
        emit_line(discard_label + ":");
        current_block_ = discard_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + enum_type_name);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + enum_type_name +
                  ", ptr " + nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_result = fresh_reg();
        emit_line("  " + nothing_result + " = load " + enum_type_name + ", ptr " + nothing_alloca);
        emit_line("  br label %" + end_label);

        // nothing block: return Nothing (original)
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + receiver + ", %" +
                  keep_label + " ], [ " + nothing_result + ", %" + discard_label + " ], [ " +
                  receiver + ", %" + is_nothing_label + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // alt(other) -> Maybe[T]
    if (method == "alt") {
        emit_coverage("Maybe::alt");
        if (call.args.empty()) {
            report_error("alt requires an argument", call.span, "C015");
            return receiver;
        }

        std::string other = gen_expr(*call.args[0]);

        // if is_just, return self, else return other
        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_just + ", " + enum_type_name + " " +
                  receiver + ", " + enum_type_name + " " + other);
        last_expr_type_ = enum_type_name;
        return result;
    }

    // one_of(other) -> Maybe[T] (renamed from xor because xor is a keyword)
    if (method == "one_of") {
        if (call.args.empty()) {
            report_error("one_of requires an argument", call.span, "C015");
            return receiver;
        }

        emit_coverage("Maybe::one_of");

        std::string other = gen_expr(*call.args[0]);
        std::string other_type = last_expr_type_;

        // Get tag from other
        std::string other_alloca = fresh_reg();
        emit_line("  " + other_alloca + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + other + ", ptr " + other_alloca);
        std::string other_tag_ptr = fresh_reg();
        emit_line("  " + other_tag_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  other_alloca + ", i32 0, i32 0");
        std::string other_tag = fresh_reg();
        emit_line("  " + other_tag + " = load i32, ptr " + other_tag_ptr);

        // Check conditions
        std::string self_is_just = fresh_reg();
        emit_line("  " + self_is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string other_is_just = fresh_reg();
        emit_line("  " + other_is_just + " = icmp eq i32 " + other_tag + ", 0");

        // xor: exactly one is Just
        // self_just && !other_just -> return self
        // !self_just && other_just -> return other
        // otherwise -> return Nothing

        std::string self_only_label = fresh_label("xor_self_only");
        std::string check_other_label = fresh_label("xor_check_other");
        std::string other_only_label = fresh_label("xor_other_only");
        std::string nothing_label = fresh_label("xor_nothing");
        std::string end_label = fresh_label("xor_end");

        emit_line("  br i1 " + self_is_just + ", label %" + check_other_label + ", label %" +
                  other_only_label);

        // self is just, check if other is nothing
        emit_line(check_other_label + ":");
        current_block_ = check_other_label;
        std::string other_is_nothing = fresh_reg();
        emit_line("  " + other_is_nothing + " = icmp eq i32 " + other_tag + ", 1");
        emit_line("  br i1 " + other_is_nothing + ", label %" + self_only_label + ", label %" +
                  nothing_label);

        // return self
        emit_line(self_only_label + ":");
        current_block_ = self_only_label;
        emit_line("  br label %" + end_label);

        // self is nothing, check if other is just
        emit_line(other_only_label + ":");
        current_block_ = other_only_label;
        emit_line("  br i1 " + other_is_just + ", label %" + end_label + ", label %" +
                  nothing_label);

        // both are just or both are nothing -> return nothing
        emit_line(nothing_label + ":");
        current_block_ = nothing_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + enum_type_name);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + enum_type_name +
                  ", ptr " + nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_result = fresh_reg();
        emit_line("  " + nothing_result + " = load " + enum_type_name + ", ptr " + nothing_alloca);
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + receiver + ", %" +
                  self_only_label + " ], [ " + other + ", %" + other_only_label + " ], [ " +
                  nothing_result + ", %" + nothing_label + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // duplicate() -> Maybe[T] (copy semantics)
    if (method == "duplicate") {
        emit_coverage("Maybe::duplicate");
        // For value types (primitives), just return the receiver as-is
        // since it's already passed by value
        last_expr_type_ = enum_type_name;
        return receiver;
    }

    // map_or(default, f) -> U
    if (method == "map_or") {
        emit_coverage("Maybe::map_or");
        if (call.args.size() < 2) {
            report_error("map_or requires a default value and a closure", call.span, "C018");
            return "0";
        }

        // Generate default value first
        std::string default_val = gen_expr(*call.args[0]);
        std::string default_type = last_expr_type_;

        if (!call.args[1]->is<parser::ClosureExpr>()) {
            report_error("map_or requires a closure as second argument", call.span, "C020");
            return default_val;
        }
        auto& closure = call.args[1]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_map_or_just");
        std::string is_nothing_label = fresh_label("maybe_map_or_nothing");
        std::string end_label = fresh_label("maybe_map_or_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        // just block: apply closure
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        std::string param_alloca = fresh_reg();
        emit_line("  " + param_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + param_alloca);
        locals_[param_name] = VarInfo{param_alloca, inner_llvm_type, nullptr, std::nullopt};

        // Set up closure return redirect for map_or
        std::string mo_merge = fresh_label("map_or_merge");
        std::string mo_alloca = fresh_reg();
        emit_line("  " + mo_alloca + " = alloca " + default_type);
        std::string saved_ca2 = closure_return_alloca_;
        std::string saved_ct2 = closure_return_type_;
        std::string saved_cl2 = closure_return_label_;
        closure_return_alloca_ = mo_alloca;
        closure_return_type_ = default_type;
        closure_return_label_ = mo_merge;

        const auto& value_expr2 = get_closure_value_expr(*closure.body);
        std::string mapped_val = gen_expr(value_expr2);

        closure_return_alloca_ = saved_ca2;
        closure_return_type_ = saved_ct2;
        closure_return_label_ = saved_cl2;

        if (!block_terminated_) {
            emit_line("  store " + default_type + " " + mapped_val + ", ptr " + mo_alloca);
            emit_line("  br label %" + mo_merge);
        }
        emit_line(mo_merge + ":");
        current_block_ = mo_merge;
        block_terminated_ = false;
        mapped_val = fresh_reg();
        emit_line("  " + mapped_val + " = load " + default_type + ", ptr " + mo_alloca);
        std::string just_end_block = current_block_;
        locals_.erase(param_name);
        emit_line("  br label %" + end_label);

        // nothing block: return default
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + default_type + " [ " + mapped_val + ", %" +
                  just_end_block + " ], [ " + default_val + ", %" + is_nothing_label + " ]");
        last_expr_type_ = default_type;
        return result;
    }

    // to_string() -> Str / debug_string() -> Str
    if (method == "to_string" || method == "debug_string") {
        emit_coverage("Maybe::" + method);

        std::string is_just_label = fresh_label("maybe_tostr_just");
        std::string is_nothing_label = fresh_label("maybe_tostr_nothing");
        std::string end_label = fresh_label("maybe_tostr_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" +
                  is_nothing_label);

        // Just block: extract value, call to_string on it, wrap with "Just(...)"
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " +
                  alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        // Call to_string/debug_string on inner value via TML behavior dispatch
        // (Phase 44: replaced hardcoded IR function calls with TML Display/Debug impls)
        std::string inner_str;
        if (inner_type && inner_type->is<types::PrimitiveType>()) {
            auto& prim = inner_type->as<types::PrimitiveType>();
            if (prim.kind == types::PrimitiveKind::Str) {
                if (method == "to_string") {
                    // Str::to_string is identity
                    inner_str = just_val;
                } else {
                    // Str::debug_string wraps in quotes
                    std::string quote = add_string_literal("\"");
                    std::string tmp = fresh_reg();
                    emit_line("  " + tmp + " = call ptr @str_concat_opt(ptr " + quote + ", ptr " +
                              just_val + ")");
                    inner_str = fresh_reg();
                    emit_line("  " + inner_str + " = call ptr @str_concat_opt(ptr " + tmp +
                              ", ptr " + quote + ")");
                }
            } else {
                // All other primitives: call @tml_<Type>_<method>(<llvm_type> %val)
                std::string type_name = types::primitive_kind_to_string(prim.kind);
                std::string fn_name = "@tml_" + type_name + "_" + method;
                inner_str = fresh_reg();
                emit_line("  " + inner_str + " = call ptr " + fn_name + "(" + inner_llvm_type +
                          " " + just_val + ")");
            }
        } else {
            // Non-primitive inner type: use generic representation as fallback
            std::string fallback = add_string_literal("...");
            inner_str = fallback;
        }

        // Concatenate "Just(" + inner_str + ")"
        std::string prefix_str = add_string_literal("Just(");
        std::string suffix_str = add_string_literal(")");
        std::string with_prefix = fresh_reg();
        emit_line("  " + with_prefix + " = call ptr @str_concat_opt(ptr " + prefix_str + ", ptr " +
                  inner_str + ")");
        std::string just_result = fresh_reg();
        emit_line("  " + just_result + " = call ptr @str_concat_opt(ptr " + with_prefix + ", ptr " +
                  suffix_str + ")");
        emit_line("  br label %" + end_label);

        // Nothing block: return "Nothing"
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        std::string nothing_str = add_string_literal("Nothing");
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi ptr [ " + just_result + ", %" + is_just_label + " ], [ " +
                  nothing_str + ", %" + is_nothing_label + " ]");
        last_expr_type_ = "ptr";
        return result;
    }

    // Method not handled
    return std::nullopt;
}

} // namespace tml::codegen
