//! # LLVM IR Generator - Expression Dispatcher
//!
//! This file implements the main expression code generation dispatcher.
//!
//! ## Expression Routing
//!
//! `gen_expr()` routes each expression kind to its specialized handler:
//!
//! | Expression Type  | Handler           | Location              |
//! |------------------|-------------------|-----------------------|
//! | Literal          | `gen_literal`     | This file             |
//! | Identifier       | `gen_ident`       | This file             |
//! | Binary           | `gen_binary`      | expr/binary.cpp       |
//! | Unary            | `gen_unary`       | expr/unary.cpp        |
//! | Call             | `gen_call`        | This file             |
//! | Method call      | `gen_method_call` | expr/method.cpp       |
//! | If/Ternary       | `gen_if`          | llvm_ir_gen_control.cpp|
//! | Loop/For/While   | `gen_loop`        | llvm_ir_gen_control.cpp|
//! | Struct           | `gen_struct_expr` | expr/struct.cpp       |
//! | Array/Index      | `gen_array`       | expr/collections.cpp  |
//! | Closure          | `gen_closure`     | expr/closure.cpp      |
//!
//! ## Literal Generation
//!
//! `gen_literal()` handles integer, float, bool, char, and string literals.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_expr(const parser::Expr& expr) -> std::string {
    std::string result;
    if (expr.is<parser::LiteralExpr>()) {
        return gen_literal(expr.as<parser::LiteralExpr>());
    } else if (expr.is<parser::IdentExpr>()) {
        return gen_ident(expr.as<parser::IdentExpr>());
    } else if (expr.is<parser::BinaryExpr>()) {
        result = gen_binary(expr.as<parser::BinaryExpr>());
    } else if (expr.is<parser::UnaryExpr>()) {
        return gen_unary(expr.as<parser::UnaryExpr>());
    } else if (expr.is<parser::CallExpr>()) {
        result = gen_call(expr.as<parser::CallExpr>());
    } else if (expr.is<parser::IfExpr>()) {
        return gen_if(expr.as<parser::IfExpr>());
    } else if (expr.is<parser::TernaryExpr>()) {
        return gen_ternary(expr.as<parser::TernaryExpr>());
    } else if (expr.is<parser::IfLetExpr>()) {
        return gen_if_let(expr.as<parser::IfLetExpr>());
    } else if (expr.is<parser::BlockExpr>()) {
        return gen_block(expr.as<parser::BlockExpr>());
    } else if (expr.is<parser::LoopExpr>()) {
        return gen_loop(expr.as<parser::LoopExpr>());
    } else if (expr.is<parser::WhileExpr>()) {
        return gen_while(expr.as<parser::WhileExpr>());
    } else if (expr.is<parser::ForExpr>()) {
        return gen_for(expr.as<parser::ForExpr>());
    } else if (expr.is<parser::ReturnExpr>()) {
        return gen_return(expr.as<parser::ReturnExpr>());
    } else if (expr.is<parser::ThrowExpr>()) {
        return gen_throw(expr.as<parser::ThrowExpr>());
    } else if (expr.is<parser::WhenExpr>()) {
        return gen_when(expr.as<parser::WhenExpr>());
    } else if (expr.is<parser::StructExpr>()) {
        return gen_struct_expr(expr.as<parser::StructExpr>());
    } else if (expr.is<parser::FieldExpr>()) {
        return gen_field(expr.as<parser::FieldExpr>());
    } else if (expr.is<parser::BreakExpr>()) {
        // Break jumps to end of current loop
        if (!current_loop_end_.empty()) {
            // Emit lifetime.end for allocas in current scope
            emit_scope_lifetime_ends();
            // Restore stack before exiting loop to reclaim allocas
            if (!current_loop_stack_save_.empty()) {
                emit_line("  call void @llvm.stackrestore(ptr " + current_loop_stack_save_ + ")");
            }
            emit_line("  br label %" + current_loop_end_);
            block_terminated_ = true;
        }
        return "void";
    } else if (expr.is<parser::ContinueExpr>()) {
        // Continue jumps to start of current loop
        if (!current_loop_start_.empty()) {
            // Emit lifetime.end for allocas in current scope
            emit_scope_lifetime_ends();
            // Restore stack before continuing to reclaim allocas from this iteration
            if (!current_loop_stack_save_.empty()) {
                emit_line("  call void @llvm.stackrestore(ptr " + current_loop_stack_save_ + ")");
            }
            emit_line("  br label %" + current_loop_start_);
            block_terminated_ = true;
        }
        return "void";
    } else if (expr.is<parser::ArrayExpr>()) {
        return gen_array(expr.as<parser::ArrayExpr>());
    } else if (expr.is<parser::IndexExpr>()) {
        return gen_index(expr.as<parser::IndexExpr>());
    } else if (expr.is<parser::PathExpr>()) {
        return gen_path(expr.as<parser::PathExpr>());
    } else if (expr.is<parser::MethodCallExpr>()) {
        result = gen_method_call(expr.as<parser::MethodCallExpr>());
    } else if (expr.is<parser::ClosureExpr>()) {
        return gen_closure(expr.as<parser::ClosureExpr>());
    } else if (expr.is<parser::LowlevelExpr>()) {
        return gen_lowlevel(expr.as<parser::LowlevelExpr>());
    } else if (expr.is<parser::InterpolatedStringExpr>()) {
        result = gen_interp_string(expr.as<parser::InterpolatedStringExpr>());
    } else if (expr.is<parser::TemplateLiteralExpr>()) {
        result = gen_template_literal(expr.as<parser::TemplateLiteralExpr>());
    } else if (expr.is<parser::CastExpr>()) {
        return gen_cast(expr.as<parser::CastExpr>());
    } else if (expr.is<parser::IsExpr>()) {
        return gen_is_check(expr.as<parser::IsExpr>());
    } else if (expr.is<parser::TupleExpr>()) {
        return gen_tuple(expr.as<parser::TupleExpr>());
    } else if (expr.is<parser::AwaitExpr>()) {
        return gen_await(expr.as<parser::AwaitExpr>());
    } else if (expr.is<parser::TryExpr>()) {
        return gen_try(expr.as<parser::TryExpr>());
    } else if (expr.is<parser::BaseExpr>()) {
        return gen_base_expr(expr.as<parser::BaseExpr>());
    } else if (expr.is<parser::NewExpr>()) {
        return gen_new_expr(expr.as<parser::NewExpr>());
    } else {
        report_error("Unsupported expression type", expr.span, "C002");
        return "0";
    }

    // NOTE: Phase 4b (automatic Str temp tracking) was attempted here but disabled.
    // Tracking Str temps from call/method/binary/interpolated expressions caused
    // heap corruption in complex patterns (glob, fmt) because:
    // 1. Assignment `var = expr` — the temp would be freed at statement end even though
    //    the variable now owns the value
    // 2. Nested function calls — inner calls produce temps that outer calls consume
    // 3. Library code patterns where Str pointers are stored in collections
    // The safe approach is Phase 4a (let/var binding drops) + Phase 3 (discarded returns).
    // Phase 4b requires a more sophisticated ownership analysis to be safe.
    return result;
}

} // namespace tml::codegen
