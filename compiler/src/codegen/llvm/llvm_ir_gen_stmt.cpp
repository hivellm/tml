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
    // Binary expressions on strings (concatenation) heap-allocate
    if (expr.is<parser::BinaryExpr>())
        return true;
    // Function/method calls returning Str: only those marked @allocates produce
    // fresh heap-allocated Str. Non-@allocates functions may return borrowed
    // pointers (e.g., FFI functions returning const char* from C data structures).
    // Auto-freeing borrowed pointers causes double-free / heap corruption.
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
            if (name == "hashmap_create" || name == "buffer_create") {
                return true;
            }
        }
    }
    return false;
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

void LLVMIRGen::gen_let_stmt(const parser::LetStmt& let) {
    // Handle tuple pattern destructuring: let (a, b): (T1, T2) = expr
    if (let.pattern->is<parser::TuplePattern>()) {
        if (!let.init.has_value()) {
            errors_.push_back(LLVMGenError{.message = "Tuple pattern requires an initializer",
                                           .span = let.span,
                                           .notes = {},
                                           .code = "C022"});
            return;
        }

        // Get the tuple type from annotation
        std::string tuple_type;
        types::TypePtr semantic_tuple_type = nullptr;
        if (let.type_annotation) {
            semantic_tuple_type =
                resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
            tuple_type = llvm_type_from_semantic(semantic_tuple_type);
        }

        // Generate the initializer expression
        std::string init_val = gen_expr(*let.init.value());
        std::string expr_type = last_expr_type_;
        if (tuple_type.empty()) {
            tuple_type = expr_type;
        }

        // Parse the tuple types for both expected and actual
        std::vector<std::string> expected_elem_types = parse_tuple_types(tuple_type);
        std::vector<std::string> actual_elem_types = parse_tuple_types(expr_type);

        // Get semantic element types if we have a tuple type annotation
        std::vector<types::TypePtr> semantic_elem_types;
        if (semantic_tuple_type && semantic_tuple_type->is<types::TupleType>()) {
            const auto& tup = semantic_tuple_type->as<types::TupleType>();
            semantic_elem_types = tup.elements;
        }

        // Store tuple to temp using actual type
        std::string src_type = expr_type;
        std::string tuple_ptr = fresh_reg();
        emit_line("  " + tuple_ptr + " = alloca " + src_type);
        emit_line("  store " + src_type + " " + init_val + ", ptr " + tuple_ptr);

        // Extract and bind each pattern element
        const auto& tuple_pattern = let.pattern->as<parser::TuplePattern>();
        for (size_t i = 0; i < tuple_pattern.elements.size(); ++i) {
            const auto& elem_pattern = *tuple_pattern.elements[i];

            std::string actual_elem = i < actual_elem_types.size() ? actual_elem_types[i] : "i32";
            std::string expected_elem =
                i < expected_elem_types.size() ? expected_elem_types[i] : actual_elem;
            types::TypePtr semantic_elem =
                i < semantic_elem_types.size() ? semantic_elem_types[i] : nullptr;

            // Get pointer to element using GEP with the actual type
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + src_type + ", ptr " +
                      tuple_ptr + ", i32 0, i32 " + std::to_string(i));

            // Load the element value with the actual type
            std::string elem_val = fresh_reg();
            emit_line("  " + elem_val + " = load " + actual_elem + ", ptr " + elem_ptr);

            // Handle coercion if types differ
            std::string store_val = elem_val;
            if (actual_elem != expected_elem) {
                std::string conv = fresh_reg();
                if (expected_elem == "i64" && actual_elem == "i32") {
                    emit_line("  " + conv + " = sext i32 " + elem_val + " to i64");
                    store_val = conv;
                } else if (expected_elem == "i32" && actual_elem == "i64") {
                    emit_line("  " + conv + " = trunc i64 " + elem_val + " to i32");
                    store_val = conv;
                }
                // Add more conversions as needed
            }

            // Bind the element to its identifier or handle nested patterns
            if (elem_pattern.is<parser::IdentPattern>()) {
                const auto& ident = elem_pattern.as<parser::IdentPattern>();
                std::string alloca_reg = fresh_reg();
                emit_line("  " + alloca_reg + " = alloca " + expected_elem);
                emit_line("  store " + expected_elem + " " + store_val + ", ptr " + alloca_reg);
                locals_[ident.name] =
                    VarInfo{alloca_reg, expected_elem, semantic_elem, std::nullopt};
            } else if (elem_pattern.is<parser::WildcardPattern>()) {
                // Ignore the value
            } else if (elem_pattern.is<parser::TuplePattern>()) {
                // Handle nested tuple patterns recursively
                gen_tuple_pattern_binding(elem_pattern.as<parser::TuplePattern>(), store_val,
                                          expected_elem, semantic_elem);
            }
        }
        return;
    }

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
    types::TypePtr semantic_var_type = nullptr;
    if (let.type_annotation) {
        // Resolve type with current type substitutions (for generic impl methods)
        semantic_var_type =
            resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
        var_type = llvm_type_from_semantic(semantic_var_type);
        is_struct = var_type.starts_with("%struct.") || var_type.starts_with("%union.");
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
                } else if (union_types_.find(base_name) != union_types_.end()) {
                    // Union type
                    var_type = "%union." + base_name;
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

        // Register for drop if type implements Drop
        std::string type_name = extract_type_name_for_drop(var_type);
        register_for_drop(var_name, init_ptr, type_name, var_type);
        return;
    }

    // For class struct literals (e.g., let p: Point = Point { x: 1, y: 2 }),
    // we also track the alloca pointer directly - no extra indirection needed.
    // This is similar to structs but for class types which have var_type = "ptr".
    if (let.init.has_value() && let.init.value()->is<parser::StructExpr>()) {
        const auto& struct_expr = let.init.value()->as<parser::StructExpr>();
        if (!struct_expr.path.segments.empty()) {
            std::string base_name = struct_expr.path.segments.back();
            auto class_def = env_.lookup_class(base_name);
            if (class_def.has_value()) {
                // This is a class struct literal - allocate and store pointer directly
                std::string init_ptr = gen_struct_expr_ptr(struct_expr);
                std::string class_type = "%class." + base_name;
                locals_[var_name] = VarInfo{init_ptr, class_type, semantic_var_type, std::nullopt};

                // Register for drop if type implements Drop
                std::string type_name = extract_type_name_for_drop(class_type);
                register_for_drop(var_name, init_ptr, type_name, class_type);
                return;
            }
        }
    }

    // Handle dyn coercion: let d: dyn Describable = c (where c is Counter)
    // This also handles interface casting: let d: dyn Drawable = circle
    if (var_type.starts_with("%dyn.") && let.init.has_value()) {
        // Extract behavior/interface name from %dyn.Describable
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
                } else if (local_type.starts_with("%class.")) {
                    concrete_type = local_type.substr(7); // Skip "%class."
                } else if (local_type == "ptr") {
                    // For pointer types, try to get the type from semantic info
                    if (it->second.semantic_type &&
                        it->second.semantic_type->is<types::ClassType>()) {
                        concrete_type = it->second.semantic_type->as<types::ClassType>().name;
                    }
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

                // Get semantic type for generic dyn dispatch (e.g., dyn Processor[I32])
                types::TypePtr dyn_semantic = nullptr;
                if (let.type_annotation) {
                    dyn_semantic =
                        resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
                }

                locals_[var_name] = VarInfo{dyn_alloca, var_type, dyn_semantic, std::nullopt};
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
                    // Skip store for Unit enum types - "{}" is zero-sized
                    if (var_type != "{}") {
                        emit_line("  store " + var_type + " " + result + ", ptr " + alloca_reg);
                    }

                    locals_[var_name] = VarInfo{alloca_reg, var_type, nullptr, std::nullopt};

                    // Register for drop if type implements Drop
                    std::string type_name = extract_type_name_for_drop(var_type);
                    register_for_drop(var_name, alloca_reg, type_name, var_type);
                    return;
                }
            }
        }
    }

    // For function/closure types, allocate and store the value
    // Closures produce { ptr, ptr } fat pointers; plain func refs produce ptr
    if (let.type_annotation) {
        if (let.type_annotation.value()->is<parser::FuncType>()) {
            if (let.init.has_value()) {
                std::string closure_fn = gen_expr(*let.init.value());

                // Resolve semantic type for FuncType - needed for Fn trait method dispatch
                types::TypePtr semantic_type =
                    resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);

                // Check if the expression produced a fat pointer (closure)
                if (last_expr_type_ == "{ ptr, ptr }") {
                    // Store the full fat pointer { fn_ptr, env_ptr }
                    std::string alloca_reg = fresh_reg();
                    emit_line("  " + alloca_reg + " = alloca { ptr, ptr }");
                    emit_line("  store { ptr, ptr } " + closure_fn + ", ptr " + alloca_reg);
                    VarInfo info{alloca_reg, "{ ptr, ptr }", semantic_type, std::nullopt};
                    info.is_capturing_closure = last_closure_is_capturing_;
                    locals_[var_name] = info;
                } else {
                    // Plain function pointer (thin pointer) — store as ptr
                    std::string alloca_reg = fresh_reg();
                    emit_line("  " + alloca_reg + " = alloca ptr");
                    emit_line("  store ptr " + closure_fn + ", ptr " + alloca_reg);
                    locals_[var_name] = VarInfo{alloca_reg, "ptr", semantic_type, std::nullopt};
                }
                return;
            }
        }
    }

    // For pointer variables, allocate space and store the pointer value
    if (is_ptr && let.init.has_value()) {
        // Set expected type for generic class constructors BEFORE evaluating initializer
        if (let.type_annotation) {
            auto sem_type =
                resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
            if (sem_type && sem_type->is<types::ClassType>()) {
                const auto& class_type = sem_type->as<types::ClassType>();
                if (!class_type.type_args.empty()) {
                    // This is a generic class like Box[I32]
                    std::string mangled = mangle_struct_name(class_type.name, class_type.type_args);
                    expected_enum_type_ = "%class." + mangled;
                }
            }
        }
        std::string ptr_val = gen_expr(*let.init.value());
        std::string expr_type = last_expr_type_;

        // Handle lowlevel C runtime calls returning i32 when ptr is expected.
        // Functions called inside lowlevel blocks without @extern declarations
        // default to i32 return type, but may actually return void* (ptr).
        // Convert via inttoptr when the variable type annotation says *Unit/ptr.
        if (expr_type != "ptr" && (expr_type == "i32" || expr_type == "i64") && var_type == "ptr") {
            std::string converted = fresh_reg();
            emit_line("  " + converted + " = inttoptr " + expr_type + " " + ptr_val + " to ptr");
            ptr_val = converted;
            expr_type = "ptr";
        }

        // Handle value class returned by value: if var_type is ptr (class type) but
        // last_expr_type_ is a struct type, use the struct type for storage
        if (expr_type.starts_with("%class.")) {
            // Method returned a value class by value - store the struct directly
            std::string alloca_reg = fresh_reg();
            emit_line("  " + alloca_reg + " = alloca " + expr_type);
            emit_line("  store " + expr_type + " " + ptr_val + ", ptr " + alloca_reg);
            // Store with struct type so field access uses correct GEP
            types::TypePtr semantic_type = nullptr;
            if (let.type_annotation) {
                semantic_type =
                    resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
            }
            locals_[var_name] = VarInfo{alloca_reg, expr_type, semantic_type, std::nullopt};
            // Register for drop if type implements Drop
            std::string type_name = extract_type_name_for_drop(expr_type);
            register_for_drop(var_name, alloca_reg, type_name, expr_type);
            return;
        }

        // Regular pointer case
        // Allocate space to hold the pointer
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca ptr");
        // Store the pointer value in the alloca
        emit_line("  store ptr " + ptr_val + ", ptr " + alloca_reg);
        // Map variable to the alloca (gen_ident will load from it)
        // Also store semantic type for pointer method dispatch
        types::TypePtr semantic_type = nullptr;
        if (let.type_annotation) {
            semantic_type =
                resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
        }
        // Infer semantic type from init expression when no annotation present
        if (!semantic_type) {
            semantic_type = infer_expr_type(*let.init.value());
        }
        locals_[var_name] = VarInfo{alloca_reg, "ptr", semantic_type, std::nullopt};

        // Register heap Str for automatic free at scope exit.
        // Requires BOTH: semantic type is Str AND init expression may produce heap Str.
        // This prevents freeing non-Str ptr types (List, Box, etc.) that have own lifecycle.
        if ((is_semantic_str(semantic_var_type) || is_semantic_str(semantic_type)) &&
            is_heap_str_producer(*let.init.value())) {
            register_heap_str_for_drop(var_name, alloca_reg);
            // Remove the temp_drop that gen_expr registered for this same Str value,
            // since the variable's scope-based drop now owns the cleanup.
            if (!temp_drops_.empty() && temp_drops_.back().is_heap_str) {
                temp_drops_.pop_back();
            }
            // Also remove from pending_str_temps_ — let binding now owns the Str.
            consume_last_str_temp();
        }
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

        // Also handle generic class types
        // If type annotation is a ClassType with type_args, compute mangled class name
        if (let.type_annotation) {
            auto sem_type =
                resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
            if (sem_type && sem_type->is<types::ClassType>()) {
                const auto& class_type = sem_type->as<types::ClassType>();
                if (!class_type.type_args.empty()) {
                    // This is a generic class like Box[I32]
                    std::string mangled = mangle_struct_name(class_type.name, class_type.type_args);
                    expected_enum_type_ = "%class." + mangled;
                }
            }
        }

        // Set expected type for numeric literals based on type annotation
        // This allows "var a: U8 = 128" without requiring "128 as U8"
        if (let.type_annotation && semantic_var_type) {
            if (semantic_var_type->is<types::PrimitiveType>()) {
                const auto& prim = semantic_var_type->as<types::PrimitiveType>();
                switch (prim.kind) {
                case types::PrimitiveKind::I8:
                    expected_literal_type_ = "i8";
                    expected_literal_is_unsigned_ = false;
                    break;
                case types::PrimitiveKind::I16:
                    expected_literal_type_ = "i16";
                    expected_literal_is_unsigned_ = false;
                    break;
                case types::PrimitiveKind::I32:
                    expected_literal_type_ = "i32";
                    expected_literal_is_unsigned_ = false;
                    break;
                case types::PrimitiveKind::I64:
                case types::PrimitiveKind::I128:
                    expected_literal_type_ = "i64";
                    expected_literal_is_unsigned_ = false;
                    break;
                case types::PrimitiveKind::U8:
                    expected_literal_type_ = "i8";
                    expected_literal_is_unsigned_ = true;
                    break;
                case types::PrimitiveKind::U16:
                    expected_literal_type_ = "i16";
                    expected_literal_is_unsigned_ = true;
                    break;
                case types::PrimitiveKind::U32:
                    expected_literal_type_ = "i32";
                    expected_literal_is_unsigned_ = true;
                    break;
                case types::PrimitiveKind::U64:
                case types::PrimitiveKind::U128:
                    expected_literal_type_ = "i64";
                    expected_literal_is_unsigned_ = true;
                    break;
                case types::PrimitiveKind::F32:
                    expected_literal_type_ = "float";
                    expected_literal_is_unsigned_ = false;
                    break;
                case types::PrimitiveKind::F64:
                    expected_literal_type_ = "double";
                    expected_literal_is_unsigned_ = false;
                    break;
                default:
                    break;
                }
            }
        }

        init_val = gen_expr(*let.init.value());
        expected_enum_type_.clear(); // Clear context after expression
        expected_literal_type_.clear();
        expected_literal_is_unsigned_ = false;
        // If type wasn't explicitly annotated and expression has a known type, use it
        if (!let.type_annotation && last_expr_type_ != "i32") {
            if (last_expr_type_ == "float" || last_expr_type_ == "double" ||
                last_expr_type_ == "i8" || last_expr_type_ == "i16" || last_expr_type_ == "i64" ||
                last_expr_type_ == "i128" || last_expr_type_ == "i1" || last_expr_type_ == "ptr" ||
                last_expr_type_.starts_with("%struct.") || last_expr_type_.starts_with("%union.") ||
                last_expr_type_.starts_with("%class.") || last_expr_type_.starts_with("{") ||
                last_expr_type_.starts_with("<")) {
                var_type = last_expr_type_;
                is_struct = var_type.starts_with("%struct.") || var_type.starts_with("%union.") ||
                            var_type.starts_with("%class.");
            }
        }
        // Infer semantic type from init expression when no annotation is present.
        // This is needed for method dispatch on variables holding slice/tuple results.
        if (!let.type_annotation && !semantic_var_type) {
            semantic_var_type = infer_expr_type(*let.init.value());
        }
    }

    // Handle value class returned by value: if var_type is ptr (class type) but
    // last_expr_type_ is a struct type, use the struct type for storage
    if (let.init.has_value() && var_type == "ptr" && last_expr_type_.starts_with("%class.")) {
        // Method returned a value class by value - store the struct directly
        std::string struct_type = last_expr_type_;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + struct_type);
        // Emit lifetime.start for LLVM stack slot optimization
        int64_t struct_size = get_type_size(struct_type);
        emit_lifetime_start(alloca_reg, struct_size);
        register_alloca_in_scope(alloca_reg, struct_size);
        emit_line("  store " + struct_type + " " + init_val + ", ptr " + alloca_reg);
        // Store with struct type so field access uses correct GEP
        locals_[var_name] = VarInfo{alloca_reg, struct_type, semantic_var_type, std::nullopt};
        // Register for drop if type implements Drop
        std::string type_name = extract_type_name_for_drop(struct_type);
        register_for_drop(var_name, alloca_reg, type_name, struct_type);
        return;
    }

    // Allocate on stack
    std::string alloca_reg = fresh_reg();
    emit_line("  " + alloca_reg + " = alloca " + var_type);
    // Emit lifetime.start for LLVM stack slot optimization
    int64_t type_size = get_type_size(var_type);
    emit_lifetime_start(alloca_reg, type_size);
    register_alloca_in_scope(alloca_reg, type_size);

    // Store the value
    if (let.init.has_value()) {
        // Skip store for empty structs (unit type) - "{}" has no data to store
        if (var_type == "{}") {
            // No-op: unit type has no data
        }
        // Handle float/double type mismatch - need to convert if storing double to float
        else if (var_type == "float" && last_expr_type_ == "double") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = fptrunc double " + init_val + " to float");
            emit_line("  store float " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i64" && last_expr_type_ == "i32") {
            // Sign extend i32 to i64
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i32 " + init_val + " to i64");
            emit_line("  store i64 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i64" && last_expr_type_ == "i16") {
            // Sign extend i16 to i64
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i16 " + init_val + " to i64");
            emit_line("  store i64 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i64" && last_expr_type_ == "i8") {
            // Sign extend i8 to i64
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i8 " + init_val + " to i64");
            emit_line("  store i64 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i32" && last_expr_type_ == "i16") {
            // Sign extend i16 to i32
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i16 " + init_val + " to i32");
            emit_line("  store i32 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i32" && last_expr_type_ == "i8") {
            // Sign extend i8 to i32
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i8 " + init_val + " to i32");
            emit_line("  store i32 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i32" && last_expr_type_ == "i64") {
            // Truncate i64 to i32 (for cases like -2147483648 which overflows i32 literal)
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = trunc i64 " + init_val + " to i32");
            emit_line("  store i32 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i16" && last_expr_type_ == "i64") {
            // Truncate i64 to i16
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = trunc i64 " + init_val + " to i16");
            emit_line("  store i16 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i8" && last_expr_type_ == "i64") {
            // Truncate i64 to i8
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = trunc i64 " + init_val + " to i8");
            emit_line("  store i8 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i16" && last_expr_type_ == "i32") {
            // Truncate i32 to i16
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = trunc i32 " + init_val + " to i16");
            emit_line("  store i16 " + conv + ", ptr " + alloca_reg);
        } else if (var_type == "i8" && last_expr_type_ == "i32") {
            // Truncate i32 to i8
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = trunc i32 " + init_val + " to i8");
            emit_line("  store i8 " + conv + ", ptr " + alloca_reg);
        } else if (var_type.starts_with("[") && last_expr_type_.starts_with("[") &&
                   var_type != last_expr_type_) {
            // Array type coercion: [N x i32] -> [N x i64] etc.
            // Need to extract elements and convert each one
            size_t x_pos_expected = var_type.find(" x ");
            size_t x_pos_actual = last_expr_type_.find(" x ");
            if (x_pos_expected != std::string::npos && x_pos_actual != std::string::npos) {
                // Extract array size
                size_t arr_size = std::stoul(var_type.substr(1, x_pos_expected - 1));
                // Extract element types
                size_t end_bracket_expected = var_type.rfind("]");
                size_t end_bracket_actual = last_expr_type_.rfind("]");
                std::string elem_type_expected = var_type.substr(
                    x_pos_expected + 3, end_bracket_expected - (x_pos_expected + 3));
                std::string elem_type_actual = last_expr_type_.substr(
                    x_pos_actual + 3, end_bracket_actual - (x_pos_actual + 3));

                // Store source array to get element pointers
                std::string src_ptr = fresh_reg();
                emit_line("  " + src_ptr + " = alloca " + last_expr_type_);
                emit_line("  store " + last_expr_type_ + " " + init_val + ", ptr " + src_ptr);

                // Convert each element and store to destination
                for (size_t i = 0; i < arr_size; ++i) {
                    // Load from source
                    std::string src_elem_ptr = fresh_reg();
                    emit_line("  " + src_elem_ptr + " = getelementptr " + last_expr_type_ +
                              ", ptr " + src_ptr + ", i32 0, i32 " + std::to_string(i));
                    std::string src_elem = fresh_reg();
                    emit_line("  " + src_elem + " = load " + elem_type_actual + ", ptr " +
                              src_elem_ptr);

                    // Convert element type
                    std::string conv_elem = src_elem;
                    if (elem_type_expected != elem_type_actual) {
                        conv_elem = fresh_reg();
                        // Determine conversion type - handle all integer size combinations
                        // Sign extend: smaller -> larger
                        if (elem_type_expected == "i64" && elem_type_actual == "i32") {
                            emit_line("  " + conv_elem + " = sext i32 " + src_elem + " to i64");
                        } else if (elem_type_expected == "i64" && elem_type_actual == "i16") {
                            emit_line("  " + conv_elem + " = sext i16 " + src_elem + " to i64");
                        } else if (elem_type_expected == "i64" && elem_type_actual == "i8") {
                            emit_line("  " + conv_elem + " = sext i8 " + src_elem + " to i64");
                        } else if (elem_type_expected == "i32" && elem_type_actual == "i16") {
                            emit_line("  " + conv_elem + " = sext i16 " + src_elem + " to i32");
                        } else if (elem_type_expected == "i32" && elem_type_actual == "i8") {
                            emit_line("  " + conv_elem + " = sext i8 " + src_elem + " to i32");
                        } else if (elem_type_expected == "i16" && elem_type_actual == "i8") {
                            emit_line("  " + conv_elem + " = sext i8 " + src_elem + " to i16");
                            // Truncate: larger -> smaller
                        } else if (elem_type_expected == "i32" && elem_type_actual == "i64") {
                            emit_line("  " + conv_elem + " = trunc i64 " + src_elem + " to i32");
                        } else if (elem_type_expected == "i16" && elem_type_actual == "i64") {
                            emit_line("  " + conv_elem + " = trunc i64 " + src_elem + " to i16");
                        } else if (elem_type_expected == "i8" && elem_type_actual == "i64") {
                            emit_line("  " + conv_elem + " = trunc i64 " + src_elem + " to i8");
                        } else if (elem_type_expected == "i16" && elem_type_actual == "i32") {
                            emit_line("  " + conv_elem + " = trunc i32 " + src_elem + " to i16");
                        } else if (elem_type_expected == "i8" && elem_type_actual == "i32") {
                            emit_line("  " + conv_elem + " = trunc i32 " + src_elem + " to i8");
                        } else if (elem_type_expected == "i8" && elem_type_actual == "i16") {
                            emit_line("  " + conv_elem + " = trunc i16 " + src_elem + " to i8");
                            // Float conversions
                        } else if (elem_type_expected == "double" && elem_type_actual == "float") {
                            emit_line("  " + conv_elem + " = fpext float " + src_elem +
                                      " to double");
                        } else if (elem_type_expected == "float" && elem_type_actual == "double") {
                            emit_line("  " + conv_elem + " = fptrunc double " + src_elem +
                                      " to float");
                        } else {
                            // Same type or unknown - just use source
                            conv_elem = src_elem;
                        }
                    }

                    // Store to destination
                    std::string dst_elem_ptr = fresh_reg();
                    emit_line("  " + dst_elem_ptr + " = getelementptr " + var_type + ", ptr " +
                              alloca_reg + ", i32 0, i32 " + std::to_string(i));
                    emit_line("  store " + elem_type_expected + " " + conv_elem + ", ptr " +
                              dst_elem_ptr);
                }
            } else {
                emit_line("  store " + var_type + " " + init_val + ", ptr " + alloca_reg);
            }
        } else {
            emit_line("  store " + var_type + " " + init_val + ", ptr " + alloca_reg);
        }
    }

    // Map variable name to alloca with type info
    // Also store semantic type if we have a type annotation (needed for ArrayType inference)
    // Use current_type_subs_ for proper substitution in generic impl methods
    types::TypePtr semantic_type = nullptr;
    if (let.type_annotation) {
        semantic_type = resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
    } else if (let.init.has_value() && var_type.starts_with("{")) {
        // For tuple types without type annotation, infer semantic type from the initializer
        // This is needed for tuple field access (pair.0, pair.1) to work correctly
        semantic_type = infer_expr_type(*let.init.value());
    }
    VarInfo var_info{alloca_reg, var_type, semantic_type, std::nullopt};
    if (var_type == "{ ptr, ptr }") {
        var_info.is_capturing_closure = last_closure_is_capturing_;
    }
    locals_[var_name] = var_info;

    // Register for drop if type implements Drop
    std::string type_name = extract_type_name_for_drop(var_type);
    register_for_drop(var_name, alloca_reg, type_name, var_type);

    // Register heap-allocated Str variables for automatic free at scope exit.
    // is_heap_str_producer checks if the init expression produces heap Str.
    if (var_type == "ptr" && let.init.has_value() &&
        (is_semantic_str(semantic_var_type) || is_semantic_str(semantic_type)) &&
        is_heap_str_producer(*let.init.value())) {
        register_heap_str_for_drop(var_name, alloca_reg);
        // Remove the temp_drop that gen_expr registered for this same Str value.
        if (!temp_drops_.empty() && temp_drops_.back().is_heap_str) {
            temp_drops_.pop_back();
        }
        // Also remove from pending_str_temps_ — var binding now owns the Str.
        consume_last_str_temp();
    }

    // Emit debug info for the variable (if enabled and debug level >= 2)
    // Level 1 only emits function scopes, level 2+ includes local variables
    if (options_.emit_debug_info && options_.debug_level >= 2 && current_scope_id_ != 0) {
        uint32_t line = let.span.start.line;
        uint32_t column = let.span.start.column;

        // Create debug info for the variable
        int var_debug_id = create_local_variable_debug_info(var_name, var_type, line);

        // Create debug location
        int loc_id = fresh_debug_id();
        std::ostringstream meta;
        meta << "!" << loc_id << " = !DILocation("
             << "line: " << line << ", "
             << "column: " << column << ", "
             << "scope: !" << current_scope_id_ << ")\n";
        debug_metadata_.push_back(meta.str());

        // Emit llvm.dbg.declare intrinsic
        emit_debug_declare(alloca_reg, var_debug_id, loc_id);
    }
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
                // The element is an enum struct { i32, payload... }
                // Extract payload pointer (field index 1)
                std::string payload_ptr = fresh_reg();
                emit_line("  " + payload_ptr + " = getelementptr inbounds " + elem_type + ", ptr " +
                          elem_ptr + ", i32 0, i32 1");

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
                    gen_tuple_pattern_binding(payload_pat->as<parser::TuplePattern>(), payload_val,
                                              bound_type, payload_type);
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
            emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " + std::to_string(variant_tag));
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
                    // Truncate if needed (i64 -> i32)
                    if (bound_type == "i32") {
                        std::string trunc = fresh_reg();
                        emit_line("  " + trunc + " = trunc i64 " + payload_raw + " to i32");
                        payload_val = trunc;
                    }

                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + bound_type);
                    emit_line("  store " + bound_type + " " + payload_val + ", ptr " + var_alloca);
                    locals_[ident.name] =
                        VarInfo{var_alloca, bound_type, payload_type, std::nullopt};
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
        register_for_drop(const_decl.name, alloca_reg, type_name, var_type);

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
