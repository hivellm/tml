TML_MODULE("compiler")

#include "query/query_core.hpp"

#include "query/query_context.hpp"

// Full includes for all compilation stages
#include "borrow/checker.hpp"
#include "borrow/polonius.hpp"
#include "codegen/codegen_backend.hpp"
#include "codegen/llvm/llvm_ir_gen.hpp"
#include "codegen/mir_codegen.hpp"
#include "common.hpp"
#include "hir/hir.hpp"
#include "hir/hir_builder.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "log/log.hpp"
#include "mir/hir_mir_builder.hpp"
#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"
#include "mir/passes/infinite_loop_check.hpp"
#include "mir/passes/memory_leak_check.hpp"
#include "mir/passes/pgo.hpp"
#include "mir/thir_mir_builder.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "thir/thir.hpp"
#include "thir/thir_lower.hpp"
#include "traits/solver.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"
#include "types/module_binary.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace tml::query::providers {

// ============================================================================
// ReadSource Provider
// ============================================================================

std::any provide_read_source(QueryContext& ctx, const QueryKey& key) {
    const auto& rk = std::get<ReadSourceKey>(key);
    ReadSourceResult result;

    // Read the source file (text mode for proper \r\n -> \n translation on Windows)
    try {
        std::ifstream file(rk.file_path);
        if (!file) {
            result.error_message = "Cannot open file: " + rk.file_path;
            return result;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        result.source_code = ss.str();
    } catch (const std::exception& e) {
        result.error_message = e.what();
        return result;
    }

    // Preprocess
    const auto& opts = ctx.options();
    preprocessor::PreprocessorConfig config = preprocessor::Preprocessor::host_config();

    if (opts.optimization_level >= 2) {
        config.build_mode = preprocessor::BuildMode::Release;
    }

    // Parse target if specified
    if (!opts.target_triple.empty()) {
        config = preprocessor::Preprocessor::parse_target_triple(opts.target_triple);
        if (opts.optimization_level >= 2) {
            config.build_mode = preprocessor::BuildMode::Release;
        }
    }

    // Add user defines
    for (const auto& def : opts.defines) {
        auto eq_pos = def.find('=');
        if (eq_pos != std::string::npos) {
            config.defines[def.substr(0, eq_pos)] = def.substr(eq_pos + 1);
        } else {
            config.defines[def] = "";
        }
    }

    preprocessor::Preprocessor pp(config);
    auto preproc_result = pp.process(result.source_code, rk.file_path);

    if (!preproc_result.success()) {
        result.error_message = "Preprocessing failed";
        for (const auto& diag : preproc_result.diagnostics) {
            if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                result.error_message += ": " + diag.message;
                break;
            }
        }
        return result;
    }

    result.preprocessed = std::move(preproc_result.output);
    result.success = true;
    return result;
}

// ============================================================================
// Tokenize Provider
// ============================================================================

std::any provide_tokenize(QueryContext& ctx, const QueryKey& key) {
    const auto& tk = std::get<TokenizeKey>(key);
    TokenizeResult result;

    // Force read_source first
    auto src = ctx.read_source(tk.file_path);
    if (!src.success) {
        result.errors.push_back(src.error_message);
        return result;
    }

    // Tokenize — store Source in result so token string_views stay valid
    auto source =
        std::make_shared<lexer::Source>(lexer::Source::from_string(src.preprocessed, tk.file_path));
    lexer::Lexer lex(*source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& err : lex.errors()) {
            result.errors.push_back(err.message);
        }
        return result;
    }

    result.tokens = std::make_shared<std::vector<lexer::Token>>(std::move(tokens));
    result.source = std::move(source);
    result.success = true;
    return result;
}

// ============================================================================
// ParseModule Provider
// ============================================================================

std::any provide_parse_module(QueryContext& ctx, const QueryKey& key) {
    const auto& pk = std::get<ParseModuleKey>(key);
    ParseModuleResult result;

    // Force tokenization
    auto tok = ctx.tokenize(pk.file_path);
    if (!tok.success) {
        result.errors = tok.errors;
        return result;
    }

    // Parse
    // Parser takes ownership of the tokens vector, so we copy it
    auto tokens_copy = *tok.tokens;
    parser::Parser parser(std::move(tokens_copy));
    auto parse_result = parser.parse_module(pk.module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& err : errors) {
            result.errors.push_back(err.message);
        }
        return result;
    }

    result.module =
        std::make_shared<parser::Module>(std::move(std::get<parser::Module>(parse_result)));
    result.success = true;
    return result;
}

