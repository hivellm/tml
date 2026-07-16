TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Statements
//!
//! This file implements statement code generation.
//!
//! ## Statement Types
//!
//! | Statement | Handler          | Description                    |
//! |-----------|------------------|--------------------------------|
//! | `let`     | `gen_let_stmt`   | Immutable binding with alloca  |
//! | `var`     | `gen_let_stmt`   | Mutable binding with alloca    |
//! | `expr`    | `gen_expr_stmt`  | Expression as statement        |
//!
//! ## Variable Allocation
//!
//! Variables are stack-allocated via LLVM `alloca`:
//! ```llvm
//! %x = alloca i32
//! store i32 42, ptr %x
//! ```
//!
//! ## Drop Insertion
//!
//! For types implementing Drop, destructor calls are inserted at scope exit.
//! `extract_type_name_for_drop()` extracts the type name to look up drop glue.
//!
//! ## Pattern Binding
//!
//! Destructuring patterns generate multiple allocas and stores.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <sstream>

namespace tml::codegen {

// Helper to extract type name from LLVM type for drop checking
// e.g., "%struct.DroppableResource" -> "DroppableResource"
static std::string extract_type_name_for_drop(const std::string& llvm_type) {
    if (llvm_type.starts_with("%struct.")) {
        return llvm_type.substr(8); // Skip "%struct."
    }
    return "";
}

// Helper to check if a semantic type is Str
static bool is_semantic_str(const types::TypePtr& sem_type) {
    return sem_type && sem_type->is<types::PrimitiveType>() &&
           sem_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str;
}

// Helper to check if a string concat chain consists entirely of string literals.
// When all operands are literals, the codegen folds them into a single global
// constant at compile time — no heap allocation occurs.
static bool is_all_literal_str_concat(const parser::Expr& expr) {
    if (expr.is<parser::LiteralExpr>()) {
        return expr.as<parser::LiteralExpr>().token.kind == lexer::TokenKind::StringLiteral;
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        if (bin.op == parser::BinaryOp::Add) {
            return is_all_literal_str_concat(*bin.left) && is_all_literal_str_concat(*bin.right);
        }
    }
    return false;
}

// Helper to check if an expression produces a heap-allocated Str.
// Returns true for expressions that produce uniquely-owned heap Str.
// tml_str_free validates heap pointers before freeing, so it's safe to
// call on any pointer — global constants and stack pointers are skipped.
// All Str-returning stdlib functions allocate fresh heap memory.
bool LLVMIRGen::is_heap_str_producer(const parser::Expr& expr) const {
    // Interpolated strings always heap-allocate via snprintf+malloc
    if (expr.is<parser::InterpolatedStringExpr>())
        return true;
    // Template literals always heap-allocate
    if (expr.is<parser::TemplateLiteralExpr>())
        return true;
    // Binary expressions on strings (concatenation) heap-allocate —
    // EXCEPT when all operands are string literals (compile-time folded to constant).
    if (expr.is<parser::BinaryExpr>()) {
        if (is_all_literal_str_concat(expr))
            return false; // Folded to global constant, no heap allocation
        return true;
    }
    // Function/method calls: only track if the function is explicitly marked @allocates.
    // @allocates populates allocating_functions_ and means the function returns a
    // heap-allocated Str that the caller must free.
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        std::string func_name;
        if (call.callee->is<parser::IdentExpr>()) {
            func_name = call.callee->as<parser::IdentExpr>().name;
        } else if (call.callee->is<parser::PathExpr>()) {
            const auto& path = call.callee->as<parser::PathExpr>().path;
            if (!path.segments.empty()) {
                func_name = path.segments.back();
            }
        }
        return !func_name.empty() && allocating_functions_.count(func_name) > 0;
    }
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& mcall = expr.as<parser::MethodCallExpr>();
        return allocating_functions_.count(mcall.method) > 0;
    }
    // String literals are global constants — tml_str_free skips them (not heap)
    // Identifiers are aliases — freeing would double-free the original
    return false;
}

