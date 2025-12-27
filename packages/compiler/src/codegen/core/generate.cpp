// LLVM IR generator - Main generate function
// Handles: generate, infer_print_type

#include "tml/codegen/llvm_ir_gen.hpp"
#include <iomanip>
#include <set>

namespace tml::codegen {

auto LLVMIRGen::generate(const parser::Module& module) -> Result<std::string, std::vector<LLVMGenError>> {
    errors_.clear();
    output_.str("");
    type_defs_buffer_.str("");  // Clear type definitions buffer
    string_literals_.clear();
    temp_counter_ = 0;
    label_counter_ = 0;

    emit_header();
    emit_runtime_decls();
    emit_module_lowlevel_decls();

    // Save headers before generating imported module code
    std::string headers = output_.str();
    output_.str("");

    // Generate code for pure TML imported functions (like std::math)
    // This may add types to type_defs_buffer_
    emit_module_pure_tml_functions();

    // Save imported module code (functions)
    std::string imported_func_code = output_.str();
    output_.str("");

    // Now reassemble with types before functions
    output_ << headers;

    // Emit any generic types discovered during imported module processing
    std::string imported_type_defs = type_defs_buffer_.str();
    if (!imported_type_defs.empty()) {
        emit_line("; Generic types from imported modules");
        output_ << imported_type_defs;
    }
    type_defs_buffer_.str("");  // Clear for main module processing

    // Emit imported module functions AFTER their type dependencies
    output_ << imported_func_code;

    // First pass: collect const declarations and struct/enum declarations
    for (const auto& decl : module.decls) {
        if (decl->is<parser::ConstDecl>()) {
            const auto& const_decl = decl->as<parser::ConstDecl>();
            // For now, only support literal constants
            if (const_decl.value->is<parser::LiteralExpr>()) {
                const auto& lit = const_decl.value->as<parser::LiteralExpr>();
                std::string value;
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    value = std::to_string(lit.token.int_value().value);
                } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    value = (lit.token.lexeme == "true") ? "1" : "0";
                }
                global_constants_[const_decl.name] = value;
            }
        } else if (decl->is<parser::StructDecl>()) {
            gen_struct_decl(decl->as<parser::StructDecl>());
        } else if (decl->is<parser::EnumDecl>()) {
            gen_enum_decl(decl->as<parser::EnumDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            // Register impl block for vtable generation
            register_impl(&decl->as<parser::ImplDecl>());
        } else if (decl->is<parser::TraitDecl>()) {
            // Register trait/behavior declaration for default implementations
            const auto& trait_decl = decl->as<parser::TraitDecl>();
            trait_decls_[trait_decl.name] = &trait_decl;
        }
    }

    // Generate any pending generic instantiations collected during first pass
    // This happens after structs/enums are registered but before function codegen
    generate_pending_instantiations();

    // Emit dyn types for all registered behaviors before function generation
    for (const auto& [key, vtable_name] : vtables_) {
        // key is "TypeName::BehaviorName", extract behavior name
        size_t pos = key.find("::");
        if (pos != std::string::npos) {
            std::string behavior_name = key.substr(pos + 2);
            emit_dyn_type(behavior_name);
        }
    }

    // Buffer function code separately so we can emit type instantiations before functions
    std::stringstream func_output;
    std::stringstream saved_output;
    saved_output.str(output_.str());  // Save current output (headers, type defs)
    output_.str("");  // Clear for function code