// ============================================================================
// TypecheckModule Provider
// ============================================================================

std::any provide_typecheck_module(QueryContext& ctx, const QueryKey& key) {
    const auto& tk = std::get<TypecheckModuleKey>(key);
    TypecheckResult result;

    // Force parsing
    auto parsed = ctx.parse_module(tk.file_path, tk.module_name);
    if (!parsed.success) {
        result.errors = parsed.errors;
        return result;
    }

    // Library modules are loaded on-demand by TypeChecker when `use` imports
    // are resolved. load_native_module() checks GlobalModuleCache first, then
    // falls back to .tml.meta binary cache files. This avoids eagerly loading
    // all ~100+ library modules when only a few are needed.

    // Create module registry and type checker
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);

    // Set source directory for local module resolution
    auto source_dir = fs::path(tk.file_path).parent_path();
    if (source_dir.empty()) {
        source_dir = fs::current_path();
    }
    if (!ctx.options().source_directory.empty()) {
        source_dir = fs::path(ctx.options().source_directory);
    }
    checker.set_source_directory(source_dir.string());

    auto check_result = checker.check_module(*parsed.module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& err : errors) {
            result.errors.push_back(err.message);
        }
        return result;
    }

    result.env =
        std::make_shared<types::TypeEnv>(std::move(std::get<types::TypeEnv>(check_result)));
    result.registry = registry;
    result.success = true;
    return result;
}

// ============================================================================
// BorrowcheckModule Provider
// ============================================================================

std::any provide_borrowcheck_module(QueryContext& ctx, const QueryKey& key) {
    const auto& bk = std::get<BorrowcheckModuleKey>(key);
    BorrowcheckResult result;

    // Force type checking and parsing
    auto tc = ctx.typecheck_module(bk.file_path, bk.module_name);
    if (!tc.success) {
        result.errors = tc.errors;
        return result;
    }

    auto parsed = ctx.parse_module(bk.file_path, bk.module_name);
    if (!parsed.success) {
        result.errors = parsed.errors;
        return result;
    }

    // Borrow check (Polonius or NLL)
    std::variant<bool, std::vector<borrow::BorrowError>> borrow_result;
    if (CompilerOptions::polonius) {
        borrow::polonius::PoloniusChecker polonius_checker(*tc.env);
        borrow_result = polonius_checker.check_module(*parsed.module);
    } else {
        borrow::BorrowChecker borrow_checker(*tc.env);
        borrow_result = borrow_checker.check_module(*parsed.module);
    }

    if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
        const auto& errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
        for (const auto& err : errors) {
            result.errors.push_back(err.message);
        }
        return result;
    }

    result.success = true;
    return result;
}

// ============================================================================
// HirLower Provider
// ============================================================================

std::any provide_hir_lower(QueryContext& ctx, const QueryKey& key) {
    const auto& hk = std::get<HirLowerKey>(key);
    HirLowerResult result;

    // Force type checking and parsing
    auto tc = ctx.typecheck_module(hk.file_path, hk.module_name);
    if (!tc.success) {
        return result;
    }

    auto parsed = ctx.parse_module(hk.file_path, hk.module_name);
    if (!parsed.success) {
        return result;
    }

    // HirBuilder needs non-const TypeEnv — make a copy
    auto env_copy = *tc.env;
    hir::HirBuilder hir_builder(env_copy);
    auto hir_module = hir_builder.lower_module(*parsed.module);

    result.hir_module = std::make_shared<hir::HirModule>(std::move(hir_module));
    result.success = true;
    return result;
}

// ============================================================================
// ThirLower Provider
// ============================================================================

std::any provide_thir_lower(QueryContext& ctx, const QueryKey& key) {
    const auto& tk = std::get<ThirLowerKey>(key);
    ThirLowerResult result;

    // Force HIR lowering
    auto hir = ctx.hir_lower(tk.file_path, tk.module_name);
    if (!hir.success) {
        return result;
    }

    auto tc = ctx.typecheck_module(tk.file_path, tk.module_name);
    if (!tc.success) {
        return result;
    }

    // Lower HIR to THIR using trait solver
    traits::TraitSolver solver(*tc.env);
    thir::ThirLower thir_lower(*tc.env, solver);
    auto thir_module = thir_lower.lower_module(*hir.hir_module);

    result.thir_module = std::make_shared<thir::ThirModule>(std::move(thir_module));
    result.diagnostics = thir_lower.diagnostics();
    result.success = true;
    return result;
}

