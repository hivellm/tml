//! # LLVM IR Generator - Function Declarations
//!
//! This file implements function declaration and instantiation code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/type.hpp"

#include <functional>
#include <iostream>
#include <sstream>

namespace tml::codegen {

// Helper to extract name from FuncParam pattern
// For tuple patterns, returns a synthetic name like __tuple_param_0
static std::string get_param_name(const parser::FuncParam& param, size_t param_index = 0) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    } else if (param.pattern && param.pattern->is<parser::TuplePattern>()) {
        return "__tuple_param_" + std::to_string(param_index);
    }
    return "_anon";
}

void LLVMIRGen::pre_register_func(const parser::FuncDecl& func) {
    // Skip generic functions - they are instantiated on demand
    if (!func.generics.empty()) {
        return;
    }

    // Skip @extern functions - they're handled in gen_func_decl
    if (func.extern_abi.has_value()) {
        return;
    }

    // Build return type
    std::string ret_type = "i32"; // Default
    if (func.return_type.has_value()) {
        std::string inner_ret_type = llvm_type_ptr(*func.return_type);
        if (func.is_async && inner_ret_type != "void") {
            // Async functions return Poll[T]
            auto semantic_ret = resolve_parser_type_with_subs(**func.return_type, {});
            std::vector<types::TypePtr> poll_type_args = {semantic_ret};
            std::string poll_mangled = require_enum_instantiation("Poll", poll_type_args);
            ret_type = "%struct." + poll_mangled;
        } else {
            ret_type = inner_ret_type;
        }
    } else if (func.is_async) {
        // async func with no return type -> Poll[Unit]
        std::vector<types::TypePtr> poll_type_args = {
            std::make_shared<types::Type>(types::PrimitiveType{types::PrimitiveKind::Unit})};
        std::string poll_mangled = require_enum_instantiation("Poll", poll_type_args);
        ret_type = "%struct." + poll_mangled;
    }

    // Build parameter type list
    std::string param_types;
    std::vector<std::string> param_types_vec;
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            param_types += ", ";
        }
        std::string param_type = llvm_type_ptr(func.params[i].type);
        param_types += param_type;
        param_types_vec.push_back(param_type);
    }

    // Build function name with module prefix
    std::string full_func_name = func.name;
    if (!current_module_prefix_.empty()) {
        full_func_name = current_module_prefix_ + "_" + func.name;
    }

    // Register function in functions_ map
    std::string func_type = ret_type + " (" + param_types + ")";
    FuncInfo func_info{"@tml_" + full_func_name, func_type, ret_type, param_types_vec};
    functions_[func.name] = func_info;

    // Also register semantic return type in func_return_types_ for infer_expr_type
    // This enables forward reference resolution (calling a function defined later in the file)
    if (func.return_type.has_value()) {
        types::TypePtr semantic_ret = resolve_parser_type_with_subs(**func.return_type, {});
        if (semantic_ret) {
            func_return_types_[func.name] = semantic_ret;
        }
    }

    // Register with module-qualified name for cross-module calls
    if (!current_module_prefix_.empty()) {
        // Convert prefix to :: format (core_unicode -> core::unicode)
        std::string qualified_name = current_module_prefix_;
        size_t pos = 0;
        while ((pos = qualified_name.find("_", pos)) != std::string::npos) {
            qualified_name.replace(pos, 1, "::");
            pos += 2;
        }
        qualified_name += "::" + func.name;
        functions_[qualified_name] = func_info;

        // Also register with short key (e.g., "unicode::is_alphabetic")
        size_t last_sep = qualified_name.rfind("::");
        if (last_sep != std::string::npos) {
            std::string without_func = qualified_name.substr(0, last_sep);
            size_t second_last_sep = without_func.rfind("::");
            if (second_last_sep != std::string::npos) {
                std::string short_key = qualified_name.substr(second_last_sep + 2);
                functions_[short_key] = func_info;
            }
        }

        // Register with submodule name (e.g., "unicode_data::is_alphabetic_nonascii")
        if (!current_submodule_name_.empty() && current_submodule_name_ != "mod") {
            std::string submod_key = current_submodule_name_ + "::" + func.name;
            functions_[submod_key] = func_info;
        }
    }
}