void LLVMIRGen::gen_stmt(const parser::Stmt& stmt) {
    if (stmt.is<parser::LetStmt>()) {
        gen_let_stmt(stmt.as<parser::LetStmt>());
    } else if (stmt.is<parser::LetElseStmt>()) {
        gen_let_else_stmt(stmt.as<parser::LetElseStmt>());
    } else if (stmt.is<parser::ExprStmt>()) {
        gen_expr_stmt(stmt.as<parser::ExprStmt>());
    } else if (stmt.is<parser::DeclPtr>()) {
        gen_nested_decl(*stmt.as<parser::DeclPtr>());
    }

    // After any statement completes, flush temporary drops.
    // Intermediates from method chains (e.g., MutexGuard from m.lock().get())
    // must be dropped at statement end. This is safe even for gen_expr_stmt
    // which already calls emit_temp_drops() — double call is a no-op.
    emit_temp_drops();

    // Free any heap Str temporaries that weren't consumed by let/var bindings.
    // E.g., assert_eq(x.to_string(), "42", "msg") — to_string() result is freed here.
    flush_str_temps();
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
            if (name == "hashmap_has" || name == "hashmap_remove" || name == "str_eq") {
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

// Helper to parse tuple type string into element types
static std::vector<std::string> parse_tuple_types(const std::string& tuple_type) {
    std::vector<std::string> element_types;
    if (tuple_type.size() > 2 && tuple_type.front() == '{' && tuple_type.back() == '}') {
        // Parse "{ i32, i64, ptr }" -> ["i32", "i64", "ptr"]
        std::string inner = tuple_type.substr(2, tuple_type.size() - 4);
        int brace_depth = 0;
        int bracket_depth = 0;
        std::string current;

        for (size_t i = 0; i < inner.size(); ++i) {
            char c = inner[i];
            if (c == '{') {
                brace_depth++;
                current += c;
            } else if (c == '}') {
                brace_depth--;
                current += c;
            } else if (c == '[') {
                bracket_depth++;
                current += c;
            } else if (c == ']') {
                bracket_depth--;
                current += c;
            } else if (c == ',' && brace_depth == 0 && bracket_depth == 0) {
                // Trim whitespace
                size_t start = current.find_first_not_of(" ");
                size_t end = current.find_last_not_of(" ");
                if (start != std::string::npos) {
                    element_types.push_back(current.substr(start, end - start + 1));
                }
                current.clear();
            } else {
                current += c;
            }
        }
        // Don't forget the last element
        if (!current.empty()) {
            size_t start = current.find_first_not_of(" ");
            size_t end = current.find_last_not_of(" ");
            if (start != std::string::npos) {
                element_types.push_back(current.substr(start, end - start + 1));
            }
        }
    }
    return element_types;
}

static bool is_ref_expr(const parser::Expr& expr) {
    if (expr.is<parser::UnaryExpr>()) {
        const auto& un = expr.as<parser::UnaryExpr>();
        return un.op == parser::UnaryOp::Ref || un.op == parser::UnaryOp::RefMut;
    }
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& name = call.callee->as<parser::IdentExpr>().name;
            if (name == "ptr_read" || name == "ptr_offset" || name == "mem_alloc")
                return true;
        }
    }
    return false;
}

void LLVMIRGen::gen_expr_stmt(const parser::ExprStmt& expr) {
    std::string result = gen_expr(*expr.expr);

    // If the expression is a call/method that returned a droppable struct value,
    // drop it. Only applies to actual call expressions (not all expressions),
    // and the result must be a valid LLVM register (starts with '%').
    if (!result.empty() && result[0] == '%' && last_expr_type_.starts_with("%struct.") &&
        (expr.expr->is<parser::CallExpr>() || expr.expr->is<parser::MethodCallExpr>())) {
        std::string type_name = extract_type_name_for_drop(last_expr_type_);
        if (!type_name.empty()) {
            bool has_drop = env_.type_implements(type_name, "Drop");
            if (!has_drop) {
                auto sep = type_name.find("__");
                if (sep != std::string::npos) {
                    has_drop = env_.type_implements(type_name.substr(0, sep), "Drop");
                }
            }
            bool needs_field_drops = !has_drop && env_.type_needs_drop(type_name);
            if (has_drop || needs_field_drops) {
                register_temp_for_drop(result, type_name, last_expr_type_);
            }
        }
    }

    // Drop any temporary droppable values produced during this expression.
    // This handles both the discarded return value above and any intermediates
    // from method chains (e.g., a.lock().get() — MutexGuard is intermediate).
    emit_temp_drops();
}