    // Second pass: generate function declarations (into temp buffer)
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            gen_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            // Generate impl methods as named functions inline
            const auto& impl = decl->as<parser::ImplDecl>();
            std::string type_name;
            if (impl.self_type->kind.index() == 0) {  // NamedType
                const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            }
            if (!type_name.empty()) {
                for (const auto& method : impl.methods) {
                    // Generate method with mangled name TypeName_MethodName
                    std::string method_name = type_name + "_" + method.name;
                    current_func_ = method_name;
                    current_impl_type_ = type_name;  // Track impl self type for 'this' access
                    locals_.clear();
                    block_terminated_ = false;

                    // Determine return type
                    std::string ret_type = "void";
                    if (method.return_type.has_value()) {
                        ret_type = llvm_type_ptr(*method.return_type);
                    }
                    current_ret_type_ = ret_type;

                    // Build parameter list (including 'this')
                    std::string params;
                    std::string param_types;
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        if (i > 0) {
                            params += ", ";
                            param_types += ", ";
                        }
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        if (method.params[i].pattern && method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                        } else {
                            param_name = "_anon";
                        }
                        // Substitute 'This' type with the actual impl type
                        if (param_name == "this" && param_type.find("This") != std::string::npos) {
                            param_type = "ptr";  // 'this' is always a pointer to the struct
                        }
                        params += param_type + " %" + param_name;
                        param_types += param_type;
                    }

                    // Register function
                    std::string func_type = ret_type + " (" + param_types + ")";
                    functions_[method_name] = FuncInfo{
                        "@tml_" + method_name,
                        func_type,
                        ret_type
                    };

                    // Generate function
                    emit_line("");
                    emit_line("define internal " + ret_type + " @tml_" + method_name + "(" + params + ") #0 {");
                    emit_line("entry:");

                    // Register params in locals
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        if (method.params[i].pattern && method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                        } else {
                            param_name = "_anon";
                        }
                        // Substitute 'This' type with ptr for 'this' param
                        if (param_name == "this" && param_type.find("This") != std::string::npos) {
                            param_type = "ptr";
                        }

                        // 'this' is already a pointer parameter, don't create alloca for it
                        if (param_name == "this") {
                            locals_[param_name] = VarInfo{"%" + param_name, param_type, nullptr};
                        } else {
                            std::string alloca_reg = fresh_reg();
                            emit_line("  " + alloca_reg + " = alloca " + param_type);
                            emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
                            locals_[param_name] = VarInfo{alloca_reg, param_type, nullptr};
                        }
                    }

