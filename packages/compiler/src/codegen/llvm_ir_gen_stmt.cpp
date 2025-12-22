// LLVM IR generator - Statement generation
// Handles: let statements, expression statements

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

void LLVMIRGen::gen_stmt(const parser::Stmt& stmt) {
    if (stmt.is<parser::LetStmt>()) {
        gen_let_stmt(stmt.as<parser::LetStmt>());
    } else if (stmt.is<parser::ExprStmt>()) {
        gen_expr_stmt(stmt.as<parser::ExprStmt>());
    }
}

// Helper to check if an expression is boolean-typed (without variable lookup)
static bool is_bool_expr_static(const parser::Expr& expr) {
    if (expr.is<parser::LiteralExpr>()) {
        return expr.as<parser::LiteralExpr>().token.kind == lexer::TokenKind::BoolLiteral;
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        switch (bin.op) {
            case parser::BinaryOp::Eq:
            case parser::BinaryOp::Ne:
            case parser::BinaryOp::Lt:
            case parser::BinaryOp::Gt:
            case parser::BinaryOp::Le:
            case parser::BinaryOp::Ge:
            case parser::BinaryOp::And:
            case parser::BinaryOp::Or:
                return true;
            default:
                return false;
        }
    }
    if (expr.is<parser::UnaryExpr>()) {
        return expr.as<parser::UnaryExpr>().op == parser::UnaryOp::Not;
    }
    // Check for functions that return bool
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& name = call.callee->as<parser::IdentExpr>().name;
            // Atomic/spinlock functions
            if (name == "atomic_cas" || name == "spin_trylock") {
                return true;
            }
            // Channel functions that return bool
            if (name == "channel_send" || name == "channel_try_send" || name == "channel_try_recv") {
                return true;
            }
            // Mutex functions that return bool
            if (name == "mutex_try_lock") {
                return true;
            }
            // Collection functions that return bool
            if (name == "hashmap_has" || name == "hashmap_remove" || name == "list_is_empty" || name == "str_eq") {
                return true;
            }
        }
    }
    // Check for method calls that return bool
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& call = expr.as<parser::MethodCallExpr>();
        const auto& method = call.method;
        if (method == "is_empty" || method == "isEmpty" ||
            method == "has" || method == "contains" ||
            method == "remove") {
            return true;
        }
    }
    return false;
}

// Helper to check if an expression is boolean-typed (with variable lookup)
bool is_bool_expr(const parser::Expr& expr, const std::unordered_map<std::string, LLVMIRGen::VarInfo>& locals) {
    // Check for bool-typed variable
    if (expr.is<parser::IdentExpr>()) {
        const auto& ident = expr.as<parser::IdentExpr>().name;
        auto it = locals.find(ident);
        if (it != locals.end() && it->second.type == "i1") {
            return true;
        }
    }
    return is_bool_expr_static(expr);
}

// Helper to check if expression is a reference (pointer) expression
static bool is_ref_expr(const parser::Expr& expr) {
    if (expr.is<parser::UnaryExpr>()) {
        const auto& un = expr.as<parser::UnaryExpr>();
        return un.op == parser::UnaryOp::Ref || un.op == parser::UnaryOp::RefMut;
    }
    // Array literals return a list pointer
    if (expr.is<parser::ArrayExpr>()) {
        return true;
    }
    // Check for functions that return pointers
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& name = call.callee->as<parser::IdentExpr>().name;
            // Memory allocation
            if (name == "alloc" || name == "ptr_offset") {
                return true;
            }
            // Threading primitives that return handles
            if (name == "thread_spawn") {
                return true;
            }
            // Channel/Mutex/WaitGroup creation
            if (name == "channel_create" || name == "mutex_create" || name == "waitgroup_create") {
                return true;
            }
            // Collection creation (List, HashMap, Buffer)
            if (name == "list_create" || name == "hashmap_create" || name == "buffer_create") {
                return true;
            }
        }
    }
    return false;
}

