// LLVM IR generator - Main generate function
// Handles: generate, infer_print_type

#include "codegen/llvm_ir_gen.hpp"

#include <iomanip>
#include <set>

namespace tml::codegen {

auto LLVMIRGen::generate(const parser::Module& module)
    -> Result<std::string, std::vector<LLVMGenError>> {
    errors_.clear();
    output_.str("");
    type_defs_buffer_.str(""); // Clear type definitions buffer
    string_literals_.clear();
    temp_counter_ = 0;
    label_counter_ = 0;

    // Register builtin enums
    // Ordering enum: Less=0, Equal=1, Greater=2
    enum_variants_["Ordering::Less"] = 0;
    enum_variants_["Ordering::Equal"] = 1;
    enum_variants_["Ordering::Greater"] = 2;

    // Register builtin generic enums: Maybe[T], Outcome[T, E]
    // These need to be stored in builtin_enum_decls_ to keep the AST alive
    {
        // Maybe[T] { Just(T), Nothing }
        auto maybe_decl = std::make_unique<parser::EnumDecl>();
        maybe_decl->name = "Maybe";
        maybe_decl->generics.push_back(parser::GenericParam{"T"});

        // Just(T) variant
        parser::EnumVariant just_variant;
        just_variant.name = "Just";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}}};
        std::vector<parser::TypePtr> just_fields;
        just_fields.push_back(std::move(t_type));
        just_variant.tuple_fields = std::move(just_fields);
        maybe_decl->variants.push_back(std::move(just_variant));

        // Nothing variant
        parser::EnumVariant nothing_variant;
        nothing_variant.name = "Nothing";
        maybe_decl->variants.push_back(std::move(nothing_variant));

        pending_generic_enums_["Maybe"] = maybe_decl.get();
        builtin_enum_decls_.push_back(std::move(maybe_decl));
    }

    {
        // Outcome[T, E] { Ok(T), Err(E) }
        auto outcome_decl = std::make_unique<parser::EnumDecl>();
        outcome_decl->name = "Outcome";
        outcome_decl->generics.push_back(parser::GenericParam{"T"});
        outcome_decl->generics.push_back(parser::GenericParam{"E"});

        // Ok(T) variant
        parser::EnumVariant ok_variant;
        ok_variant.name = "Ok";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}}};
        std::vector<parser::TypePtr> ok_fields;
        ok_fields.push_back(std::move(t_type));
        ok_variant.tuple_fields = std::move(ok_fields);
        outcome_decl->variants.push_back(std::move(ok_variant));

        // Err(E) variant
        parser::EnumVariant err_variant;
        err_variant.name = "Err";
        auto e_type = std::make_unique<parser::Type>();
        e_type->kind = parser::NamedType{parser::TypePath{{"E"}}};
        std::vector<parser::TypePtr> err_fields;
        err_fields.push_back(std::move(e_type));
        err_variant.tuple_fields = std::move(err_fields);
        outcome_decl->variants.push_back(std::move(err_variant));

        pending_generic_enums_["Outcome"] = outcome_decl.get();
        builtin_enum_decls_.push_back(std::move(outcome_decl));
    }

    {
        // Poll[T] { Ready(T), Pending }
        auto poll_decl = std::make_unique<parser::EnumDecl>();
        poll_decl->name = "Poll";
        poll_decl->generics.push_back(parser::GenericParam{"T"});

        // Ready(T) variant
        parser::EnumVariant ready_variant;
        ready_variant.name = "Ready";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}}};
        std::vector<parser::TypePtr> ready_fields;
        ready_fields.push_back(std::move(t_type));
        ready_variant.tuple_fields = std::move(ready_fields);
        poll_decl->variants.push_back(std::move(ready_variant));

        // Pending variant
        parser::EnumVariant pending_variant;
        pending_variant.name = "Pending";
        poll_decl->variants.push_back(std::move(pending_variant));

        pending_generic_enums_["Poll"] = poll_decl.get();
        builtin_enum_decls_.push_back(std::move(poll_decl));
    }

    emit_header();
    emit_debug_info_header(); // Initialize debug info metadata
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
    type_defs_buffer_.str(""); // Clear for main module processing

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
                } else if (lit.token.kind == lexer::TokenKind::NullLiteral) {
                    value = "null";
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

            // Collect associated constants from impl block
            const auto& impl = decl->as<parser::ImplDecl>();
            std::string type_name;
            if (impl.self_type->kind.index() == 0) { // NamedType
                const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            }
            if (!type_name.empty()) {
                for (const auto& const_decl : impl.constants) {
                    std::string qualified_name = type_name + "::" + const_decl.name;
                    // For now, only support literal constants
                    if (const_decl.value->is<parser::LiteralExpr>()) {
                        const auto& lit = const_decl.value->as<parser::LiteralExpr>();
                        std::string value;
                        if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                            value = std::to_string(lit.token.int_value().value);
                        } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                            value = (lit.token.lexeme == "true") ? "1" : "0";
                        } else if (lit.token.kind == lexer::TokenKind::NullLiteral) {
                            value = "null";
                        }
                        if (!value.empty()) {
                            global_constants_[qualified_name] = value;
                        }
                    }
                    // Struct literal constants will need special handling during codegen
                }
            }
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
    saved_output.str(output_.str()); // Save current output (headers, type defs)
    output_.str("");                 // Clear for function code

    // Second pass: generate function declarations (into temp buffer)
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            gen_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            // Generate impl methods as named functions inline
            const auto& impl = decl->as<parser::ImplDecl>();
            std::string type_name;
            if (impl.self_type->kind.index() == 0) { // NamedType
                const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            }
            if (!type_name.empty()) {
                // Skip builtin types that have hard-coded implementations in method.cpp
                if (type_name == "File" || type_name == "Path" || type_name == "List" ||
                    type_name == "HashMap" || type_name == "Buffer") {
                    continue;
                }
                // Skip generic impl blocks - they will be instantiated when methods are called
                // (e.g., impl[T] Container[T] { ... } is not generated directly)
                if (!impl.generics.empty()) {
                    // Store the generic impl block for later instantiation
                    pending_generic_impls_[type_name] = &impl;
                    continue;
                }
                // Populate associated types from impl type_bindings
                current_associated_types_.clear();
                for (const auto& binding : impl.type_bindings) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*binding.type, {});
                    current_associated_types_[binding.name] = resolved;
                }
                for (const auto& method : impl.methods) {
                    // Generate method with mangled name TypeName_MethodName
                    std::string method_name = type_name + "_" + method.name;
                    current_func_ = method_name;
                    current_impl_type_ = type_name; // Track impl self type for 'this' access
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
                    std::vector<std::string> param_types_vec;
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        if (i > 0) {
                            params += ", ";
                            param_types += ", ";
                        }
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        if (method.params[i].pattern &&
                            method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                        } else {
                            param_name = "_anon";
                        }
                        // Substitute 'This' type with the actual impl type
                        if (param_name == "this" && param_type.find("This") != std::string::npos) {
                            param_type = "ptr"; // 'this' is always a pointer to the struct
                        }
                        params += param_type + " %" + param_name;
                        param_types += param_type;
                        param_types_vec.push_back(param_type);
                    }

                    // Register function
                    std::string func_type = ret_type + " (" + param_types + ")";
                    functions_[method_name] =
                        FuncInfo{"@tml_" + method_name, func_type, ret_type, param_types_vec};

                    // Generate function
                    emit_line("");
                    emit_line("define internal " + ret_type + " @tml_" + method_name + "(" +
                              params + ") #0 {");
                    emit_line("entry:");

                    // Register params in locals
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        if (method.params[i].pattern &&
                            method.params[i].pattern->is<parser::IdentPattern>()) {
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
                            locals_[param_name] =
                                VarInfo{"%" + param_name, param_type, nullptr, std::nullopt};
                        } else {
                            std::string alloca_reg = fresh_reg();
                            emit_line("  " + alloca_reg + " = alloca " + param_type);
                            emit_line("  store " + param_type + " %" + param_name + ", ptr " +
                                      alloca_reg);
                            locals_[param_name] =
                                VarInfo{alloca_reg, param_type, nullptr, std::nullopt};
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
                    current_impl_type_.clear(); // Clear impl type context
                }

                // Generate default implementations for missing methods
                std::string trait_name;
                if (impl.trait_type && impl.trait_type->is<parser::NamedType>()) {
                    const auto& named = impl.trait_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        trait_name = named.path.segments.back();
                    }
                }
                if (!trait_name.empty()) {
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
                            if (impl_method_names.count(trait_method.name) > 0)
                                continue;

                            // Skip if trait method has no default implementation
                            if (!trait_method.body.has_value())
                                continue;

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
                            std::vector<std::string> param_types_vec;
                            for (size_t i = 0; i < trait_method.params.size(); ++i) {
                                if (i > 0) {
                                    params += ", ";
                                    param_types += ", ";
                                }
                                std::string param_type = llvm_type_ptr(trait_method.params[i].type);
                                std::string param_name;
                                if (trait_method.params[i].pattern &&
                                    trait_method.params[i].pattern->is<parser::IdentPattern>()) {
                                    param_name = trait_method.params[i]
                                                     .pattern->as<parser::IdentPattern>()
                                                     .name;
                                } else {
                                    param_name = "_anon";
                                }
                                // Substitute 'This' type with ptr for 'this' param
                                if (param_name == "this" &&
                                    param_type.find("This") != std::string::npos) {
                                    param_type = "ptr";
                                }
                                params += param_type + " %" + param_name;
                                param_types += param_type;
                                param_types_vec.push_back(param_type);
                            }

                            // Register function
                            std::string func_type = ret_type + " (" + param_types + ")";
                            functions_[method_name] = FuncInfo{"@tml_" + method_name, func_type,
                                                               ret_type, param_types_vec};

                            // Generate function
                            emit_line("");
                            emit_line("; Default implementation from behavior " + trait_name);
                            emit_line("define internal " + ret_type + " @tml_" + method_name + "(" +
                                      params + ") #0 {");
                            emit_line("entry:");

                            // Register params in locals
                            for (size_t i = 0; i < trait_method.params.size(); ++i) {
                                std::string param_type = llvm_type_ptr(trait_method.params[i].type);
                                std::string param_name;
                                if (trait_method.params[i].pattern &&
                                    trait_method.params[i].pattern->is<parser::IdentPattern>()) {
                                    param_name = trait_method.params[i]
                                                     .pattern->as<parser::IdentPattern>()
                                                     .name;
                                } else {
                                    param_name = "_anon";
                                }

                                // Create semantic type for this parameter
                                types::TypePtr semantic_type = nullptr;
                                if (param_name == "this" &&
                                    param_type.find("This") != std::string::npos) {
                                    param_type = "ptr";
                                    // Create semantic type as the concrete impl type
                                    semantic_type = std::make_shared<types::Type>();
                                    semantic_type->kind = types::NamedType{type_name, "", {}};
                                }

                                // 'this' is already a pointer parameter, don't create alloca for it
                                if (param_name == "this") {
                                    locals_[param_name] = VarInfo{"%" + param_name, param_type,
                                                                  semantic_type, std::nullopt};
                                } else {
                                    std::string alloca_reg = fresh_reg();
                                    emit_line("  " + alloca_reg + " = alloca " + param_type);
                                    emit_line("  store " + param_type + " %" + param_name +
                                              ", ptr " + alloca_reg);
                                    locals_[param_name] = VarInfo{alloca_reg, param_type,
                                                                  semantic_type, std::nullopt};
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

    // Collect test, benchmark, and fuzz functions BEFORE emitting string constants
    // so we can pre-register expected panic message strings
    struct TestInfo {
        std::string name;
        bool should_panic = false;
        std::string expected_panic_message;     // Empty means any panic is fine
        std::string expected_panic_message_str; // LLVM string constant reference
    };
    std::vector<TestInfo> test_functions;
    std::vector<std::string> fuzz_functions;
    struct BenchInfo {
        std::string name;
        int64_t iterations = 1000; // Default iterations
    };
    std::vector<BenchInfo> bench_functions;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            bool is_test = false;
            bool should_panic = false;
            std::string expected_panic_message;

            for (const auto& decorator : func.decorators) {
                if (decorator.name == "test") {
                    is_test = true;
                } else if (decorator.name == "should_panic") {
                    should_panic = true;
                    // Check for expected message: @should_panic(expected = "message")
                    for (const auto& arg : decorator.args) {
                        if (arg->is<parser::BinaryExpr>()) {
                            // Handle named argument: expected = "message"
                            const auto& bin = arg->as<parser::BinaryExpr>();
                            if (bin.op == parser::BinaryOp::Assign &&
                                bin.left->is<parser::IdentExpr>() &&
                                bin.right->is<parser::LiteralExpr>()) {
                                const auto& ident = bin.left->as<parser::IdentExpr>();
                                const auto& lit = bin.right->as<parser::LiteralExpr>();
                                if (ident.name == "expected" &&
                                    lit.token.kind == lexer::TokenKind::StringLiteral) {
                                    expected_panic_message = lit.token.string_value().value;
                                }
                            }
                        } else if (arg->is<parser::LiteralExpr>()) {
                            // Also support @should_panic("message") without named argument
                            const auto& lit = arg->as<parser::LiteralExpr>();
                            if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                                expected_panic_message = lit.token.string_value().value;
                            }
                        }
                    }
                } else if (decorator.name == "bench") {
                    BenchInfo info;
                    info.name = func.name;
                    // Check for iterations argument: @bench(1000) or @bench(iterations=1000)
                    if (!decorator.args.empty()) {
                        const auto& arg = *decorator.args[0];
                        if (arg.is<parser::LiteralExpr>()) {
                            const auto& lit = arg.as<parser::LiteralExpr>();
                            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                                info.iterations = static_cast<int64_t>(lit.token.int_value().value);
                            }
                        }
                    }
                    bench_functions.push_back(info);
                } else if (decorator.name == "fuzz") {
                    fuzz_functions.push_back(func.name);
                }
            }

            if (is_test) {
                TestInfo info;
                info.name = func.name;
                info.should_panic = should_panic;
                info.expected_panic_message = expected_panic_message;
                // Pre-register the expected message string BEFORE emit_string_constants
                if (!expected_panic_message.empty()) {
                    info.expected_panic_message_str = add_string_literal(expected_panic_message);
                }
                test_functions.push_back(info);
            }
        }
    }

    // Pre-register coverage output file string if needed (before emitting string constants)
    std::string coverage_output_str;
    if (options_.coverage_enabled && !options_.coverage_output_file.empty()) {
        coverage_output_str = add_string_literal(options_.coverage_output_file);
    }

    // Emit string constants at the end (they were collected during codegen)
    emit_string_constants();

    // Generate main entry point
    bool has_user_main = false;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>() && decl->as<parser::FuncDecl>().name == "main") {
            has_user_main = true;
            break;
        }
    }

    if (!bench_functions.empty()) {
        // Generate benchmark runner main with proper output
        // Note: time functions are always declared in preamble
        emit_line("; Auto-generated benchmark runner");
        emit_line("");

        // Add format strings for benchmark output
        // String lengths: \0A = 1 byte, \00 = 1 byte (null terminator)
        emit_line(
            "@.bench.header = private constant [23 x i8] c\"\\0A  Running benchmarks\\0A\\00\"");
        emit_line("@.bench.name = private constant [16 x i8] c\"  + bench %-20s\\00\"");
        emit_line("@.bench.time = private constant [19 x i8] c\" ... %lld ns/iter\\0A\\00\"");
        emit_line("@.bench.summary = private constant [30 x i8] c\"\\0A  %d benchmark(s) "
                  "completed\\0A\\00\"");

        // Add string constants for benchmark names
        int idx = 0;
        for (const auto& bench_info : bench_functions) {
            std::string name_const = "@.bench.fn." + std::to_string(idx);
            size_t name_len = bench_info.name.size() + 1;
            emit_line(name_const + " = private constant [" + std::to_string(name_len) +
                      " x i8] c\"" + bench_info.name + "\\00\"");
            idx++;
        }
        emit_line("");

        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");

        // Print benchmark header
        emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.header)");
        emit_line("");

        int bench_num = 0;
        std::string prev_block = "entry";
        for (const auto& bench_info : bench_functions) {
            std::string bench_fn = "@tml_" + bench_info.name;
            std::string n = std::to_string(bench_num);
            std::string name_const = "@.bench.fn." + n;
            std::string iterations_str = std::to_string(bench_info.iterations);

            // Print benchmark name
            emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.name, ptr " + name_const + ")");

            // Warmup: Run 10 iterations to warm up caches
            std::string warmup_var = "%warmup_" + n;
            std::string warmup_header = "warmup_header_" + n;
            std::string warmup_body = "warmup_body_" + n;
            std::string warmup_end = "warmup_end_" + n;

            emit_line("  br label %" + warmup_header);
            emit_line("");
            emit_line(warmup_header + ":");
            emit_line("  " + warmup_var + " = phi i64 [ 0, %" + prev_block + " ], [ " + warmup_var +
                      "_next, %" + warmup_body + " ]");
            emit_line("  %warmup_cmp_" + n + " = icmp slt i64 " + warmup_var + ", 10");
            emit_line("  br i1 %warmup_cmp_" + n + ", label %" + warmup_body + ", label %" +
                      warmup_end);
            emit_line("");
            emit_line(warmup_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + warmup_var + "_next = add i64 " + warmup_var + ", 1");
            emit_line("  br label %" + warmup_header);
            emit_line("");
            emit_line(warmup_end + ":");

            // Get start time (nanoseconds for precision)
            std::string start_time = "%bench_start_" + n;
            emit_line("  " + start_time + " = call i64 @time_ns()");

            // Run benchmark with configured iterations (default 1000)
            std::string iter_var = "%bench_iter_" + n;
            std::string loop_header = "bench_loop_header_" + n;
            std::string loop_body = "bench_loop_body_" + n;
            std::string loop_end = "bench_loop_end_" + n;

            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_header + ":");
            emit_line("  " + iter_var + " = phi i64 [ 0, %" + warmup_end + " ], [ " + iter_var +
                      "_next, %" + loop_body + " ]");
            std::string cmp_var = "%bench_cmp_" + n;
            emit_line("  " + cmp_var + " = icmp slt i64 " + iter_var + ", " + iterations_str);
            emit_line("  br i1 " + cmp_var + ", label %" + loop_body + ", label %" + loop_end);
            emit_line("");
            emit_line(loop_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + iter_var + "_next = add i64 " + iter_var + ", 1");
            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_end + ":");

            // Get end time and calculate duration
            std::string end_time = "%bench_end_" + n;
            std::string duration = "%bench_duration_" + n;
            emit_line("  " + end_time + " = call i64 @time_ns()");
            emit_line("  " + duration + " = sub i64 " + end_time + ", " + start_time);

            // Calculate average (duration / iterations)
            std::string avg_time = "%bench_avg_" + n;
            emit_line("  " + avg_time + " = sdiv i64 " + duration + ", " + iterations_str);

            // Print benchmark time
            emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.time, i64 " + avg_time + ")");
            emit_line("");

            prev_block = loop_end;
            bench_num++;
        }

        // Print summary
        emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.summary, i32 " +
                  std::to_string(bench_num) + ")");
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (options_.generate_fuzz_entry && !fuzz_functions.empty()) {
        // Generate fuzz target entry point for fuzzing
        // The fuzz target receives (ptr data, i64 len) and calls @fuzz functions
        emit_line("; Auto-generated fuzz target entry point");
        emit_line("");

#ifdef _WIN32
        emit_line("define dllexport i32 @tml_fuzz_target(ptr %data, i64 %len) {");
#else
        emit_line("define i32 @tml_fuzz_target(ptr %data, i64 %len) {");
#endif
        emit_line("entry:");

        // Call each @fuzz function with the input data
        // Fuzz functions should have signature: func fuzz_name(data: Ptr[U8], len: U64)
        for (const auto& fuzz_name : fuzz_functions) {
            std::string fuzz_fn = "@tml_" + fuzz_name;
            // Look up the function's return type from functions_ map
            auto it = functions_.find(fuzz_name);
            if (it != functions_.end()) {
                // Check if function takes (ptr, i64) parameters
                if (it->second.param_types.size() >= 2) {
                    emit_line("  call void " + fuzz_fn + "(ptr %data, i64 %len)");
                } else {
                    // Function doesn't take data parameters, just call it
                    emit_line("  call void " + fuzz_fn + "()");
                }
            } else {
                // Fallback - assume void function
                emit_line("  call void " + fuzz_fn + "()");
            }
        }

        // Return 0 for success (crash will never reach here)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (!test_functions.empty()) {
        // Generate test runner main (or DLL entry point)
        // @test functions can return I32 (0 for success) or Unit
        // Assertions inside will call panic() on failure which doesn't return
        emit_line("; Auto-generated test runner");

        // Check if any tests need @should_panic support
        bool has_should_panic = false;
        for (const auto& test_info : test_functions) {
            if (test_info.should_panic) {
                has_should_panic = true;
                break;
            }
        }

        // Add error message strings for should_panic tests
        if (has_should_panic) {
            emit_line("");
            emit_line("; Error messages for @should_panic tests");
            // "test did not panic as expected\n\0" = 30 + 1 + 1 = 32 bytes
            emit_line("@.should_panic_no_panic = private constant [32 x i8] c\"test did not "
                      "panic as expected\\0A\\00\"");
            // "panic message did not contain expected string\n\0" = 45 + 1 + 1 = 47 bytes
            emit_line("@.should_panic_wrong_msg = private constant [47 x i8] c\"panic message "
                      "did not contain expected string\\0A\\00\"");
            emit_line("");
        }

        // For DLL entry, generate exported tml_test_entry function instead of main
        if (options_.generate_dll_entry) {
            // Export tml_test_entry for DLL loading
#ifdef _WIN32
            emit_line("define dllexport i32 @tml_test_entry() {");
#else
            emit_line("define i32 @tml_test_entry() {");
#endif
        } else {
            emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        }
        emit_line("entry:");

        int test_idx = 0;
        std::string prev_block = "entry";
        for (const auto& test_info : test_functions) {
            std::string test_fn = "@tml_" + test_info.name;
            std::string idx_str = std::to_string(test_idx);

            if (test_info.should_panic) {
                // Generate panic-catching call for @should_panic tests
                // Uses callback approach: pass function pointer to tml_run_should_panic()
                // which keeps setjmp on the stack while the test runs

                // Call tml_run_should_panic with function pointer
                // Returns: 1 if panicked (success), 0 if didn't panic (failure)
                std::string result = "%panic_result_" + idx_str;
                emit_line("  " + result + " = call i32 @tml_run_should_panic(ptr " + test_fn + ")");

                // Check if test panicked
                std::string cmp = "%panic_cmp_" + idx_str;
                emit_line("  " + cmp + " = icmp eq i32 " + result + ", 0");

                std::string no_panic_label = "no_panic_" + idx_str;
                std::string panic_ok_label = "panic_ok_" + idx_str;
                std::string test_done_label = "test_done_" + idx_str;

                emit_line("  br i1 " + cmp + ", label %" + no_panic_label + ", label %" +
                          panic_ok_label);
                emit_line("");

                // Test didn't panic - that's an error for @should_panic
                emit_line(no_panic_label + ":");
                emit_line("  call i32 (ptr, ...) @printf(ptr @.should_panic_no_panic)");
                emit_line("  call void @exit(i32 1)");
                emit_line("  unreachable");
                emit_line("");

                // Test panicked - check message if expected
                emit_line(panic_ok_label + ":");
                if (!test_info.expected_panic_message_str.empty()) {
                    // Check if panic message contains expected string
                    std::string msg_check = "%msg_check_" + idx_str;
                    emit_line("  " + msg_check + " = call i32 @tml_panic_message_contains(ptr " +
                              test_info.expected_panic_message_str + ")");

                    std::string msg_ok_label = "msg_ok_" + idx_str;
                    std::string msg_fail_label = "msg_fail_" + idx_str;
                    std::string msg_cmp = "%msg_cmp_" + idx_str;
                    emit_line("  " + msg_cmp + " = icmp ne i32 " + msg_check + ", 0");
                    emit_line("  br i1 " + msg_cmp + ", label %" + msg_ok_label + ", label %" +
                              msg_fail_label);
                    emit_line("");

                    // Message didn't match - fail
                    emit_line(msg_fail_label + ":");
                    emit_line("  call i32 (ptr, ...) @printf(ptr @.should_panic_wrong_msg)");
                    emit_line("  call void @exit(i32 1)");
                    emit_line("  unreachable");
                    emit_line("");

                    // Message matched - continue
                    emit_line(msg_ok_label + ":");
                    emit_line("  br label %" + test_done_label);
                } else {
                    // No expected message - any panic is fine
                    emit_line("  br label %" + test_done_label);
                }
                emit_line("");

                emit_line(test_done_label + ":");
                prev_block = test_done_label;
            } else {
                // Regular test - just call it
                auto it = functions_.find(test_info.name);
                if (it != functions_.end() && it->second.ret_type != "void") {
                    std::string tmp = "%test_result_" + idx_str;
                    emit_line("  " + tmp + " = call " + it->second.ret_type + " " + test_fn + "()");
                } else {
                    emit_line("  call void " + test_fn + "()");
                }
            }

            test_idx++;
        }

        // Print coverage report if enabled
        if (options_.coverage_enabled) {
            emit_line("  call void @print_coverage_report()");
            // Write HTML report if output file specified
            if (!coverage_output_str.empty()) {
                emit_line("  call void @write_coverage_html(ptr " + coverage_output_str + ")");
            }
        }

        // All tests passed (if we got here, no assertion failed)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (has_user_main) {
        // Standard main wrapper for user-defined main
        emit_line("; Entry point");

        // For DLL entry, generate exported tml_test_entry function instead of main
        if (options_.generate_dll_entry) {
#ifdef _WIN32
            emit_line("define dllexport i32 @tml_test_entry() {");
#else
            emit_line("define i32 @tml_test_entry() {");
#endif
            emit_line("entry:");
            emit_line("  %ret = call i32 @tml_main()");
            // Print coverage report if enabled
            if (options_.coverage_enabled) {
                emit_line("  call void @print_coverage_report()");
                if (!coverage_output_str.empty()) {
                    emit_line("  call void @write_coverage_html(ptr " + coverage_output_str + ")");
                }
            }
            emit_line("  ret i32 %ret");
            emit_line("}");
        } else {
            emit_line("define i32 @main(i32 %argc, ptr %argv) {");
            emit_line("entry:");
            emit_line("  %ret = call i32 @tml_main()");
            // Print coverage report if enabled
            if (options_.coverage_enabled) {
                emit_line("  call void @print_coverage_report()");
                if (!coverage_output_str.empty()) {
                    emit_line("  call void @write_coverage_html(ptr " + coverage_output_str + ")");
                }
            }
            emit_line("  ret i32 %ret");
            emit_line("}");
        }
    }

    // Emit function attributes for optimization
    emit_line("");
    emit_line("; Function attributes for optimization");
    emit_line("attributes #0 = { nounwind mustprogress willreturn }");

    // Emit debug info metadata at the end
    emit_debug_info_footer();

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
        case lexer::TokenKind::IntLiteral:
            return PrintArgType::Int;
        case lexer::TokenKind::FloatLiteral:
            return PrintArgType::Float;
        case lexer::TokenKind::BoolLiteral:
            return PrintArgType::Bool;
        case lexer::TokenKind::StringLiteral:
            return PrintArgType::Str;
        default:
            return PrintArgType::Unknown;
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        switch (bin.op) {
        case parser::BinaryOp::Add:
            // Check if operands are strings (string concatenation)
            if (infer_print_type(*bin.left) == PrintArgType::Str ||
                infer_print_type(*bin.right) == PrintArgType::Str) {
                return PrintArgType::Str;
            }
            // Check if operands are float
            if (infer_print_type(*bin.left) == PrintArgType::Float ||
                infer_print_type(*bin.right) == PrintArgType::Float) {
                return PrintArgType::Float;
            }
            return PrintArgType::Int;
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
        if (un.op == parser::UnaryOp::Not)
            return PrintArgType::Bool;
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
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& call = expr.as<parser::MethodCallExpr>();
        // to_string() methods return strings
        if (call.method == "to_string" || call.method == "debug_string") {
            return PrintArgType::Str;
        }
        return PrintArgType::Unknown;
    }
    return PrintArgType::Unknown;
}

} // namespace tml::codegen
