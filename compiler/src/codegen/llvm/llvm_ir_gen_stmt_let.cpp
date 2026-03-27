TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Let Statement
//!
//! This file implements code generation for `let`/`var` binding statements.
//!
//! Extracted from llvm_ir_gen_stmt.cpp to keep file sizes manageable.
//! See that file for the full statement dispatch and other statement types.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <sstream>

namespace tml::codegen {

// Static helpers (duplicated from llvm_ir_gen_stmt.cpp — they're file-static)
static std::string extract_type_name_for_drop(const std::string& llvm_type) {
    if (llvm_type.starts_with("%struct.")) {
        return llvm_type.substr(8);
    }
    return "";
}

static bool is_semantic_str(const types::TypePtr& sem_type) {
    return sem_type && sem_type->is<types::PrimitiveType>() &&
           sem_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str;
}

static bool is_bool_expr_static(const parser::Expr& expr) {
    if (expr.is<parser::LiteralExpr>())
        return expr.as<parser::LiteralExpr>().token.kind == lexer::TokenKind::BoolLiteral;
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
    if (expr.is<parser::UnaryExpr>())
        return expr.as<parser::UnaryExpr>().op == parser::UnaryOp::Not;
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& name = call.callee->as<parser::IdentExpr>().name;
            if (name == "atomic_cas" || name == "spin_trylock")
                return true;
            if (name == "channel_send" || name == "channel_try_send" || name == "channel_try_recv")
                return true;
            if (name == "mutex_try_lock")
                return true;
            if (name == "hashmap_has" || name == "hashmap_remove" || name == "str_eq")
                return true;
        }
    }
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& call = expr.as<parser::MethodCallExpr>();
        if (call.method == "is_empty" || call.method == "isEmpty" || call.method == "has" ||
            call.method == "contains" || call.method == "remove")
            return true;
    }
    return false;
}

static std::vector<std::string> parse_tuple_types(const std::string& tuple_type) {
    std::vector<std::string> element_types;
    if (tuple_type.size() > 2 && tuple_type.front() == '{' && tuple_type.back() == '}') {
        std::string inner = tuple_type.substr(2, tuple_type.size() - 4);
        int brace_depth = 0, bracket_depth = 0;
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
                size_t start = current.find_first_not_of(' ');
                size_t end = current.find_last_not_of(' ');
                if (start != std::string::npos)
                    element_types.push_back(current.substr(start, end - start + 1));
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            size_t start = current.find_first_not_of(' ');
            size_t end = current.find_last_not_of(' ');
            if (start != std::string::npos)
                element_types.push_back(current.substr(start, end - start + 1));
        }
    }
    return element_types;
}