// ============================================================================
// MirBuild Provider
// ============================================================================

std::any provide_mir_build(QueryContext& ctx, const QueryKey& key) {
    const auto& mk = std::get<MirBuildKey>(key);
    MirBuildResult result;

    auto tc = ctx.typecheck_module(mk.file_path, mk.module_name);
    if (!tc.success) {
        return result;
    }

    mir::Module mir_module;

    // Route through THIR pipeline when --use-thir is enabled
    if (CompilerOptions::use_thir) {
        auto thir = ctx.thir_lower(mk.file_path, mk.module_name);
        if (!thir.success) {
            return result;
        }

        // Report THIR diagnostics (exhaustiveness warnings, etc.)
        for (const auto& diag : thir.diagnostics) {
            TML_LOG_WARN("thir", diag);
        }

        // Build MIR from THIR
        mir::ThirMirBuilder thir_mir_builder(*tc.env);
        mir_module = thir_mir_builder.build(*thir.thir_module);
    } else {
        // Default: HIR → MIR pipeline
        auto hir = ctx.hir_lower(mk.file_path, mk.module_name);
        if (!hir.success) {
            return result;
        }

        mir::HirMirBuilder hir_mir_builder(*tc.env);
        mir_module = hir_mir_builder.build(*hir.hir_module);
    }

    // Run infinite loop detection
    mir::InfiniteLoopCheckPass loop_check;
    loop_check.run(mir_module);
    if (loop_check.has_warnings()) {
        for (const auto& warning : loop_check.get_warnings()) {
            result.errors.push_back("potential infinite loop in function '" +
                                    warning.function_name + "' at block '" + warning.block_name +
                                    "': " + warning.reason);
        }
        return result;
    }

    // Run memory leak detection
    mir::MemoryLeakCheckPass leak_check;
    leak_check.run(mir_module);
    if (leak_check.has_errors()) {
        result.errors.push_back("memory leak detected");
        return result;
    }

    // Apply MIR optimizations
    int opt_level = ctx.options().optimization_level;
    if (opt_level > 0) {
        mir::OptLevel mir_opt = mir::OptLevel::O0;
        if (opt_level == 1)
            mir_opt = mir::OptLevel::O1;
        else if (opt_level == 2)
            mir_opt = mir::OptLevel::O2;
        else if (opt_level >= 3)
            mir_opt = mir::OptLevel::O3;

        auto env_copy = *tc.env;
        mir::PassManager pm(mir_opt);
        pm.configure_standard_pipeline(env_copy);

        // PGO instrumentation
        if (ctx.options().profile_generate) {
            mir::ProfileInstrumentationPass inst_pass;
            inst_pass.run(mir_module);
        }

        // Load profile data
        if (!ctx.options().profile_use.empty()) {
            auto profile_opt = mir::ProfileData::load(ctx.options().profile_use);
            if (profile_opt) {
                pm.set_profile_data(&*profile_opt);
            }
        }

        pm.run(mir_module);

        // Apply PGO passes after inlining
        if (!ctx.options().profile_use.empty()) {
            auto profile_opt = mir::ProfileData::load(ctx.options().profile_use);
            if (profile_opt && mir::ProfileIO::validate(*profile_opt, mir_module)) {
                mir::PgoPass pgo_pass(*profile_opt);
                pgo_pass.run(mir_module);
            }
        }
    }

    result.mir_module = std::make_shared<mir::Module>(std::move(mir_module));

    result.success = true;
    return result;
}

// ============================================================================
// CodegenUnit Provider
// ============================================================================