void LLVMIRGen::gen_tuple_pattern_binding(const parser::TuplePattern& pattern,
                                          const std::string& value, const std::string& tuple_type,
                                          const types::TypePtr& semantic_type) {
    // Parse element types from the tuple type string
    std::vector<std::string> elem_types = parse_tuple_types(tuple_type);

    // Get semantic element types if available
    std::vector<types::TypePtr> semantic_elem_types;
    if (semantic_type && semantic_type->is<types::TupleType>()) {
        const auto& tup = semantic_type->as<types::TupleType>();
        semantic_elem_types = tup.elements;
    }

    // Store the tuple value to a temporary so we can GEP into it
    std::string tuple_ptr = fresh_reg();
    emit_line("  " + tuple_ptr + " = alloca " + tuple_type);
    emit_line("  store " + tuple_type + " " + value + ", ptr " + tuple_ptr);

    // Extract and bind each element
    for (size_t i = 0; i < pattern.elements.size(); ++i) {
        const auto& elem_pattern = *pattern.elements[i];

        std::string elem_type = i < elem_types.size() ? elem_types[i] : "i32";
        types::TypePtr semantic_elem =
            i < semantic_elem_types.size() ? semantic_elem_types[i] : nullptr;

        // Get pointer to element
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr inbounds " + tuple_type + ", ptr " +
                  tuple_ptr + ", i32 0, i32 " + std::to_string(i));

        // Load the element
        std::string elem_val = fresh_reg();
        emit_line("  " + elem_val + " = load " + elem_type + ", ptr " + elem_ptr);

        // Bind based on pattern type
        if (elem_pattern.is<parser::IdentPattern>()) {
            const auto& ident = elem_pattern.as<parser::IdentPattern>();
            std::string alloca_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + elem_type);
            emit_line("  store " + elem_type + " " + elem_val + ", ptr " + alloca_reg);
            locals_[ident.name] = VarInfo{alloca_reg, elem_type, semantic_elem, std::nullopt};
        } else if (elem_pattern.is<parser::WildcardPattern>()) {
            // Ignore the value
        } else if (elem_pattern.is<parser::TuplePattern>()) {
            // Recursively handle nested tuple patterns
            gen_tuple_pattern_binding(elem_pattern.as<parser::TuplePattern>(), elem_val, elem_type,
                                      semantic_elem);
        } else if (elem_pattern.is<parser::EnumPattern>()) {
            // Handle enum destructuring inside tuple patterns
            // e.g., (Just(a), Just(b)) where each element is an EnumPattern
            const auto& enum_pat = elem_pattern.as<parser::EnumPattern>();

            if (enum_pat.payload.has_value() && !enum_pat.payload->empty()) {
                // Check for nullable Maybe optimization (elem_type == "ptr")
                bool is_nullable_maybe = false;
                if (semantic_elem && semantic_elem->is<types::NamedType>()) {
                    const auto& named = semantic_elem->as<types::NamedType>();
                    if (named.name == "Maybe" && elem_type == "ptr") {
                        is_nullable_maybe = true;
                    }
                }

                if (is_nullable_maybe) {
                    // Nullable Maybe: the elem_val IS the payload pointer
                    // Just(a) → bind a = elem_val (which is the ptr, non-null in Just arm)
                    types::TypePtr payload_type = nullptr;
                    if (semantic_elem && semantic_elem->is<types::NamedType>()) {
                        const auto& named = semantic_elem->as<types::NamedType>();
                        if (!named.type_args.empty()) {
                            payload_type = named.type_args[0];
                        }
                    }
                    std::string bound_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "ptr";

                    const auto& payload_pat = enum_pat.payload->at(0);
                    if (payload_pat->is<parser::IdentPattern>()) {
                        const auto& ident = payload_pat->as<parser::IdentPattern>();
                        if (!ident.name.empty() && ident.name != "_") {
                            // elem_val is already the ptr payload; store to alloca
                            std::string var_alloca = fresh_reg();
                            emit_line("  " + var_alloca + " = alloca " + bound_type);
                            emit_line("  store " + bound_type + " " + elem_val + ", ptr " +
                                      var_alloca);
                            locals_[ident.name] =
                                VarInfo{var_alloca, bound_type, payload_type, std::nullopt};
                        }
                    }
                } else {
                    // Normal enum struct { i32, payload... }
                    // Extract payload pointer (field index 1)
                    std::string payload_ptr = fresh_reg();
                    emit_line("  " + payload_ptr + " = getelementptr inbounds " + elem_type +
                              ", ptr " + elem_ptr + ", i32 0, i32 1");

                    // Determine the payload type from semantic type info
                    types::TypePtr payload_type = nullptr;
                    if (semantic_elem && semantic_elem->is<types::NamedType>()) {
                        const auto& named = semantic_elem->as<types::NamedType>();
                        std::string variant_name;
                        if (!enum_pat.path.segments.empty()) {
                            variant_name = enum_pat.path.segments.back();
                        }

                        if (named.name == "Maybe" && !named.type_args.empty()) {
                            if (variant_name == "Just") {
                                payload_type = named.type_args[0];
                            }
                        } else if (named.name == "Outcome" && named.type_args.size() >= 2) {
                            if (variant_name == "Ok") {
                                payload_type = named.type_args[0];
                            } else if (variant_name == "Err") {
                                payload_type = named.type_args[1];
                            }
                        } else {
                            // Look up the enum definition for other enum types
                            auto enum_def = env_.lookup_enum(named.name);
                            if (enum_def.has_value()) {
                                for (const auto& [var_name, var_payloads] : enum_def->variants) {
                                    if (var_name == variant_name && !var_payloads.empty()) {
                                        payload_type = var_payloads[0];
                                        break;
                                    }
                                }
                            }
                            // Substitute type parameters if needed
                            if (payload_type && !named.type_args.empty()) {
                                std::unordered_map<std::string, types::TypePtr> type_subs;
                                auto enum_def2 = env_.lookup_enum(named.name);
                                if (enum_def2 && !enum_def2->type_params.empty()) {
                                    for (size_t j = 0; j < enum_def2->type_params.size() &&
                                                       j < named.type_args.size();
                                         ++j) {
                                        type_subs[enum_def2->type_params[j]] = named.type_args[j];
                                    }
                                }
                                if (!type_subs.empty()) {
                                    payload_type = types::substitute_type(payload_type, type_subs);
                                }
                            }
                        }
                    }

                    std::string bound_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "i64";

                    // Bind the payload variable(s)
                    const auto& payload_pat = enum_pat.payload->at(0);
                    if (payload_pat->is<parser::IdentPattern>()) {
                        const auto& ident = payload_pat->as<parser::IdentPattern>();
                        if (!ident.name.empty() && ident.name != "_") {
                            if (bound_type.starts_with("%struct.") || bound_type.starts_with("{")) {
                                // Struct/tuple type - variable is the pointer
                                locals_[ident.name] =
                                    VarInfo{payload_ptr, bound_type, payload_type, std::nullopt};
                            } else {
                                // Primitive type - load from payload
                                std::string payload_val = fresh_reg();
                                emit_line("  " + payload_val + " = load " + bound_type + ", ptr " +
                                          payload_ptr);
                                std::string var_alloca = fresh_reg();
                                emit_line("  " + var_alloca + " = alloca " + bound_type);
                                emit_line("  store " + bound_type + " " + payload_val + ", ptr " +
                                          var_alloca);
                                locals_[ident.name] =
                                    VarInfo{var_alloca, bound_type, payload_type, std::nullopt};
                            }
                        }
                    } else if (payload_pat->is<parser::TuplePattern>()) {
                        // Nested tuple in enum payload: e.g., Ok((a, b))
                        std::string payload_val = fresh_reg();
                        emit_line("  " + payload_val + " = load " + bound_type + ", ptr " +
                                  payload_ptr);
                        gen_tuple_pattern_binding(payload_pat->as<parser::TuplePattern>(),
                                                  payload_val, bound_type, payload_type);
                    }
                }
            }
        }
    }
}