// Helper to check if expression is a reference (pointer) expression
static bool is_ref_expr(const parser::Expr& expr) {
    if (expr.is<parser::UnaryExpr>()) {
        const auto& un = expr.as<parser::UnaryExpr>();
        return un.op == parser::UnaryOp::Ref || un.op == parser::UnaryOp::RefMut;
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
        // Use for_data=true because variable declarations are data contexts —
        // Unit should be "{}" (empty struct), not "void" which can't be alloca'd.
        var_type = llvm_type_from_semantic(semantic_var_type, /*for_data=*/true);
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
                emit_line("  " + data_field + " = getelementptr inbounds " + var_type + ", ptr " +
                          dyn_alloca + ", i32 0, i32 0");
                emit_line("  store ptr " + data_ptr + ", ptr " + data_field);

                // Store vtable pointer (field 1)
                std::string vtable_field = fresh_reg();
                emit_line("  " + vtable_field + " = getelementptr inbounds " + var_type + ", ptr " +
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

                    locals_[var_name] =
                        VarInfo{alloca_reg, var_type, semantic_var_type, std::nullopt};

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
        // Use type_annotation if present, or fall back to semantic_var_type from call inference
        types::TypePtr sem_type = nullptr;
        if (let.type_annotation) {
            sem_type = resolve_parser_type_with_subs(**let.type_annotation, current_type_subs_);
        } else if (semantic_var_type) {
            sem_type = semantic_var_type;
        }
        if (sem_type) {
            if (sem_type->is<types::ClassType>()) {
                const auto& class_type = sem_type->as<types::ClassType>();
                if (!class_type.type_args.empty()) {
                    // This is a generic class like Box[I32]
                    std::string mangled = mangle_struct_name(class_type.name, class_type.type_args);
                    expected_enum_type_ = "%class." + mangled;
                }
            }
            // Nullable pointer optimization: set expected_enum_type_ = "ptr" for Maybe[ptr-type]
            // so that Nothing/Just constructors in core.cpp know to use nullable representation
            if (sem_type->is<types::NamedType>()) {
                const auto& named = sem_type->as<types::NamedType>();
                if (named.name == "Maybe" && var_type == "ptr") {
                    expected_enum_type_ = "ptr";
                }
            }
        }
        std::string ptr_val = gen_expr(*let.init.value());
        expected_enum_type_.clear(); // Clear after generating initializer expression
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
        // Infer semantic type from init expression when no annotation is present
        if (!semantic_type) {
            semantic_type = infer_expr_type(*let.init.value());
        }
        // Use semantic_var_type from call return type inference if still no semantic type
        if (!semantic_type && semantic_var_type) {
            semantic_type = semantic_var_type;
        }
        // Use last_semantic_type_ from method dispatch (set by gen_impl_method_call etc.)
        if (!semantic_type && last_semantic_type_) {
            semantic_type = last_semantic_type_;
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
            } else if (semantic_var_type->is<types::ArrayType>()) {
                // For array type annotations like [U8; N], propagate the element type
                // so that array literals like [42; N] use the correct element type.
                const auto& arr_type = semantic_var_type->as<types::ArrayType>();
                if (arr_type.element && arr_type.element->is<types::PrimitiveType>()) {
                    const auto& elem_prim = arr_type.element->as<types::PrimitiveType>();
                    switch (elem_prim.kind) {
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
        }

        // If type annotation provides a struct type, temporarily set current_ret_type_
        // so struct literal codegen can use it for generic type resolution
        // (e.g., let w: Wrapper[I32, 3] = Wrapper { data: [1,2,3] })
        std::string saved_ret_type;
        if (is_struct && !var_type.empty()) {
            saved_ret_type = current_ret_type_;
            current_ret_type_ = var_type;
        }
        init_val = gen_expr(*let.init.value());
        if (!saved_ret_type.empty() || (is_struct && !var_type.empty())) {
            current_ret_type_ = saved_ret_type;
        }
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
                last_expr_type_.starts_with("<") || last_expr_type_.starts_with("[")) {
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
        std::string alloca_reg = emit_hoisted_alloca(struct_type);
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

    // Allocate on stack (hoisted to entry block for mem2reg optimization)
    std::string alloca_reg = emit_hoisted_alloca(var_type);
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
                    emit_line("  " + src_elem_ptr + " = getelementptr inbounds " + last_expr_type_ +
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
                    emit_line("  " + dst_elem_ptr + " = getelementptr inbounds " + var_type +
                              ", ptr " + alloca_reg + ", i32 0, i32 " + std::to_string(i));
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
    } else if (let.init.has_value() && (var_type.starts_with("{") || var_type.starts_with("["))) {
        // For tuple/array types without type annotation, infer semantic type from the initializer
        // This is needed for tuple field access and array method dispatch to work correctly
        semantic_type = infer_expr_type(*let.init.value());
    }
    // For struct variables from method calls (e.g., let r = x.transpose()),
    // use last_semantic_type_ for correct nested generic type dispatch.
    // infer_expr_type falls back to parse_mangled_type_string which gives a flat
    // (wrong) type for nested generics like Maybe[Outcome[Maybe[I32],Str]].
    if (!semantic_type && (var_type.starts_with("%struct.") || var_type.starts_with("%union."))) {
        if (last_semantic_type_) {
            // Validate last_semantic_type_ matches the LLVM type to avoid cross-function leaks.
            // E.g., last_semantic_type_ might be I64 from a previous function's as_nanos() call
            // but current var_type is %struct.Instant — the mismatch means it's stale.
            std::string mangled_check =
                var_type.starts_with("%struct.") ? var_type.substr(8) : var_type.substr(7);
            auto delim_check = mangled_check.find("__");
            std::string base_check = delim_check != std::string::npos
                                         ? mangled_check.substr(0, delim_check)
                                         : mangled_check;
            bool lst_matches = false;
            if (auto* named = std::get_if<types::NamedType>(&last_semantic_type_->kind)) {
                if (named->name == base_check) {
                    // Base name matches. Now verify type arguments match too.
                    // Without this check, a stale last_semantic_type_ from a previous
                    // expression (e.g., List[Violation]::new()) would be incorrectly
                    // applied to a variable of a different instantiation (e.g., List[Str]).
                    if (delim_check != std::string::npos && !named->type_args.empty()) {
                        // var_type has type args (e.g., "List__Str") — verify they match
                        std::string semantic_mangled =
                            mangle_struct_name(named->name, named->type_args);
                        lst_matches = (semantic_mangled == mangled_check);
                    } else if (delim_check == std::string::npos && named->type_args.empty()) {
                        // Neither has type args — simple non-generic type match
                        lst_matches = true;
                    } else {
                        // One has type args and the other doesn't — mismatch
                        lst_matches = false;
                    }
                }
            }
            if (lst_matches) {
                // Check for unresolved generic type args (e.g., K, V, T, E).
                // If any type_arg is a bare generic parameter name, the semantic type
                // is from an uninstantiated impl and we should fall through to parse
                // the LLVM type name instead (which has the concrete types).
                bool has_unresolved = false;
                if (auto* named = std::get_if<types::NamedType>(&last_semantic_type_->kind)) {
                    for (const auto& arg : named->type_args) {
                        if (auto* arg_named = std::get_if<types::NamedType>(&arg->kind)) {
                            const auto& n = arg_named->name;
                            // Single uppercase letter or known generic param names
                            if (n.size() <= 2 && std::isupper(n[0]) &&
                                (n.size() == 1 || std::isupper(n[1]))) {
                                has_unresolved = true;
                                break;
                            }
                        }
                    }
                }
                if (!has_unresolved) {
                    semantic_type = last_semantic_type_;
                }
            }
        }
        if (!semantic_type && let.init.has_value()) {
            semantic_type = infer_expr_type(*let.init.value());
            // Validate: if infer_expr_type returned a type that doesn't match the LLVM type,
            // discard it. E.g., for `let x = arr.partial_cmp(ref brr)`, infer returns
            // ArrayType [I32;3] (the receiver) but var_type is %struct.Maybe__Ordering.
            // In that case, fall through to parse the LLVM type name instead.
            if (semantic_type && var_type.starts_with("%struct.")) {
                std::string mangled = var_type.substr(8);
                auto delim = mangled.find("__");
                std::string base = delim != std::string::npos ? mangled.substr(0, delim) : mangled;
                bool matches = false;
                if (auto* named = std::get_if<types::NamedType>(&semantic_type->kind)) {
                    matches = (named->name == base);
                }
                if (!matches) {
                    semantic_type = nullptr; // discard wrong type, let fallback handle it
                }
            }
            // Also check for unresolved generic type args from infer_expr_type
            if (semantic_type) {
                if (auto* named = std::get_if<types::NamedType>(&semantic_type->kind)) {
                    for (const auto& arg : named->type_args) {
                        if (auto* an = std::get_if<types::NamedType>(&arg->kind)) {
                            if (an->name.size() <= 2 && std::isupper(an->name[0]) &&
                                (an->name.size() == 1 || std::isupper(an->name[1]))) {
                                semantic_type = nullptr;
                                break;
                            }
                        }
                    }
                }
            }
        }
        // Fallback: parse the LLVM struct/union type name to infer semantic type
        // This handles cases like %struct.Maybe__Ordering from inline method codegen
        // that doesn't set last_semantic_type_
        if (!semantic_type) {
            std::string mangled = var_type.starts_with("%struct.")  ? var_type.substr(8)
                                  : var_type.starts_with("%union.") ? var_type.substr(7)
                                                                    : var_type;
            auto delim = mangled.find("__");
            if (delim != std::string::npos) {
                std::string base = mangled.substr(0, delim);
                std::string arg_str = mangled.substr(delim + 2);
                // Split arg_str on "__" to handle multi-param generics
                // e.g., "HashMap__Str__I64" → base="HashMap", args=["Str", "I64"]
                std::vector<types::TypePtr> type_args;
                size_t pos = 0;
                while (pos < arg_str.size()) {
                    auto next = arg_str.find("__", pos);
                    std::string part = (next != std::string::npos) ? arg_str.substr(pos, next - pos)
                                                                   : arg_str.substr(pos);
                    auto arg_type = std::make_shared<types::Type>();
                    arg_type->kind = types::NamedType{part, "", {}};
                    type_args.push_back(arg_type);
                    if (next == std::string::npos)
                        break;
                    pos = next + 2;
                }
                semantic_type = std::make_shared<types::Type>();
                semantic_type->kind = types::NamedType{base, "", std::move(type_args)};
            } else {
                semantic_type = std::make_shared<types::Type>();
                semantic_type->kind = types::NamedType{mangled, "", {}};
            }
        }
    }
    // For ptr variables from method calls (e.g., let maybe_ptr = s.get_mut()),
    // use semantic_var_type or last_semantic_type_ for method dispatch
    if (!semantic_type && var_type == "ptr") {
        if (semantic_var_type) {
            semantic_type = semantic_var_type;
        } else if (last_semantic_type_) {
            semantic_type = last_semantic_type_;
        } else if (let.init.has_value()) {
            semantic_type = infer_expr_type(*let.init.value());
        }
    }
    VarInfo var_info{alloca_reg, var_type, semantic_type, std::nullopt};
    if (var_type == "{ ptr, ptr }") {
        var_info.is_capturing_closure = last_closure_is_capturing_;
    }
    locals_[var_name] = var_info;

    // Register for drop if type implements Drop.
    // IMPORTANT: suppress field drops for `let` bindings initialized from method calls
    // (like `let r = list.get(i)`). These return by-value copies whose inner handles
    // are shared with the collection. Dropping the copy's fields would free shared data,
    // causing heap corruption when the collection is later accessed.
    std::string type_name = extract_type_name_for_drop(var_type);
    bool suppress_field_drops = false;
    if (let.init.has_value()) {
        const auto& init_expr = *let.init.value();
        if (init_expr.is<parser::MethodCallExpr>() || init_expr.is<parser::CallExpr>()) {
            // Let bindings from method/function calls don't own their struct fields.
            // The caller (collection) retains ownership of inner heap handles.
            suppress_field_drops = true;
        }
    }
    if (suppress_field_drops) {
        // Only register direct Drop impl (not field-level drops)
        bool has_direct_drop = env_.type_implements(type_name, "Drop");
        if (has_direct_drop) {
            register_for_drop(var_name, alloca_reg, type_name, var_type);
        }
        // Skip registration — no field drops for non-owning let bindings
    } else {
        register_for_drop(var_name, alloca_reg, type_name, var_type);
    }

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

} // namespace tml::codegen
