// LLVM IR generator - Control flow
// Handles: if, block, loop, while, for, return

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// Forward declaration for is_bool_expr (defined in llvm_ir_gen_stmt.cpp)
bool is_bool_expr(const parser::Expr& expr, const std::unordered_map<std::string, LLVMIRGen::VarInfo>& locals);

auto LLVMIRGen::gen_if(const parser::IfExpr& if_expr) -> std::string {
    std::string cond = gen_expr(*if_expr.condition);

    // If condition is not already i1 (bool), convert it
    // This handles user-defined functions that return Bool but are generated as i32
    if (!is_bool_expr(*if_expr.condition, locals_)) {
        // Condition might be i32, convert to i1 by comparing with 0
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne i32 " + cond + ", 0");
        cond = bool_cond;
    }

    std::string label_then = fresh_label("if.then");
    std::string label_else = fresh_label("if.else");
    std::string label_end = fresh_label("if.end");

    // Branch
    if (if_expr.else_branch.has_value()) {
        emit_line("  br i1 " + cond + ", label %" + label_then + ", label %" + label_else);
    } else {
        emit_line("  br i1 " + cond + ", label %" + label_then + ", label %" + label_end);
    }

    // Then block
    emit_line(label_then + ":");
    block_terminated_ = false;
    std::string then_val = gen_expr(*if_expr.then_branch);
    if (!block_terminated_) {
        emit_line("  br label %" + label_end);
    }

    // Else block
    if (if_expr.else_branch.has_value()) {
        emit_line(label_else + ":");
        block_terminated_ = false;
        std::string else_val = gen_expr(*if_expr.else_branch.value());
        if (!block_terminated_) {
            emit_line("  br label %" + label_end);
        }
    }

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    return "0";  // TODO: phi node for if-expression value
}

auto LLVMIRGen::gen_block(const parser::BlockExpr& block) -> std::string {
    std::string result = "0";

    for (const auto& stmt : block.stmts) {
        gen_stmt(*stmt);
    }

    if (block.expr.has_value()) {
        result = gen_expr(*block.expr.value());
    }

    return result;
}

auto LLVMIRGen::gen_loop(const parser::LoopExpr& loop) -> std::string {
    std::string label_start = fresh_label("loop.start");
    std::string label_body = fresh_label("loop.body");
    std::string label_end = fresh_label("loop.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    current_loop_start_ = label_start;
    current_loop_end_ = label_end;

    emit_line("  br label %" + label_start);
    emit_line(label_start + ":");
    block_terminated_ = false;

    gen_expr(*loop.body);

    if (!block_terminated_) {
        emit_line("  br label %" + label_start);
    }

    emit_line(label_end + ":");
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;

    return "0";
}

auto LLVMIRGen::gen_while(const parser::WhileExpr& while_expr) -> std::string {
    std::string label_cond = fresh_label("while.cond");
    std::string label_body = fresh_label("while.body");
    std::string label_end = fresh_label("while.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    current_loop_start_ = label_cond;
    current_loop_end_ = label_end;

    // Jump to condition
    emit_line("  br label %" + label_cond);

    // Condition block
    emit_line(label_cond + ":");
    block_terminated_ = false;
    std::string cond = gen_expr(*while_expr.condition);

    // If condition is not already i1 (bool), convert it
    if (!is_bool_expr(*while_expr.condition, locals_)) {
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne i32 " + cond + ", 0");
        cond = bool_cond;
    }

    emit_line("  br i1 " + cond + ", label %" + label_body + ", label %" + label_end);

    // Body block
    emit_line(label_body + ":");
    block_terminated_ = false;
    gen_expr(*while_expr.body);
    if (!block_terminated_) {
        emit_line("  br label %" + label_cond);
    }

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;

    return "0";
}

auto LLVMIRGen::gen_for(const parser::ForExpr& for_expr) -> std::string {
    // For loops: for pattern in iter { body }
    // We support range expressions: for i in 0 to 10 { ... }
    std::string label_init = fresh_label("for.init");
    std::string label_cond = fresh_label("for.cond");
    std::string label_body = fresh_label("for.body");
    std::string label_incr = fresh_label("for.incr");
    std::string label_end = fresh_label("for.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    current_loop_start_ = label_incr;  // continue goes to increment
    current_loop_end_ = label_end;

    // Get loop variable name from pattern
    std::string var_name = "_for_idx";
    if (for_expr.pattern->is<parser::IdentPattern>()) {
        var_name = for_expr.pattern->as<parser::IdentPattern>().name;
    }

    // Check if iter is a range expression
    std::string range_start = "0";
    std::string range_end = "0";
    bool inclusive = false;

    std::string range_type = "i32";  // Default type for range

    if (for_expr.iter->is<parser::RangeExpr>()) {
        const auto& range = for_expr.iter->as<parser::RangeExpr>();
        inclusive = range.inclusive;
        if (range.start.has_value()) {
            range_start = gen_expr(*range.start.value());
        }
        if (range.end.has_value()) {
            range_end = gen_expr(*range.end.value());
            range_type = last_expr_type_;  // Use type of end value
        }
    } else {
        // Fallback: treat as simple range 0 to iter
        range_end = gen_expr(*for_expr.iter);
        range_type = last_expr_type_;
    }

    // Allocate loop variable with correct type
    std::string var_alloca = fresh_reg();
    emit_line("  " + var_alloca + " = alloca " + range_type);
    emit_line("  store " + range_type + " " + range_start + ", ptr " + var_alloca);
    locals_[var_name] = VarInfo{var_alloca, range_type};

    // Jump to condition
    emit_line("  br label %" + label_cond);

    // Condition block
    emit_line(label_cond + ":");
    block_terminated_ = false;
    std::string current = fresh_reg();
    emit_line("  " + current + " = load " + range_type + ", ptr " + var_alloca);
    std::string cmp_result = fresh_reg();
    if (inclusive) {
        emit_line("  " + cmp_result + " = icmp sle " + range_type + " " + current + ", " + range_end);
    } else {
        emit_line("  " + cmp_result + " = icmp slt " + range_type + " " + current + ", " + range_end);
    }
    emit_line("  br i1 " + cmp_result + ", label %" + label_body + ", label %" + label_end);

    // Body block
    emit_line(label_body + ":");
    block_terminated_ = false;
    gen_expr(*for_expr.body);
    if (!block_terminated_) {
        emit_line("  br label %" + label_incr);
    }

    // Increment block
    emit_line(label_incr + ":");
    block_terminated_ = false;
    std::string next_val = fresh_reg();
    std::string current2 = fresh_reg();
    emit_line("  " + current2 + " = load " + range_type + ", ptr " + var_alloca);
    emit_line("  " + next_val + " = add nsw " + range_type + " " + current2 + ", 1");
    emit_line("  store " + range_type + " " + next_val + ", ptr " + var_alloca);
    emit_line("  br label %" + label_cond);

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;

    return "0";
}

auto LLVMIRGen::gen_return(const parser::ReturnExpr& ret) -> std::string {
    if (ret.value.has_value()) {
        std::string val = gen_expr(*ret.value.value());
        emit_line("  ret " + current_ret_type_ + " " + val);
    } else {
        emit_line("  ret void");
    }
    block_terminated_ = true;
    return "void";
}

} // namespace tml::codegen