void LLVMIRGen::gen_func_decl(const parser::FuncDecl& func) {
    // Defer generic functions - they will be instantiated when called
    if (!func.generics.empty()) {
        pending_generic_funcs_[func.name] = &func;
        return;
    }

    // Determine return type
    std::string ret_type = "void";
    std::string inner_ret_type = "void"; // For async functions, the unwrapped return type
    types::TypePtr semantic_ret = nullptr;
    if (func.return_type.has_value()) {
        semantic_ret = resolve_parser_type_with_subs(**func.return_type, {});

        // Handle impl Behavior return types - analyze function body to find concrete type
        if (semantic_ret && semantic_ret->is<types::ImplBehaviorType>() && func.body.has_value()) {
            // Set current_func_ temporarily so llvm_type_from_semantic can look up the concrete
            // type
            std::string saved_func = current_func_;
            current_func_ = func.name;

            // Look for return expressions and infer concrete type from them
            std::string concrete_type;

            // Recursive function to find ReturnExpr in expressions
            std::function<void(const parser::Expr&)> find_in_expr;
            find_in_expr = [&](const parser::Expr& expr) {
                if (!concrete_type.empty())
                    return;
                if (expr.is<parser::ReturnExpr>()) {
                    const auto& ret_expr = expr.as<parser::ReturnExpr>();
                    if (ret_expr.value.has_value()) {
                        const auto& ret_value = **ret_expr.value;
                        // Check if return value is a struct literal directly
                        if (ret_value.is<parser::StructExpr>()) {
                            const auto& struct_expr = ret_value.as<parser::StructExpr>();
                            if (!struct_expr.path.segments.empty()) {
                                concrete_type = "%struct." + struct_expr.path.segments.back();
                                return;
                            }
                        }
                        // Otherwise try to infer the type
                        auto expr_type = infer_expr_type(ret_value);
                        if (expr_type) {
                            // First try to get the LLVM type
                            std::string llvm_t = llvm_type_from_semantic(expr_type, false);
                            if (!llvm_t.empty() && llvm_t != "ptr" && llvm_t != "i32") {
                                concrete_type = llvm_t;
                            }
                            // If that failed, try to extract from NamedType directly
                            // This handles the case where struct isn't registered yet
                            if (concrete_type.empty() && expr_type->is<types::NamedType>()) {
                                const auto& named = expr_type->as<types::NamedType>();
                                if (!named.name.empty()) {
                                    concrete_type = "%struct." + named.name;
                                }
                            }
                        }
                    }
                } else if (expr.is<parser::StructExpr>()) {
                    // Handle struct literal directly (implicit return)
                    const auto& struct_expr = expr.as<parser::StructExpr>();
                    if (!struct_expr.path.segments.empty()) {
                        concrete_type = "%struct." + struct_expr.path.segments.back();
                    }
                } else if (expr.is<parser::BlockExpr>()) {
                    const auto& block = expr.as<parser::BlockExpr>();
                    for (const auto& s : block.stmts) {
                        if (s->is<parser::ExprStmt>()) {
                            find_in_expr(*s->as<parser::ExprStmt>().expr);
                        }
                    }
                    if (block.expr.has_value()) {
                        find_in_expr(**block.expr);
                    }
                } else if (expr.is<parser::IfExpr>()) {
                    const auto& if_expr = expr.as<parser::IfExpr>();
                    find_in_expr(*if_expr.then_branch);
                    if (if_expr.else_branch.has_value()) {
                        find_in_expr(**if_expr.else_branch);
                    }
                } else if (expr.is<parser::WhenExpr>()) {
                    const auto& when_expr = expr.as<parser::WhenExpr>();
                    for (const auto& arm : when_expr.arms) {
                        find_in_expr(*arm.body);
                    }
                } else if (expr.is<parser::LoopExpr>()) {
                    find_in_expr(*expr.as<parser::LoopExpr>().body);
                }
            };

            // Recursive function to find ReturnExpr in statements
            std::function<void(const parser::Stmt&)> find_in_stmt;
            find_in_stmt = [&](const parser::Stmt& stmt) {
                if (!concrete_type.empty())
                    return;
                if (stmt.is<parser::ExprStmt>()) {
                    find_in_expr(*stmt.as<parser::ExprStmt>().expr);
                }
            };

            // Analyze function body statements
            for (const auto& stmt : func.body->stmts) {
                find_in_stmt(*stmt);
                if (!concrete_type.empty())
                    break;
            }

            // Also check the block's tail expression (if any)
            // This handles both `return x` as tail expr and `x` as implicit return
            if (concrete_type.empty() && func.body->expr.has_value()) {
                const auto& tail_expr = **func.body->expr;
                // First check if the tail expr is a ReturnExpr
                find_in_expr(tail_expr);

                // If still empty, check if tail expr is a struct literal directly
                // (implicit return of struct value)
                if (concrete_type.empty() && tail_expr.is<parser::StructExpr>()) {
                    auto expr_type = infer_expr_type(tail_expr);
                    if (expr_type && expr_type->is<types::NamedType>()) {
                        const auto& named = expr_type->as<types::NamedType>();
                        if (!named.name.empty()) {
                            concrete_type = "%struct." + named.name;
                        }
                    }
                }
            }

            // Store the concrete type for use when generating LLVM types
            if (!concrete_type.empty()) {
                impl_behavior_concrete_types_[func.name] = concrete_type;
            }
        }

        inner_ret_type = llvm_type_ptr(*func.return_type);
    }

    // Async functions return Poll[T] instead of T
    // Poll[T] = { i32 tag, T data } where tag 0 = Ready, tag 1 = Pending
    if (func.is_async && inner_ret_type != "void") {
        // Use the semantic return type to create Poll[T]
        if (!semantic_ret) {
            semantic_ret =
                std::make_shared<types::Type>(types::PrimitiveType{types::PrimitiveKind::Unit});
        }
        std::vector<types::TypePtr> poll_type_args = {semantic_ret};
        std::string poll_mangled = require_enum_instantiation("Poll", poll_type_args);
        ret_type = "%struct." + poll_mangled;
        current_poll_type_ = ret_type;
        current_poll_inner_type_ = inner_ret_type; // Store inner type for wrap_in_poll_ready

        // For async functions, record Poll[T] as the return type for type inference
        auto poll_type =
            std::make_shared<types::Type>(types::NamedType{"Poll", "", poll_type_args});
        func_return_types_[func.name] = poll_type;
    } else {
        ret_type = inner_ret_type;
        current_poll_type_.clear();
        current_poll_inner_type_.clear();

        // Check if return type is a value class - return by value instead of ptr
        // This fixes dangling pointer bug for stack-allocated value class objects
        if (ret_type == "ptr" && func.return_type.has_value() &&
            (*func.return_type)->is<parser::NamedType>()) {
            const auto& named = (*func.return_type)->as<parser::NamedType>();
            std::string return_class_name =
                named.path.segments.empty() ? "" : named.path.segments.back();
            if (!return_class_name.empty() && env_.is_value_class_candidate(return_class_name)) {
                // Return value class by value (struct type) instead of ptr
                ret_type = "%class." + return_class_name;
            }
        }

        // Record semantic return type for use in infer_expr_type
        if (semantic_ret) {
            func_return_types_[func.name] = semantic_ret;
        }
    }

    // Build parameter list and type list for function signature
    std::string params;
    std::string param_types;
    std::vector<std::string> param_types_vec;
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
            param_types += ", ";
        }
        std::string param_type = llvm_type_ptr(func.params[i].type);
        // Function-typed parameters use fat pointer { ptr, ptr } to support closures
        // This matches the generic function path (gen_generic_func) and struct field storage
        if (func.params[i].type && func.params[i].type->is<parser::FuncType>()) {
            param_type = "{ ptr, ptr }";
        }
        std::string param_name = get_param_name(func.params[i], i);
        params += param_type + " %" + param_name;
        param_types += param_type;
        param_types_vec.push_back(param_type);
    }

    // Handle @extern functions - emit declare instead of define
    if (func.extern_abi.has_value()) {
        // Get the actual symbol name (extern_name or func.name)
        std::string symbol_name = func.extern_name.value_or(func.name);
        TML_DEBUG_LN("[EXTERN] Processing @extern: func.name="
                     << func.name << " symbol=" << symbol_name << " ret=" << ret_type);

        // Skip if already declared (prevents duplicate declarations when module is imported
        // multiple times)
        if (declared_externals_.find(symbol_name) != declared_externals_.end()) {
            // Still register the function mapping even if declaration was already emitted
            std::string func_type = ret_type + " (" + param_types + ")";
            functions_[func.name] =
                FuncInfo{"@" + symbol_name, func_type, ret_type, param_types_vec, true, func.name};
            return;
        }
        declared_externals_.insert(symbol_name);

        // Determine calling convention based on ABI
        std::string call_conv = "";
        const std::string& abi = *func.extern_abi;
        if (abi == "stdcall") {
            call_conv = "x86_stdcallcc ";
        } else if (abi == "fastcall") {
            call_conv = "x86_fastcallcc ";
        } else if (abi == "thiscall") {
            call_conv = "x86_thiscallcc ";
        }
        // "c" and "c++" use default calling convention (no prefix)

        // For C ABI compatibility: C functions returning bool use i32 (int),
        // not i1. Declare with i32 and truncate at call site.
        std::string abi_ret_type = ret_type;
        bool promoted_bool = false;
        if (abi_ret_type == "i1") {
            abi_ret_type = "i32";
            promoted_bool = true;
        }

        // Emit external declaration
        emit_line("");
        emit_line("; @extern(\"" + abi + "\") " + func.name);
        emit_line("declare " + call_conv + abi_ret_type + " @" + symbol_name + "(" + param_types +
                  ")");

        // Register function - map TML name to external symbol
        // Mark as extern for coverage tracking
        // Use abi_ret_type so call sites emit the correct C ABI return type
        std::string func_type = abi_ret_type + " (" + param_types + ")";
        functions_[func.name] =
            FuncInfo{"@" + symbol_name, func_type,    abi_ret_type, param_types_vec, true,
                     func.name,         promoted_bool};

        // Store link libraries for later (linker phase)
        for (const auto& lib : func.link_libs) {
            extern_link_libs_.insert(lib);
        }

        return; // Don't generate function body for extern functions
    }

    // Handle lowlevel functions without body - these are external C functions
    // pub lowlevel func sys_wsa_startup() -> I32 maps to C function sys_wsa_startup
    if (func.is_unsafe && !func.body.has_value()) {
        // For C ABI compatibility: C functions returning bool use i32, not i1
        std::string abi_ret_type = ret_type;
        bool promoted_bool = false;
        if (abi_ret_type == "i1") {
            abi_ret_type = "i32";
            promoted_bool = true;
        }

        // Only emit declaration if not already declared by runtime
        if (declared_externals_.find(func.name) == declared_externals_.end()) {
            // External C function - emit declaration
            emit_line("");
            emit_line("; lowlevel func " + func.name + " (external C function)");
            emit_line("declare " + abi_ret_type + " @" + func.name + "(" + param_types + ")");
            declared_externals_.insert(func.name);
        }

        // Register function - map TML name directly to C symbol
        std::string func_type = abi_ret_type + " (" + param_types + ")";
        functions_[func.name] = FuncInfo{"@" + func.name, func_type, abi_ret_type, param_types_vec,
                                         false,           "",        promoted_bool};

        return; // Don't generate function body for external lowlevel functions
    }

    current_func_ = func.name;
    locals_.clear();
    consumed_vars_.clear();
    block_terminated_ = false;

    // Store the return type for use in gen_return
    current_ret_type_ = ret_type;
    current_func_is_async_ = func.is_async;

    // Build function name with module prefix if generating code for an imported module
    std::string full_func_name = func.name;
    if (!current_module_prefix_.empty()) {
        full_func_name = current_module_prefix_ + "_" + func.name;
    }

    // In suite mode, add unique prefix to avoid symbol collisions when linking multiple
    // test files into a single DLL. Each test file gets a unique suite_test_index.
    // IMPORTANT: Only add prefix to test-local functions (not library/imported module functions)
    // Library functions (those with current_module_prefix_) should NOT have suite prefix
    // because they're shared across all tests in the suite.
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    // Skip if this function was already generated (handles duplicates in directory modules)
    std::string llvm_name = "@tml_" + suite_prefix + full_func_name;
    if (generated_functions_.count(llvm_name)) {
        // Warn if a file-level function collides with a module-imported function.
        // This can happen when e.g. test function "test_assert_str_empty" collides with
        // module "test" function "assert_str_empty" (both mangle to "tml_test_assert_str_empty").
        if (current_module_prefix_.empty()) {
            TML_LOG_WARN("codegen",
                         "function '"
                             << func.name << "' has LLVM name collision with '" << llvm_name
                             << "' (already generated). The function body will be skipped.");
        }
        return;
    }
    generated_functions_.insert(llvm_name);

    // Register function for first-class function support
    // The registration key uses the original name for lookups within this test file
    std::string func_type = ret_type + " (" + param_types + ")";
    FuncInfo func_info{"@tml_" + suite_prefix + full_func_name, func_type, ret_type,
                       param_types_vec};
    functions_[func.name] = func_info;

    // Also register with module-qualified name for cross-module calls
    // When module A calls module B's function, the lookup uses "B::func" or "B_func"
    if (!current_module_prefix_.empty()) {
        // Register with :: separator (e.g., "core::unicode::is_grapheme_extend_nonascii")
        std::string qualified_name = current_module_prefix_;
        // Replace _ back to :: for the lookup key
        size_t pos = 0;
        while ((pos = qualified_name.find("_", pos)) != std::string::npos) {
            qualified_name.replace(pos, 1, "::");
            pos += 2;
        }
        qualified_name += "::" + func.name;
        functions_[qualified_name] = func_info;

        // Also register with just the last segment of module path
        // This allows `use core::unicode` to enable calls like `unicode::is_alphabetic`
        // From qualified_name like "core::unicode::is_alphabetic", extract "unicode::is_alphabetic"
        size_t last_sep = qualified_name.rfind("::");
        if (last_sep != std::string::npos) {
            // Find the second-to-last :: (the one before the module name)
            std::string without_func = qualified_name.substr(0, last_sep);
            size_t second_last_sep = without_func.rfind("::");
            if (second_last_sep != std::string::npos) {
                // Extract "module::func" pattern (e.g., "unicode::is_alphabetic")
                std::string short_key = qualified_name.substr(second_last_sep + 2);
                functions_[short_key] = func_info;
            }
        }

        // Also register with submodule name for `submodule::func` style calls
        // e.g., when mod.tml calls unicode_data::is_grapheme_extend, register as
        // "unicode_data::is_grapheme_extend"
        if (!current_submodule_name_.empty() && current_submodule_name_ != "mod") {
            std::string submod_key = current_submodule_name_ + "::" + func.name;
            functions_[submod_key] = func_info;
        }
    }

    // Function signature with optimization attributes
    // All user-defined functions get tml_ prefix (main becomes tml_main, wrapper @main calls it)
    // In suite mode, add unique prefix to avoid symbol collisions
    std::string func_llvm_name = "tml_" + suite_prefix + full_func_name;
    // Public functions, main, and @should_panic tests get external linkage
    // @should_panic tests need external linkage because they're called via function pointer
    bool has_should_panic = false;
    for (const auto& decorator : func.decorators) {
        if (decorator.name == "should_panic") {
            has_should_panic = true;
            break;
        }
    }
    // In suite mode (force_internal_linkage), all functions including main get internal linkage
    // to avoid duplicate symbols when linking multiple test objects into one DLL.
    // Only @should_panic tests need external linkage (called via function pointer).
    // In library_ir_only mode, all functions need external linkage so they can be
    // linked from test objects that only have `declare` stubs.
    std::string linkage =
        (options_.library_ir_only || (!options_.force_internal_linkage && func.name == "main") ||
         (func.vis == parser::Visibility::Public && !options_.force_internal_linkage) ||
         has_should_panic)
            ? ""
            : "internal ";
    // Windows DLL export for public functions (disabled in suite mode)
    std::string dll_linkage = "";
    if (options_.dll_export && func.vis == parser::Visibility::Public && func.name != "main" &&
        !options_.force_internal_linkage) {
        dll_linkage = "dllexport ";
    }
    // Optimization attributes:
    // - nounwind: function doesn't throw exceptions
    // - mustprogress: function will eventually return (enables loop optimizations)
    // - willreturn: function will return (helps with dead code elimination)
    std::string attrs = " #0";
    emit_line("");

    // In library_decls_only mode, emit a declare statement for library functions
    // instead of the full definition. The function info is already registered above.
    // Library functions have a non-empty current_module_prefix_.
    if (options_.library_decls_only && !current_module_prefix_.empty()) {
        emit_line("declare " + ret_type + " @" + func_llvm_name + "(" + param_types + ")");
        current_func_.clear();
        return;
    }

    // Create debug scope for function (if debug info enabled)
    int func_scope_id = 0;
    if (options_.emit_debug_info) {
        func_scope_id = create_function_debug_scope(func_llvm_name, func.span.start.line,
                                                    func.span.start.column);
        // Create a default debug location for instructions in this function
        create_debug_location(func.span.start.line, func.span.start.column);
    }

    // Add debug info as function attribute if we have a scope
    std::string dbg_attr = "";
    if (func_scope_id != 0) {
        dbg_attr = " !dbg !" + std::to_string(func_scope_id);
    }

    emit_line("define " + dll_linkage + linkage + ret_type + " @" + func_llvm_name + "(" + params +
              ")" + attrs + dbg_attr + " {");
    emit_line("entry:");

    // Register function parameters in locals_ by creating allocas
    for (size_t i = 0; i < func.params.size(); ++i) {
        std::string param_type = llvm_type_ptr(func.params[i].type);
        // Function-typed parameters use fat pointer { ptr, ptr } to support closures
        if (func.params[i].type && func.params[i].type->is<parser::FuncType>()) {
            param_type = "{ ptr, ptr }";
        }
        std::string param_name = get_param_name(func.params[i], i);
        // Resolve semantic type for the parameter
        types::TypePtr semantic_type = resolve_parser_type_with_subs(*func.params[i].type, {});
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};

        // Emit debug info for parameters (if enabled and debug level >= 2)
        if (options_.emit_debug_info && options_.debug_level >= 2 && current_scope_id_ != 0) {
            uint32_t line = func.params[i].span.start.line;
            uint32_t column = func.params[i].span.start.column;

            // Create debug info for parameter (arg_no is 1-based)
            int param_debug_id = create_local_variable_debug_info(param_name, param_type, line,
                                                                  static_cast<uint32_t>(i + 1));

            // Create debug location
            int loc_id = fresh_debug_id();
            std::ostringstream meta;
            meta << "!" << loc_id << " = !DILocation("
                 << "line: " << line << ", "
                 << "column: " << column << ", "
                 << "scope: !" << current_scope_id_ << ")\n";
            debug_metadata_.push_back(meta.str());

            // Emit llvm.dbg.declare intrinsic
            emit_debug_declare(alloca_reg, param_debug_id, loc_id);
        }
    }

    // Destructure tuple pattern parameters
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (func.params[i].pattern && func.params[i].pattern->is<parser::TuplePattern>()) {
            const auto& tuple_pat = func.params[i].pattern->as<parser::TuplePattern>();
            std::string param_name = get_param_name(func.params[i], i);
            std::string param_type = llvm_type_ptr(func.params[i].type);
            types::TypePtr semantic_type = resolve_parser_type_with_subs(*func.params[i].type, {});

            // Get the alloca for the tuple parameter
            auto it = locals_.find(param_name);
            if (it == locals_.end())
                continue;
            std::string tuple_ptr = it->second.reg;

            // Parse tuple element types
            std::vector<std::string> elem_types;
            std::vector<types::TypePtr> semantic_elem_types;
            if (semantic_type && semantic_type->is<types::TupleType>()) {
                const auto& tup = semantic_type->as<types::TupleType>();
                semantic_elem_types = tup.elements;
                for (const auto& elem : tup.elements) {
                    elem_types.push_back(llvm_type_from_semantic(elem));
                }
            }

            // Destructure each element
            for (size_t j = 0; j < tuple_pat.elements.size() && j < elem_types.size(); ++j) {
                const auto& elem_pattern = *tuple_pat.elements[j];
                if (elem_pattern.is<parser::IdentPattern>()) {
                    const auto& ident = elem_pattern.as<parser::IdentPattern>();
                    std::string elem_type = elem_types[j];
                    types::TypePtr semantic_elem =
                        j < semantic_elem_types.size() ? semantic_elem_types[j] : nullptr;

                    // GEP to get element pointer
                    std::string elem_ptr = fresh_reg();
                    emit_line("  " + elem_ptr + " = getelementptr inbounds " + param_type +
                              ", ptr " + tuple_ptr + ", i32 0, i32 " + std::to_string(j));

                    // Load element value
                    std::string elem_val = fresh_reg();
                    emit_line("  " + elem_val + " = load " + elem_type + ", ptr " + elem_ptr);

                    // Alloca and store for the named variable
                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + elem_type);
                    emit_line("  store " + elem_type + " " + elem_val + ", ptr " + var_alloca);
                    locals_[ident.name] =
                        VarInfo{var_alloca, elem_type, semantic_elem, std::nullopt};
                }
                // Wildcards are ignored, nested tuples would need recursive handling
            }
        }
    }

    // Coverage instrumentation - inject call at function entry
    emit_coverage(func.name);

    // LLVM source-based coverage instrumentation
    // Only instrument user code (not library functions) to avoid duplicate symbols in suite mode
    // For full library coverage, would need to compile library separately with coverage
    if (options_.llvm_source_coverage && current_module_prefix_.empty()) {
        // Create function name global for profiling
        std::string prof_name = "@__profn_" + func_llvm_name;
        std::string name_value = func_llvm_name;
        size_t name_len = name_value.length() + 1; // +1 for null terminator

        // Emit the function name constant in the profile names section
        // Note: We use linkonce_odr to handle multiple definitions in suite mode
        type_defs_buffer_ << prof_name << " = linkonce_odr constant [" << name_len << " x i8] c\""
                          << name_value << "\\00\", section \"__llvm_prf_names\"\n";

        // Compute a simple hash based on the function name (FNV-1a)
        uint64_t hash = 14695981039346656037ULL; // FNV offset basis
        for (char c : name_value) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL; // FNV prime
        }

        // Insert instrprof.increment at function entry
        // Arguments: name ptr, hash, num counters, counter index
        emit_line("  call void @llvm.instrprof.increment(ptr " + prof_name + ", i64 " +
                  std::to_string(hash) + ", i32 1, i32 0)");
    }

    // Generate function body
    if (func.body) {
        // Push drop scope for function body - variables here need drop at return
        push_drop_scope();

        for (const auto& stmt : func.body->stmts) {
            if (block_terminated_) {
                // Block already terminated, skip remaining statements
                break;
            }
            gen_stmt(*stmt);
        }

        // Handle trailing expression (return value)
        if (func.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*func.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                // Emit drops before returning
                emit_all_drops();
                // For async functions, wrap result in Poll.Ready
                if (current_func_is_async_ && !current_poll_type_.empty()) {
                    std::string wrapped = wrap_in_poll_ready(result, last_expr_type_);
                    emit_line("  ret " + current_poll_type_ + " " + wrapped);
                } else {
                    // Fix: Unit type always uses zeroinitializer (can't use bool/int values)
                    if (ret_type == "{}") {
                        emit_line("  ret {} zeroinitializer");
                    } else if (ret_type == "ptr" && result == "0") {
                        // Fix: if returning ptr type with "0" placeholder (from loops), use null
                        emit_line("  ret ptr null");
                    } else if (result == "0" && ret_type.find("%struct.") == 0) {
                        // Fix: if returning struct type with "0" placeholder, use zeroinitializer
                        emit_line("  ret " + ret_type + " zeroinitializer");
                    } else {
                        // Handle integer type extension when actual differs from expected
                        std::string final_result = result;
                        std::string actual_type = last_expr_type_;
                        if (actual_type != ret_type) {
                            // Integer extension: i32 -> i64, i16 -> i64, i8 -> i64
                            if (ret_type == "i64" &&
                                (actual_type == "i32" || actual_type == "i16" ||
                                 actual_type == "i8")) {
                                std::string ext_reg = fresh_reg();
                                emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                          " to i64");
                                final_result = ext_reg;
                            } else if (ret_type == "i32" &&
                                       (actual_type == "i16" || actual_type == "i8")) {
                                std::string ext_reg = fresh_reg();
                                emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                          " to i32");
                                final_result = ext_reg;
                            }
                        }
                        emit_line("  ret " + ret_type + " " + final_result);
                    }
                }
                block_terminated_ = true;
            }
        }

        pop_drop_scope();
    }

    // Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else {
            // For other types, return zeroinitializer
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");
    current_func_.clear();
    current_ret_type_.clear();
    current_func_is_async_ = false;
    current_poll_type_.clear();
    current_poll_inner_type_.clear();
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