void LLVMIRGen::gen_let_else_stmt(const parser::LetElseStmt& let_else) {
    // let Pattern: Type = expr else { diverging_block }
    //
    // This is similar to if-let but with different control flow:
    // - If pattern matches: bind variables and continue
    // - If pattern doesn't match: execute else block (which must diverge)

    // Evaluate scrutinee
    std::string scrutinee = gen_expr(*let_else.init);
    std::string scrutinee_type = last_expr_type_;

    // Get semantic type for better payload handling
    types::TypePtr scrutinee_semantic = infer_expr_type(*let_else.init);
    if (scrutinee_type == "ptr" && scrutinee_semantic) {
        scrutinee_type = llvm_type_from_semantic(scrutinee_semantic);
    }

    std::string label_match = fresh_label("letelse.match");
    std::string label_else = fresh_label("letelse.else");
    std::string label_cont = fresh_label("letelse.cont");

    // Handle enum patterns (most common for let-else with Maybe/Outcome)
    if (let_else.pattern->is<parser::EnumPattern>()) {
        const auto& enum_pat = let_else.pattern->as<parser::EnumPattern>();
        std::string variant_name = enum_pat.path.segments.back();

        // Detect nullable Maybe optimization: Maybe[Str], Maybe[ref T], etc.
        // are represented as bare pointers (null = Nothing, non-null = Just).
        // This matches the detection in when.cpp (gen_when_expr).
        bool is_nullable_maybe = false;
        if (scrutinee_type == "ptr" && scrutinee_semantic &&
            scrutinee_semantic->is<types::NamedType>() &&
            scrutinee_semantic->as<types::NamedType>().name == "Maybe") {
            is_nullable_maybe = true;
        }

        if (is_nullable_maybe) {
            // Nullable pointer optimization path.
            // Discriminant: null = Nothing (tag 1), non-null = Just (tag 0).
            std::string is_null = fresh_reg();
            emit_line("  " + is_null + " = icmp eq ptr " + scrutinee + ", null");
            std::string tag = fresh_reg();
            emit_line("  " + tag + " = zext i1 " + is_null + " to i32");

            // Find variant tag (Just=0, Nothing=1)
            int variant_tag = -1;
            if (variant_name == "Just")
                variant_tag = 0;
            else if (variant_name == "Nothing")
                variant_tag = 1;
            else {
                // Fallback to env lookup
                for (const auto& [enum_name, enum_def] : env_.all_enums()) {
                    for (size_t v_idx = 0; v_idx < enum_def.variants.size(); ++v_idx) {
                        if (enum_def.variants[v_idx].first == variant_name) {
                            variant_tag = static_cast<int>(v_idx);
                            break;
                        }
                    }
                    if (variant_tag >= 0)
                        break;
                }
            }

            // Compare tag and branch
            if (variant_tag >= 0) {
                std::string cmp = fresh_reg();
                emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " +
                          std::to_string(variant_tag));
                emit_line("  br i1 " + cmp + ", label %" + label_match + ", label %" + label_else);
            } else {
                emit_line("  br label %" + label_else);
            }

            // Match block - bind pattern variables
            emit_line(label_match + ":");
            block_terminated_ = false;

            if (enum_pat.payload.has_value() && !enum_pat.payload->empty()) {
                // For nullable Maybe, the payload IS the pointer itself (scrutinee).
                // Get payload type from semantic info.
                types::TypePtr payload_type = nullptr;
                if (scrutinee_semantic->is<types::NamedType>()) {
                    const auto& named = scrutinee_semantic->as<types::NamedType>();
                    if (named.name == "Maybe" && !named.type_args.empty() &&
                        variant_name == "Just") {
                        payload_type = named.type_args[0];
                    }
                }

                if (enum_pat.payload->at(0)->is<parser::IdentPattern>()) {
                    const auto& ident = enum_pat.payload->at(0)->as<parser::IdentPattern>();
                    std::string bound_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "ptr";

                    // For nullable Maybe, the scrutinee ptr IS the payload value.
                    // Store it in an alloca so the variable has an address.
                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + bound_type);
                    emit_line("  store " + bound_type + " " + scrutinee + ", ptr " + var_alloca);
                    locals_[ident.name] =
                        VarInfo{var_alloca, bound_type, payload_type, std::nullopt};
                }
            }
        } else {
            // Non-nullable enum path (standard struct-based enum representation).

            // Get pointer to scrutinee
            std::string scrutinee_ptr;
            if (last_expr_type_ == "ptr") {
                scrutinee_ptr = scrutinee;
            } else {
                scrutinee_ptr = fresh_reg();
                emit_line("  " + scrutinee_ptr + " = alloca " + scrutinee_type);
                emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + scrutinee_ptr);
            }

            // Extract tag
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " +
                      scrutinee_ptr + ", i32 0, i32 0");
            std::string tag = fresh_reg();
            emit_line("  " + tag + " = load i32, ptr " + tag_ptr);

            // Find variant index
            int variant_tag = -1;
            std::string scrutinee_enum_name;
            if (scrutinee_type.starts_with("%struct.")) {
                scrutinee_enum_name = scrutinee_type.substr(8);
            }

            if (!scrutinee_enum_name.empty()) {
                std::string key = scrutinee_enum_name + "::" + variant_name;
                auto it = enum_variants_.find(key);
                if (it != enum_variants_.end()) {
                    variant_tag = it->second;
                }
            }

            // Fallback to env lookup
            if (variant_tag < 0) {
                for (const auto& [enum_name, enum_def] : env_.all_enums()) {
                    for (size_t v_idx = 0; v_idx < enum_def.variants.size(); ++v_idx) {
                        if (enum_def.variants[v_idx].first == variant_name) {
                            variant_tag = static_cast<int>(v_idx);
                            break;
                        }
                    }
                    if (variant_tag >= 0)
                        break;
                }
            }

            // Compare tag and branch
            if (variant_tag >= 0) {
                std::string cmp = fresh_reg();
                emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " +
                          std::to_string(variant_tag));
                emit_line("  br i1 " + cmp + ", label %" + label_match + ", label %" + label_else);
            } else {
                emit_line("  br label %" + label_else);
            }

            // Match block - bind pattern variables
            emit_line(label_match + ":");
            block_terminated_ = false;

            if (enum_pat.payload.has_value() && !enum_pat.payload->empty()) {
                std::string payload_ptr = fresh_reg();
                emit_line("  " + payload_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + scrutinee_ptr + ", i32 0, i32 1");

                // Get payload type from semantic info
                types::TypePtr payload_type = nullptr;
                if (scrutinee_semantic && scrutinee_semantic->is<types::NamedType>()) {
                    const auto& named = scrutinee_semantic->as<types::NamedType>();
                    if (named.name == "Outcome" && named.type_args.size() >= 2) {
                        if (variant_name == "Ok")
                            payload_type = named.type_args[0];
                        else if (variant_name == "Err")
                            payload_type = named.type_args[1];
                    } else if (named.name == "Maybe" && !named.type_args.empty()) {
                        if (variant_name == "Just")
                            payload_type = named.type_args[0];
                    }
                }

                // Bind first payload element
                if (enum_pat.payload->at(0)->is<parser::IdentPattern>()) {
                    const auto& ident = enum_pat.payload->at(0)->as<parser::IdentPattern>();
                    std::string bound_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "i64";

                    if (bound_type.starts_with("%struct.") || bound_type.starts_with("{")) {
                        // Struct/tuple: variable is pointer to payload
                        locals_[ident.name] =
                            VarInfo{payload_ptr, bound_type, payload_type, std::nullopt};
                    } else {
                        // Primitive: load and allocate
                        std::string payload_raw = fresh_reg();
                        emit_line("  " + payload_raw + " = load i64, ptr " + payload_ptr);

                        std::string payload_val = payload_raw;
                        // Convert from i64 storage to actual type
                        if (bound_type == "double" || bound_type == "float") {
                            // Float types: bitcast from i64
                            std::string cast = fresh_reg();
                            emit_line("  " + cast + " = bitcast i64 " + payload_raw + " to " +
                                      bound_type);
                            payload_val = cast;
                        } else if (bound_type != "i64" && bound_type != "ptr") {
                            // Integer types smaller than i64: truncate
                            std::string trunc = fresh_reg();
                            emit_line("  " + trunc + " = trunc i64 " + payload_raw + " to " +
                                      bound_type);
                            payload_val = trunc;
                        }

                        std::string var_alloca = fresh_reg();
                        emit_line("  " + var_alloca + " = alloca " + bound_type);
                        emit_line("  store " + bound_type + " " + payload_val + ", ptr " +
                                  var_alloca);
                        locals_[ident.name] =
                            VarInfo{var_alloca, bound_type, payload_type, std::nullopt};
                    }
                }
            }
        }

        // Continue to rest of function
        emit_line("  br label %" + label_cont);

        // Else block - pattern didn't match, execute diverging block
        emit_line(label_else + ":");
        block_terminated_ = false;
        gen_expr(*let_else.else_block);
        // The else block should diverge (return/panic), but add branch just in case
        if (!block_terminated_) {
            emit_line("  br label %" + label_cont);
        }

        // Continue block
        emit_line(label_cont + ":");
        current_block_ = label_cont;
        block_terminated_ = false;
    } else {
        // For non-enum patterns, just bind directly (fallback)
        // This handles simple ident patterns that always match
        emit_line("  br label %" + label_match);
        emit_line(label_match + ":");
        block_terminated_ = false;
        emit_line("  br label %" + label_cont);
        emit_line(label_cont + ":");
        current_block_ = label_cont;
        block_terminated_ = false;
    }
}