std::any provide_codegen_unit(QueryContext& ctx, const QueryKey& key) {
    const auto* ckp = std::get_if<CodegenUnitKey>(&key);
    if (!ckp) {
        CodegenUnitResult fail;
        return fail;
    }
    const auto& ck = *ckp;
    CodegenUnitResult result;

    // Force borrow checking (ensures typecheck + parse are done too)
    auto bc = ctx.borrowcheck_module(ck.file_path, ck.module_name);
    if (!bc.success) {
        result.error_message = "Borrow check failed";
        if (!bc.errors.empty()) {
            result.error_message = bc.errors[0];
        }
        return result;
    }

    auto tc = ctx.typecheck_module(ck.file_path, ck.module_name);
    auto parsed = ctx.parse_module(ck.file_path, ck.module_name);

    if (!tc.success || !parsed.success) {
        result.error_message = "Upstream query failed";
        return result;
    }

    // Check if there are imported TML modules that need codegen
    bool has_tml_imports_needing_codegen = false;
    if (tc.env->module_registry()) {
        for (const auto& [mod_name, mod] : tc.env->module_registry()->get_all_modules()) {
            if (mod.has_pure_tml_functions && !mod.source_code.empty()) {
                has_tml_imports_needing_codegen = true;
                break;
            }
        }
    }

    // Check if there are local generic types
    bool has_local_generics = false;
    for (const auto& decl : parsed.module->decls) {
        if (decl->is<parser::StructDecl>()) {
            if (!decl->as<parser::StructDecl>().generics.empty()) {
                has_local_generics = true;
                break;
            }
        } else if (decl->is<parser::EnumDecl>()) {
            if (!decl->as<parser::EnumDecl>().generics.empty()) {
                has_local_generics = true;
                break;
            }
        } else if (decl->is<parser::ImplDecl>()) {
            const auto& impl = decl->as<parser::ImplDecl>();
            if (!impl.generics.empty()) {
                has_local_generics = true;
                break;
            }
            if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                const auto& named = impl.self_type->as<parser::NamedType>();
                if (named.generics.has_value() && !named.generics->args.empty()) {
                    has_local_generics = true;
                    break;
                }
            }
        }
    }

    // Use MIR-based codegen when safe, AST codegen otherwise.
    // When Cranelift backend is selected, always use MIR path — library code
    // is compiled separately as runtime objects and linked in.
    bool force_mir = (ctx.options().backend == "cranelift");
    if (force_mir || (!has_tml_imports_needing_codegen && !has_local_generics)) {
        // MIR pipeline — use CodegenBackend abstraction
        auto mir = ctx.mir_build(ck.file_path, ck.module_name);
        if (!mir.success) {
            result.error_message = "MIR build failed";
            if (!mir.errors.empty()) {
                result.error_message = mir.errors[0];
            }
            return result;
        }

        // Build CodegenOptions from query context
        codegen::CodegenOptions codegen_opts;
        codegen_opts.emit_comments = ctx.options().verbose;
        codegen_opts.coverage_enabled = ctx.options().coverage;
#ifdef _WIN32
        codegen_opts.target_triple = "x86_64-pc-windows-msvc";
#else
        codegen_opts.target_triple = "x86_64-unknown-linux-gnu";
#endif
        if (!ctx.options().target_triple.empty()) {
            codegen_opts.target_triple = ctx.options().target_triple;
        }

        // Create backend from options (defaults to LLVM)
        auto backend_type = codegen::default_backend_type();
        if (ctx.options().backend == "cranelift") {
            backend_type = codegen::BackendType::Cranelift;
        }
        auto backend = codegen::create_backend(backend_type);

        if (backend_type == codegen::BackendType::Cranelift) {
            // Cranelift: compile MIR directly to object file (no IR text step)
            auto cg_result = backend->compile_mir(*mir.mir_module, codegen_opts);
            if (!cg_result.success) {
                result.error_message = cg_result.error_message;
                return result;
            }
            result.object_file = cg_result.object_file;
        } else {
            // LLVM: generate IR text (compiled to object later by the build system)
            result.llvm_ir = backend->generate_ir(*mir.mir_module, codegen_opts);
        }

        // Extract link_libs from AST
        for (const auto& decl : parsed.module->decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                for (const auto& lib : func.link_libs) {
                    result.link_libs.insert(lib);
                }
            }
        }
    } else {
        // AST-based codegen fallback
        codegen::LLVMGenOptions llvm_gen_options;
        llvm_gen_options.emit_comments = ctx.options().verbose;
        llvm_gen_options.emit_debug_info = ck.debug_info;
        llvm_gen_options.coverage_enabled = ctx.options().coverage;
        if (!ctx.options().target_triple.empty()) {
            llvm_gen_options.target_triple = ctx.options().target_triple;
        }

        codegen::LLVMIRGen llvm_gen(*tc.env, llvm_gen_options);
        auto gen_result = llvm_gen.generate(*parsed.module);

        if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
            const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
            if (!errors.empty()) {
                result.error_message = errors[0].message;
            } else {
                result.error_message = "Code generation failed";
            }
            return result;
        }

        result.llvm_ir = std::get<std::string>(gen_result);
        result.link_libs = llvm_gen.get_link_libs();
    }

    result.success = true;
    return result;
}

} // namespace tml::query::providers
