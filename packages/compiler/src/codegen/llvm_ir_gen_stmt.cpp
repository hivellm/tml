// LLVM IR generator - Statement generation
// Handles: let statements, expression statements

#include "codegen/llvm_ir_gen.hpp"

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
            if (name == "channel_send" || name == "channel_try_send" ||
                name == "channel_try_recv") {
                return true;
            }
            // Mutex functions that return bool
            if (name == "mutex_try_lock") {
                return true;
            }
            // Collection functions that return bool
            if (name == "hashmap_has" || name == "hashmap_remove" || name == "list_is_empty" ||
                name == "str_eq") {
                return true;
            }
        }
    }
    // Check for method calls that return bool
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& call = expr.as<parser::MethodCallExpr>();
        const auto& method = call.method;
        if (method == "is_empty" || method == "isEmpty" || method == "has" ||
            method == "contains" || method == "remove") {
            return true;
        }
    }
    return false;
}

// Helper to check if an expression is boolean-typed (with variable lookup)
bool is_bool_expr(const parser::Expr& expr,
                  const std::unordered_map<std::string, LLVMIRGen::VarInfo>& locals) {
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
        is_ptr = (var_type == "ptr"); // Collection types like List[T] are pointers
    } else if (let.init.has_value()) {
        // Infer type from initializer
        const auto& init = *let.init.value();
        if (is_bool_expr_static(init)) {
            var_type = "i1";
        } else if (init.is<parser::StructExpr>()) {
            const auto& s = init.as<parser::StructExpr>();
            if (!s.path.segments.empty()) {
                std::string base_name = s.path.segments.back();
                // Check if this is a generic struct - use mangled name
                auto generic_it = pending_generic_structs_.find(base_name);
                if (generic_it != pending_generic_structs_.end() && !s.fields.empty()) {
                    // Infer type from field values
                    types::TypePtr inferred = infer_expr_type(init);
                    var_type = llvm_type_from_semantic(inferred);
                } else {
                    var_type = "%struct." + base_name;
                }
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
                    if (i > 0)
                        fn_name += "::";
                    fn_name += path_expr.path.segments[i];
                }
            } else if (call.callee->is<parser::IdentExpr>()) {
                fn_name = call.callee->as<parser::IdentExpr>().name;
            }
            if (!fn_name.empty()) {
                auto sig_opt = env_.lookup_func(fn_name);
                if (sig_opt && sig_opt->return_type) {
                    if (sig_opt->return_type->is<types::PrimitiveType>()) {
                        types::PrimitiveKind kind =
                            sig_opt->return_type->as<types::PrimitiveType>().kind;
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
        locals_[var_name] = VarInfo{init_ptr, var_type, nullptr, std::nullopt};
        return;
    }

    // Handle dyn coercion: let d: dyn Describable = c (where c is Counter)
    if (var_type.starts_with("%dyn.") && let.init.has_value()) {
        // Extract behavior name from %dyn.Describable
        std::string behavior_name = var_type.substr(5); // Skip "%dyn."

        // Get the concrete type name from the initializer
        std::string concrete_type;
        std::string data_ptr;

        if (let.init.value()->is<parser::IdentExpr>()) {
            const auto& ident = let.init.value()->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                // Get type from locals_
                std::string local_type = it->second.type;
                if (local_type.starts_with("%struct.")) {
                    concrete_type = local_type.substr(8); // Skip "%struct."
                }
                data_ptr = it->second.reg; // Use alloca pointer
            }
        }

        if (!concrete_type.empty() && !data_ptr.empty()) {
            // Look up the vtable
            std::string vtable = get_vtable(concrete_type, behavior_name);
            if (!vtable.empty()) {
                // Allocate the fat pointer struct
                std::string dyn_alloca = fresh_reg();
                emit_line("  " + dyn_alloca + " = alloca " + var_type);

                // Store data pointer (field 0)
                std::string data_field = fresh_reg();
                emit_line("  " + data_field + " = getelementptr " + var_type + ", ptr " +
                          dyn_alloca + ", i32 0, i32 0");
                emit_line("  store ptr " + data_ptr + ", ptr " + data_field);

                // Store vtable pointer (field 1)
                std::string vtable_field = fresh_reg();
                emit_line("  " + vtable_field + " = getelementptr " + var_type + ", ptr " +
                          dyn_alloca + ", i32 0, i32 1");
                emit_line("  store ptr " + vtable + ", ptr " + vtable_field);

                locals_[var_name] = VarInfo{dyn_alloca, var_type, nullptr, std::nullopt};
                return;
            }
        }
    }

    // Handle generic enum unit variants (like Nothing from Maybe[I32])
    // When we have an explicit type annotation for a generic enum, we need to use that type
    // rather than inferring from the expression (which can't infer type args for unit variants)
    if (is_struct && let.init.has_value() && let.init.value()->is<parser::IdentExpr>()) {
        const auto& ident_init = let.init.value()->as<parser::IdentExpr>();

        // Check if this is a unit variant of a generic enum
        for (const auto& [gen_enum_name, gen_enum_decl] : pending_generic_enums_) {
            for (size_t variant_idx = 0; variant_idx < gen_enum_decl->variants.size();
                 ++variant_idx) {
                const auto& variant = gen_enum_decl->variants[variant_idx];
                // Check if this is a unit variant (no tuple_fields and no struct_fields)
                bool is_unit =
                    !variant.tuple_fields.has_value() && !variant.struct_fields.has_value();

                if (variant.name == ident_init.name && is_unit) {
                    // Found matching unit variant - use var_type from annotation
                    // var_type should already be %struct.Maybe__I32 etc from annotation

                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack with correct mangled type
                    emit_line("  " + enum_val + " = alloca " + var_type + ", align 8");

                    // Set tag (field 0)
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + var_type + ", ptr " +
                              enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + var_type + ", ptr " + enum_val);

                    // Allocate storage for the variable
                    std::string alloca_reg = fresh_reg();
                    emit_line("  " + alloca_reg + " = alloca " + var_type);
                    emit_line("  store " + var_type + " " + result + ", ptr " + alloca_reg);

                    locals_[var_name] = VarInfo{alloca_reg, var_type, nullptr, std::nullopt};
                    return;
                }
            }
        }
    }

    // For function/closure types, store the function pointer directly
    if (let.type_annotation) {
        if (let.type_annotation.value()->is<parser::FuncType>()) {
            if (let.init.has_value()) {
                std::string closure_fn = gen_expr(*let.init.value());
                // closure_fn is like "@tml_closure_0", store it as a function pointer
                // Also capture closure info if it was a closure with captures
                VarInfo info{closure_fn, "ptr", nullptr, std::nullopt};
                if (last_closure_captures_.has_value()) {
                    info.closure_captures = last_closure_captures_;
                    last_closure_captures_ = std::nullopt; // Clear after use
                }
                locals_[var_name] = info;
                return;
            }
        }
    }

    // For pointer variables, allocate space and store the pointer value
    if (is_ptr && let.init.has_value()) {
        std::string ptr_val = gen_expr(*let.init.value());
        // Allocate space to hold the pointer
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca ptr");
        // Store the pointer value in the alloca
        emit_line("  store ptr " + ptr_val + ", ptr " + alloca_reg);
        // Map variable to the alloca (gen_ident will load from it)
        // Also store semantic type for pointer method dispatch
        types::TypePtr semantic_type = nullptr;
        if (let.type_annotation) {
            semantic_type = resolve_parser_type_with_subs(**let.type_annotation, {});
        }
        locals_[var_name] = VarInfo{alloca_reg, "ptr", semantic_type, std::nullopt};
        return;
    }

    // Initialize if there's a value - generate first to infer type
    std::string init_val;
    if (let.init.has_value()) {
        // Set expected type for generic enum constructors
        // If var_type is a generic enum like %struct.Outcome__I32__I32, set context
        if (is_struct && var_type.find("__") != std::string::npos) {
            expected_enum_type_ = var_type;
        }
        init_val = gen_expr(*let.init.value());
        expected_enum_type_.clear(); // Clear context after expression
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
        // Handle float/double type mismatch - need to convert if storing double to float
        if (var_type == "float" && last_expr_type_ == "double") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = fptrunc double " + init_val + " to float");
            emit_line("  store float " + conv + ", ptr " + alloca_reg);
        } else {
            emit_line("  store " + var_type + " " + init_val + ", ptr " + alloca_reg);
        }
    }

    // Map variable name to alloca with type info
    locals_[var_name] = VarInfo{alloca_reg, var_type, nullptr, std::nullopt};
}

void LLVMIRGen::gen_expr_stmt(const parser::ExprStmt& expr) {
    gen_expr(*expr.expr);
}

} // namespace tml::codegen