void LLVMIRGen::gen_let_stmt(const parser::LetStmt& let) {
    std::string var_name;
    if (let.pattern->is<parser::IdentPattern>()) {
        var_name = let.pattern->as<parser::IdentPattern>().name;
    } else {
        var_name = "_anon" + std::to_string(temp_counter_++);
    }

    // Get the type - check for bool literals, comparisons, struct expressions, and refs
    std::string var_type = "i32";
    bool is_struct = false;
    bool is_ptr = false;
    if (let.type_annotation) {
        var_type = llvm_type_ptr(*let.type_annotation);
        is_struct = var_type.starts_with("%struct.");
        is_ptr = (var_type == "ptr");  // Collection types like List[T] are pointers
    } else if (let.init.has_value()) {
        // Infer type from initializer
        const auto& init = *let.init.value();
        if (is_bool_expr_static(init)) {
            var_type = "i1";
        } else if (init.is<parser::StructExpr>()) {
            const auto& s = init.as<parser::StructExpr>();
            if (!s.path.segments.empty()) {
                var_type = "%struct." + s.path.segments.back();
                is_struct = true;
            }
        } else if (is_ref_expr(init)) {
            var_type = "ptr";
            is_ptr = true;
        } else if (init.is<parser::CallExpr>()) {
            // Check function return type from type environment
            const auto& call = init.as<parser::CallExpr>();
            std::string fn_name;
            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                // Build full path name like "Instant::now"
                for (size_t i = 0; i < path_expr.path.segments.size(); ++i) {
                    if (i > 0) fn_name += "::";
                    fn_name += path_expr.path.segments[i];
                }
            } else if (call.callee->is<parser::IdentExpr>()) {
                fn_name = call.callee->as<parser::IdentExpr>().name;
            }
            if (!fn_name.empty()) {
                auto sig_opt = env_.lookup_func(fn_name);
                if (sig_opt && sig_opt->return_type) {
                    if (sig_opt->return_type->is<types::PrimitiveType>()) {
                        types::PrimitiveKind kind = sig_opt->return_type->as<types::PrimitiveType>().kind;
                        if (kind == types::PrimitiveKind::Str) {
                            var_type = "ptr";
                            is_ptr = true;
                        } else if (kind == types::PrimitiveKind::I64) {
                            var_type = "i64";
                        } else if (kind == types::PrimitiveKind::Bool) {
                            var_type = "i1";
                        }
                    }
                }
            }
        } else if (init.is<parser::LiteralExpr>()) {
            const auto& lit = init.as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                var_type = "ptr";
                is_ptr = true;
            }
        }
    }

    // For structs, we just track the alloca pointer
    if (is_struct && let.init.has_value() && let.init.value()->is<parser::StructExpr>()) {
        // gen_struct_expr allocates and initializes, returns the pointer
        std::string init_ptr = gen_struct_expr_ptr(let.init.value()->as<parser::StructExpr>());
        locals_[var_name] = VarInfo{init_ptr, var_type};
        return;
    }

    // For pointer variables, store the pointer value
    if (is_ptr && let.init.has_value()) {
        std::string ptr_val = gen_expr(*let.init.value());
        // Store pointer in a variable - we track the pointer value directly
        // The pointer points to the original variable's alloca
        locals_[var_name] = VarInfo{ptr_val, "ptr"};
        return;
    }

    // Initialize if there's a value - generate first to infer type
    std::string init_val;
    if (let.init.has_value()) {
        init_val = gen_expr(*let.init.value());
        // If type wasn't explicitly annotated and expression has a known type, use it
        if (!let.type_annotation && last_expr_type_ != "i32") {
            if (last_expr_type_ == "double" || last_expr_type_ == "i64" ||
                last_expr_type_ == "i1" || last_expr_type_ == "ptr") {
                var_type = last_expr_type_;
            }
        }
    }

    // Allocate on stack
    std::string alloca_reg = fresh_reg();
    emit_line("  " + alloca_reg + " = alloca " + var_type);

    // Store the value
    if (let.init.has_value()) {
        emit_line("  store " + var_type + " " + init_val + ", ptr " + alloca_reg);
    }

    // Map variable name to alloca with type info
    locals_[var_name] = VarInfo{alloca_reg, var_type};
}

void LLVMIRGen::gen_expr_stmt(const parser::ExprStmt& expr) {
    gen_expr(*expr.expr);
}

} // namespace tml::codegen