void LLVMIRGen::gen_nested_decl(const parser::Decl& decl) {
    // Handle nested declarations (const, func, type, etc.)
    if (decl.is<parser::ConstDecl>()) {
        const auto& const_decl = decl.as<parser::ConstDecl>();
        // const NAME: TYPE = value is essentially the same as let NAME: TYPE = value
        // Generate like a let statement

        // Get LLVM type from annotation
        std::string var_type = llvm_type(*const_decl.type);

        // Generate initializer value
        std::string init_val = gen_expr(*const_decl.value);

        // Allocate on stack
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + var_type);

        // Emit lifetime.start for LLVM stack slot optimization
        int64_t type_size = get_type_size(var_type);
        emit_lifetime_start(alloca_reg, type_size);
        register_alloca_in_scope(alloca_reg, type_size);

        // Store the value (with type conversions if needed)
        if (var_type == "float" && last_expr_type_ == "double") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = fptrunc double " + init_val + " to float");
            emit_line("  store float " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i64" && last_expr_type_ == "i32") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i32 " + init_val + " to i64");
            emit_line("  store i64 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i32" && last_expr_type_ == "i64") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = trunc i64 " + init_val + " to i32");
            emit_line("  store i32 " + conv + ", ptr " + alloca_reg);
        } else {
            emit_line("  store " + var_type + " " + init_val + ", ptr " + alloca_reg);
        }

        // Map const name to alloca
        types::TypePtr semantic_type =
            resolve_parser_type_with_subs(*const_decl.type, current_type_subs_);
        locals_[const_decl.name] = VarInfo{alloca_reg, var_type, semantic_type, std::nullopt};

        // Register for drop if type implements Drop
        std::string type_name = extract_type_name_for_drop(var_type);
        register_for_drop(const_decl.name, alloca_reg, type_name, var_type, const_decl.span);

        // Register heap Str for automatic free
        if (var_type == "ptr" && is_semantic_str(semantic_type) &&
            is_heap_str_producer(*const_decl.value)) {
            register_heap_str_for_drop(const_decl.name, alloca_reg);
            if (!temp_drops_.empty() && temp_drops_.back().is_heap_str) {
                temp_drops_.pop_back();
            }
            consume_last_str_temp();
        }
    }
    // Other nested declarations (func, type, etc.) are handled elsewhere or ignored
}

} // namespace tml::codegen