                    // Generate body
                    if (method.body.has_value()) {
                        gen_block(*method.body);
                        if (!block_terminated_) {
                            if (ret_type == "void") {
                                emit_line("  ret void");
                            } else {
                                emit_line("  ret " + ret_type + " 0");
                            }
                        }
                    } else {
                        if (ret_type == "void") {
                            emit_line("  ret void");
                        } else {
                            emit_line("  ret " + ret_type + " 0");
                        }
                    }
                    emit_line("}");
                    current_impl_type_.clear();  // Clear impl type context
                }

                // Generate default implementations for missing methods
                if (impl.trait_path && !impl.trait_path->segments.empty()) {
                    std::string trait_name = impl.trait_path->segments.back();
                    auto trait_it = trait_decls_.find(trait_name);
                    if (trait_it != trait_decls_.end()) {
                        const auto* trait_decl = trait_it->second;

                        // Collect method names that impl provides
                        std::set<std::string> impl_method_names;
                        for (const auto& m : impl.methods) {
                            impl_method_names.insert(m.name);
                        }

                        // Generate default implementations for missing methods
                        for (const auto& trait_method : trait_decl->methods) {
                            // Skip if impl provides this method
                            if (impl_method_names.count(trait_method.name) > 0) continue;

                            // Skip if trait method has no default implementation
                            if (!trait_method.body.has_value()) continue;

                            // Generate default implementation with type substitution
                            std::string method_name = type_name + "_" + trait_method.name;
                            current_func_ = method_name;
                            current_impl_type_ = type_name;
                            locals_.clear();
                            block_terminated_ = false;

                            // Determine return type
                            std::string ret_type = "void";
                            if (trait_method.return_type.has_value()) {
                                ret_type = llvm_type_ptr(*trait_method.return_type);
                                // Substitute 'This' with actual type
                                if (ret_type.find("This") != std::string::npos) {
                                    ret_type = "%struct." + type_name;
                                }
                            }
                            current_ret_type_ = ret_type;

                            // Build parameter list
                            std::string params;
                            std::string param_types;
                            for (size_t i = 0; i < trait_method.params.size(); ++i) {
                                if (i > 0) {
                                    params += ", ";
                                    param_types += ", ";
                                }
                                std::string param_type = llvm_type_ptr(trait_method.params[i].type);
                                std::string param_name;
                                if (trait_method.params[i].pattern && trait_method.params[i].pattern->is<parser::IdentPattern>()) {
                                    param_name = trait_method.params[i].pattern->as<parser::IdentPattern>().name;
                                } else {
                                    param_name = "_anon";
                                }
                                // Substitute 'This' type with ptr for 'this' param
                                if (param_name == "this" && param_type.find("This") != std::string::npos) {
                                    param_type = "ptr";
                                }
                                params += param_type + " %" + param_name;
                                param_types += param_type;
                            }

                            // Register function
                            std::string func_type = ret_type + " (" + param_types + ")";
                            functions_[method_name] = FuncInfo{
                                "@tml_" + method_name,
                                func_type,
                                ret_type
                            };

                            // Generate function
                            emit_line("");
                            emit_line("; Default implementation from behavior " + trait_name);
                            emit_line("define internal " + ret_type + " @tml_" + method_name + "(" + params + ") #0 {");
                            emit_line("entry:");

                            // Register params in locals
                            for (size_t i = 0; i < trait_method.params.size(); ++i) {
                                std::string param_type = llvm_type_ptr(trait_method.params[i].type);
                                std::string param_name;
                                if (trait_method.params[i].pattern && trait_method.params[i].pattern->is<parser::IdentPattern>()) {
                                    param_name = trait_method.params[i].pattern->as<parser::IdentPattern>().name;
                                } else {
                                    param_name = "_anon";
                                }

                                // Create semantic type for this parameter
                                types::TypePtr semantic_type = nullptr;
                                if (param_name == "this" && param_type.find("This") != std::string::npos) {
                                    param_type = "ptr";
                                    // Create semantic type as the concrete impl type
                                    semantic_type = std::make_shared<types::Type>();
                                    semantic_type->kind = types::NamedType{type_name, "", {}};
                                }

                                // 'this' is already a pointer parameter, don't create alloca for it
                                if (param_name == "this") {
                                    locals_[param_name] = VarInfo{"%" + param_name, param_type, semantic_type};
                                } else {
                                    std::string alloca_reg = fresh_reg();
                                    emit_line("  " + alloca_reg + " = alloca " + param_type);
                                    emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
                                    locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type};
                                }
                            }

                            // Generate body
                            gen_block(*trait_method.body);
                            if (!block_terminated_) {
                                if (ret_type == "void") {
                                    emit_line("  ret void");
                                } else {
                                    emit_line("  ret " + ret_type + " 0");
                                }
                            }
                            emit_line("}");
                            current_impl_type_.clear();
                        }
                    }
                }
            }
        }
    }

    // Save function code (non-generic functions)
    func_output.str(output_.str());
    output_.str("");

    // Generate pending generic instantiations (types go to type_defs_buffer_, funcs go to output_)
    generate_pending_instantiations();

    // Save generic function code
    std::stringstream generic_func_output;
    generic_func_output.str(output_.str());
    output_.str("");

    // Now reassemble in correct order: headers + types + all functions
    // 1. Headers
    output_ << saved_output.str();

    // 2. Type definitions (from type_defs_buffer_) - MUST come before functions
    std::string type_defs = type_defs_buffer_.str();
    if (!type_defs.empty()) {
        emit_line("; Generic type instantiations");
        output_ << type_defs;
    }
    emit_line("");

    // 3. Non-generic functions
    output_ << func_output.str();

    // 4. Generic functions (instantiated functions)
    output_ << generic_func_output.str();

    // Emit generated closure functions
    for (const auto& closure_func : module_functions_) {
        emit(closure_func);
    }

    // Emit vtables for trait objects (dyn dispatch)
    emit_vtables();

    // Emit string constants at the end (they were collected during codegen)
    emit_string_constants();

    // Collect test and benchmark functions (decorated with @test and @bench)
    std::vector<std::string> test_functions;
    std::vector<std::string> bench_functions;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            for (const auto& decorator : func.decorators) {
                if (decorator.name == "test") {
                    test_functions.push_back(func.name);
                    break;
                } else if (decorator.name == "bench") {
                    bench_functions.push_back(func.name);
                    break;
                }
            }
        }
    }

    // Generate main entry point
    bool has_user_main = false;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>() && decl->as<parser::FuncDecl>().name == "main") {
            has_user_main = true;
            break;
        }
    }

    if (!bench_functions.empty()) {
        // Generate benchmark runner main
        // Note: time functions are always declared in preamble
        emit_line("; Auto-generated benchmark runner");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");

        int bench_num = 0;
        std::string prev_block = "entry";
        for (const auto& bench_name : bench_functions) {
            std::string bench_fn = "@tml_" + bench_name;

            // Get start time
            std::string start_time = "%bench_start_" + std::to_string(bench_num);
            emit_line("  " + start_time + " = call i64 @time_us()");

            // Run benchmark 1000 iterations
            std::string iter_var = "%bench_iter_" + std::to_string(bench_num);
            std::string loop_header = "bench_loop_header_" + std::to_string(bench_num);
            std::string loop_body = "bench_loop_body_" + std::to_string(bench_num);
            std::string loop_end = "bench_loop_end_" + std::to_string(bench_num);

            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_header + ":");
            emit_line("  " + iter_var + " = phi i32 [ 0, %" + prev_block + " ], [ " + iter_var + "_next, %" + loop_body + " ]");
            std::string cmp_var = "%bench_cmp_" + std::to_string(bench_num);
            emit_line("  " + cmp_var + " = icmp slt i32 " + iter_var + ", 1000");
            emit_line("  br i1 " + cmp_var + ", label %" + loop_body + ", label %" + loop_end);
            emit_line("");
            emit_line(loop_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + iter_var + "_next = add i32 " + iter_var + ", 1");
            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_end + ":");

            // Get end time and calculate duration
            std::string end_time = "%bench_end_" + std::to_string(bench_num);
            std::string duration = "%bench_duration_" + std::to_string(bench_num);
            emit_line("  " + end_time + " = call i64 @time_us()");
            emit_line("  " + duration + " = sub i64 " + end_time + ", " + start_time);

            // Calculate average (duration / 1000)
            std::string avg_time = "%bench_avg_" + std::to_string(bench_num);
            emit_line("  " + avg_time + " = sdiv i64 " + duration + ", 1000");
            emit_line("");

            prev_block = loop_end;
            bench_num++;
        }

        emit_line("  ret i32 0");
        emit_line("}");
    } else if (!test_functions.empty()) {
        // Generate test runner main
        // @test functions return Unit (void) - assertions call exit(1) on failure
        // If a test function returns, it passed
        emit_line("; Auto-generated test runner");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");

        for (const auto& test_name : test_functions) {
            std::string test_fn = "@tml_" + test_name;
            // Call test function (void return) - if it returns, test passed
            // Assertions inside will call exit(1) on failure
            emit_line("  call void " + test_fn + "()");
        }

        // Print coverage report if enabled
        if (options_.coverage_enabled) {
            emit_line("  call void @print_coverage_report()");
        }

        // All tests passed (if we got here, no assertion failed)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (has_user_main) {
        // Standard main wrapper for user-defined main
        emit_line("; Entry point");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");
        emit_line("  %ret = call i32 @tml_main()");
        emit_line("  ret i32 %ret");
        emit_line("}");
    }

    // Emit function attributes for optimization
    emit_line("");
    emit_line("; Function attributes for optimization");
    emit_line("attributes #0 = { nounwind mustprogress willreturn }");

    if (!errors_.empty()) {
        return errors_;
    }

    return output_.str();
}