void LLVMIRGen::gen_func_instantiation(const parser::FuncDecl& func,
                                       const std::vector<types::TypePtr>& type_args) {
    // 1. Create substitution map: T -> I32, U -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < func.generics.size() && i < type_args.size(); ++i) {
        subs[func.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled function name: identity[I32] -> identity__I32
    // NOTE: Do NOT add suite prefix for generic function instantiations.
    // Generic functions are typically from libraries (take, map, filter, etc.) and should
    // be shared across all test files in a suite. The instantiation is keyed by mangled_name
    // in func_instantiations_, so we need consistency between call and definition.
    std::string mangled = mangle_func_name(func.name, type_args);

    // Save current context
    std::string saved_func = current_func_;
    std::string saved_ret_type = current_ret_type_;
    bool saved_terminated = block_terminated_;
    auto saved_locals = locals_;
    auto saved_consumed = consumed_vars_;
    auto saved_type_subs = current_type_subs_;
    auto saved_where_constraints = current_where_constraints_;

    current_func_ = mangled;
    locals_.clear();
    consumed_vars_.clear();
    block_terminated_ = false;
    current_type_subs_ = subs;

    // Extract where constraints for bounded generic method dispatch
    current_where_constraints_.clear();

    // First, extract bounds directly from generic parameters (e.g., [T: ToOwned])
    for (const auto& generic_param : func.generics) {
        if (!generic_param.bounds.empty()) {
            types::WhereConstraint constraint;
            constraint.type_param = generic_param.name;

            for (const auto& bound : generic_param.bounds) {
                if (bound->is<parser::NamedType>()) {
                    const auto& named = bound->as<parser::NamedType>();
                    std::string behavior_name;
                    if (!named.path.segments.empty()) {
                        behavior_name = named.path.segments.back();
                    }

                    if (!named.generics.has_value() || named.generics->args.empty()) {
                        // Simple bound like T: ToOwned
                        constraint.required_behaviors.push_back(behavior_name);
                    } else {
                        // Parameterized bound like C: Container[T]
                        types::BoundConstraint bc;
                        bc.behavior_name = behavior_name;
                        for (const auto& arg : named.generics->args) {
                            if (arg.is_type()) {
                                bc.type_args.push_back(
                                    resolve_parser_type_with_subs(*arg.as_type(), subs));
                            }
                        }
                        constraint.parameterized_bounds.push_back(bc);
                    }
                }
            }

            if (!constraint.required_behaviors.empty() ||
                !constraint.parameterized_bounds.empty()) {
                current_where_constraints_.push_back(constraint);
            }
        }
    }

    // Then, also extract from explicit where clause if present
    if (func.where_clause) {
        for (const auto& [type_ptr, bounds] : func.where_clause->constraints) {
            // Get the type parameter name from the type pointer
            std::string type_param_name;
            if (type_ptr->is<parser::NamedType>()) {
                const auto& named_type = type_ptr->as<parser::NamedType>();
                if (!named_type.path.segments.empty()) {
                    type_param_name = named_type.path.segments.back();
                }
            }

            if (!type_param_name.empty()) {
                types::WhereConstraint constraint;
                constraint.type_param = type_param_name;

                for (const auto& bound : bounds) {
                    if (bound->is<parser::NamedType>()) {
                        const auto& named = bound->as<parser::NamedType>();
                        std::string behavior_name;
                        if (!named.path.segments.empty()) {
                            behavior_name = named.path.segments.back();
                        }

                        if (!named.generics.has_value() || named.generics->args.empty()) {
                            // Simple bound like T: Display
                            constraint.required_behaviors.push_back(behavior_name);
                        } else {
                            // Parameterized bound like C: Container[T]
                            types::BoundConstraint bc;
                            bc.behavior_name = behavior_name;
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    // Resolve the type argument with current substitutions
                                    bc.type_args.push_back(
                                        resolve_parser_type_with_subs(*arg.as_type(), subs));
                                }
                            }
                            constraint.parameterized_bounds.push_back(bc);
                        }
                    }
                }

                current_where_constraints_.push_back(constraint);
            }
        }
    }

    // 3. Determine return type with substitution
    std::string ret_type = "void";
    if (func.return_type.has_value()) {
        types::TypePtr resolved_ret = resolve_parser_type_with_subs(**func.return_type, subs);
        ret_type = llvm_type_from_semantic(resolved_ret);
    }
    current_ret_type_ = ret_type;

    // 4. Build parameter list with substituted types
    std::string params;
    std::string param_types;
    // Store name, llvm_type, and semantic_type for each parameter
    struct ParamInfo {
        std::string name;
        std::string llvm_type;
        types::TypePtr semantic_type;
    };
    std::vector<ParamInfo> param_info;

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
            param_types += ", ";
        }
        // Resolve param type with substitution
        types::TypePtr resolved_param = resolve_parser_type_with_subs(*func.params[i].type, subs);
        std::string param_type = llvm_type_from_semantic(resolved_param);
        // Function-typed parameters use fat pointer { ptr, ptr } to support closures
        // This matches struct field storage (see types.cpp struct field generation)
        if (resolved_param && resolved_param->is<types::FuncType>()) {
            param_type = "{ ptr, ptr }";
        }
        std::string param_name = get_param_name(func.params[i], i);

        params += param_type + " %" + param_name;
        param_types += param_type;
        param_info.push_back({param_name, param_type, resolved_param});
    }

    // 5. Register function for first-class function support
    std::string func_type = ret_type + " (" + param_types + ")";
    std::vector<std::string> param_types_vec;
    for (const auto& p : param_info) {
        param_types_vec.push_back(p.llvm_type);
    }
    functions_[mangled] = FuncInfo{"@tml_" + mangled, func_type, ret_type, param_types_vec};

    // 6. Emit function definition
    std::string attrs = " #0";
    // Public functions get external linkage for library export
    // In suite mode (force_internal_linkage), all functions are internal to avoid duplicate symbols
    std::string linkage =
        (func.vis == parser::Visibility::Public && !options_.force_internal_linkage) ? ""
                                                                                     : "internal ";
    // Windows DLL export for public functions (disabled in suite mode)
    std::string dll_linkage = "";
    if (options_.dll_export && func.vis == parser::Visibility::Public &&
        !options_.force_internal_linkage) {
        dll_linkage = "dllexport ";
    }
    emit_line("");

    // Create debug scope for generic function instantiation (if debug info enabled)
    int func_scope_id = 0;
    if (options_.emit_debug_info) {
        func_scope_id = create_function_debug_scope("tml_" + mangled, func.span.start.line,
                                                    func.span.start.column);
        // Create a default debug location for instructions in this function
        create_debug_location(func.span.start.line, func.span.start.column);
    }

    // Add debug info as function attribute if we have a scope
    std::string dbg_attr = "";
    if (func_scope_id != 0) {
        dbg_attr = " !dbg !" + std::to_string(func_scope_id);
    }

    emit_line("define " + dll_linkage + linkage + ret_type + " @tml_" + mangled + "(" + params +
              ")" + attrs + dbg_attr + " {");
    emit_line("entry:");

    // 7. Register parameters in locals_
    for (size_t i = 0; i < param_info.size(); ++i) {
        const auto& p = param_info[i];
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + p.llvm_type);
        emit_line("  store " + p.llvm_type + " %" + p.name + ", ptr " + alloca_reg);
        locals_[p.name] = VarInfo{alloca_reg, p.llvm_type, p.semantic_type, std::nullopt};

        // Emit debug info for parameters (if enabled and debug level >= 2)
        if (options_.emit_debug_info && options_.debug_level >= 2 && current_scope_id_ != 0) {
            uint32_t line = func.params[i].span.start.line;
            uint32_t column = func.params[i].span.start.column;

            // Create debug info for parameter (arg_no is 1-based)
            int param_debug_id = create_local_variable_debug_info(p.name, p.llvm_type, line,
                                                                  static_cast<uint32_t>(i + 1));

            // Create debug location
            int loc_id = fresh_debug_id();
            std::ostringstream meta;
            meta << "!" << loc_id << " = !DILocation("
                 << "line: " << line << ", "
                 << "column: " << column << ", "
                 << "scope: !" << current_scope_id_ << ")\n";
            debug_metadata_.push_back(meta.str());

            // Emit llvm.dbg.declare intrinsic
            emit_debug_declare(alloca_reg, param_debug_id, loc_id);
        }
    }

    // TML runtime coverage for generic instantiation
    // This tracks library function calls via the TML coverage runtime
    emit_coverage(func.name);

    // LLVM source-based coverage instrumentation for generic instantiation
    if (options_.llvm_source_coverage) {
        // Create function name global for profiling
        std::string prof_name = "@__profn_tml_" + mangled;
        std::string name_value = "tml_" + mangled;
        size_t name_len = name_value.length() + 1; // +1 for null terminator

        // Emit the function name constant in the profile names section
        // Note: We use linkonce_odr to handle multiple definitions in suite mode
        type_defs_buffer_ << prof_name << " = linkonce_odr constant [" << name_len << " x i8] c\""
                          << name_value << "\\00\", section \"__llvm_prf_names\"\n";

        // Compute a simple hash based on the function name (FNV-1a)
        uint64_t hash = 14695981039346656037ULL; // FNV offset basis
        for (char c : name_value) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL; // FNV prime
        }

        // Insert instrprof.increment at function entry
        emit_line("  call void @llvm.instrprof.increment(ptr " + prof_name + ", i64 " +
                  std::to_string(hash) + ", i32 1, i32 0)");
    }

    // 8. Generate function body
    if (func.body) {
        // Push drop scope for function body - variables here need drop at return
        push_drop_scope();

        for (const auto& stmt : func.body->stmts) {
            if (block_terminated_)
                break;
            gen_stmt(*stmt);
        }

        // Handle trailing expression (return value)
        if (func.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*func.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                // Emit drops before returning
                emit_all_drops();
                // Fix: Unit type always uses zeroinitializer (can't use bool/int values)
                if (ret_type == "{}") {
                    emit_line("  ret {} zeroinitializer");
                } else if (ret_type == "ptr" && result == "0") {
                    // Fix: if returning ptr type with "0" placeholder (from loops), use null
                    emit_line("  ret ptr null");
                } else if (result == "0" && ret_type.find("%struct.") == 0) {
                    // Fix: if returning struct type with "0" placeholder, use zeroinitializer
                    emit_line("  ret " + ret_type + " zeroinitializer");
                } else {
                    // Handle integer type extension when actual differs from expected
                    std::string final_result = result;
                    std::string actual_type = last_expr_type_;
                    if (actual_type != ret_type) {
                        if (ret_type == "i64" &&
                            (actual_type == "i32" || actual_type == "i16" || actual_type == "i8")) {
                            std::string ext_reg = fresh_reg();
                            emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                      " to i64");
                            final_result = ext_reg;
                        } else if (ret_type == "i32" &&
                                   (actual_type == "i16" || actual_type == "i8")) {
                            std::string ext_reg = fresh_reg();
                            emit_line("  " + ext_reg + " = sext " + actual_type + " " + result +
                                      " to i32");
                            final_result = ext_reg;
                        }
                    }
                    emit_line("  ret " + ret_type + " " + final_result);
                }
                block_terminated_ = true;
            }
        }

        pop_drop_scope();
    }

    // 9. Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");

    // Restore context
    current_func_ = saved_func;
    current_ret_type_ = saved_ret_type;
    block_terminated_ = saved_terminated;
    locals_ = saved_locals;
    consumed_vars_ = saved_consumed;
    current_type_subs_ = saved_type_subs;
    current_where_constraints_ = saved_where_constraints;
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

} // namespace tml::codegen