// Infer print argument type from expression
auto LLVMIRGen::infer_print_type(const parser::Expr& expr) -> PrintArgType {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        switch (lit.token.kind) {
            case lexer::TokenKind::IntLiteral: return PrintArgType::Int;
            case lexer::TokenKind::FloatLiteral: return PrintArgType::Float;
            case lexer::TokenKind::BoolLiteral: return PrintArgType::Bool;
            case lexer::TokenKind::StringLiteral: return PrintArgType::Str;
            default: return PrintArgType::Unknown;
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        switch (bin.op) {
            case parser::BinaryOp::Add:
            case parser::BinaryOp::Sub:
            case parser::BinaryOp::Mul:
            case parser::BinaryOp::Div:
            case parser::BinaryOp::Mod:
                // Check if operands are float
                if (infer_print_type(*bin.left) == PrintArgType::Float ||
                    infer_print_type(*bin.right) == PrintArgType::Float) {
                    return PrintArgType::Float;
                }
                return PrintArgType::Int;
            case parser::BinaryOp::Eq:
            case parser::BinaryOp::Ne:
            case parser::BinaryOp::Lt:
            case parser::BinaryOp::Gt:
            case parser::BinaryOp::Le:
            case parser::BinaryOp::Ge:
            case parser::BinaryOp::And:
            case parser::BinaryOp::Or:
                return PrintArgType::Bool;
            default:
                return PrintArgType::Int;
        }
    }
    if (expr.is<parser::UnaryExpr>()) {
        const auto& un = expr.as<parser::UnaryExpr>();
        if (un.op == parser::UnaryOp::Not) return PrintArgType::Bool;
        if (un.op == parser::UnaryOp::Neg) {
            // Check if operand is float
            if (infer_print_type(*un.operand) == PrintArgType::Float) {
                return PrintArgType::Float;
            }
            return PrintArgType::Int;
        }
    }
    if (expr.is<parser::IdentExpr>()) {
        // For identifiers, we need to check the variable type
        // For now, default to Unknown (will be checked by caller)
        return PrintArgType::Unknown;
    }
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        // Check for known I64-returning functions
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& fn_name = call.callee->as<parser::IdentExpr>().name;
            if (fn_name == "time_us" || fn_name == "time_ns") {
                return PrintArgType::I64;
            }
        }
        return PrintArgType::Int; // Assume functions return int
    }
    return PrintArgType::Unknown;
}

} // namespace tml::codegen
