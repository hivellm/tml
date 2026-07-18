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
#include "hir/hir_printer.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "log/log.hpp"
#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"
#include "mir/mir_validate.hpp"
#include "mir/passes/infinite_loop_check.hpp"
#include "mir/passes/memory_leak_check.hpp"
#include "mir/passes/pgo.hpp"
#include "mir/thir_mir_builder.hpp"
#include "parser/parser.hpp"
#include "pipeline/tml_stage.hpp"
#include "preprocessor/preprocessor.hpp"
#include "serial/ast_reader.hpp"
#include "serial/token_reader.hpp"
#include "testing/testing_process.hpp"
#include "thir/thir.hpp"
#include "thir/thir_lower.hpp"
#include "traits/solver.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"
#include "types/module_binary.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tml::query::providers {

// ============================================================================
// Helpers
// ============================================================================

/// Locate the running tml executable (duplicated from tml_stage.cpp).
static fs::path get_self_exe_path() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return fs::path(std::string(buf, len));
    }
#else
    std::error_code ec;
    auto exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe;
    }
#endif
    return {};
}

/// Find the TML frontend binary (main_frontend.exe) used by --stage=parser:tml.
/// Searches relative to the running tml.exe in well-known build output paths.
static fs::path find_tml_frontend_exe() {
    auto self = get_self_exe_path();
    if (self.empty()) {
        return {};
    }
    auto bin_dir = self.parent_path();      // e.g. build/debug/bin/
    auto build_dir = bin_dir.parent_path(); // e.g. build/debug/

    std::error_code ec;
    // Primary: build/debug/main_frontend.exe (produced by tml build)
    auto candidate = build_dir / "main_frontend.exe";
    if (fs::exists(candidate, ec)) {
        return candidate;
    }
    // Fallback: same directory as tml.exe
    candidate = bin_dir / "main_frontend.exe";
    if (fs::exists(candidate, ec)) {
        return candidate;
    }
    // Fallback: TmlStage cache
    candidate = build_dir / "cache" / "stages" / "parser.exe";
    if (fs::exists(candidate, ec)) {
        return candidate;
    }
    return {};
}

// ============================================================================
// Pipeline Dump Helper
// ============================================================================

// Write content to a pipeline dump file, creating parent directories as needed.
static void pipeline_write(const std::string& dir, const std::string& filename,
                           const std::string& content) {
    if (dir.empty())
        return;
    fs::create_directories(dir);
    std::ofstream f(fs::path(dir) / filename, std::ios::out | std::ios::trunc);
    if (f.is_open()) {
        f << content;
    }
}

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
            TML_LOG_ERROR("query", "[Q005] Source file not found: " << rk.file_path);
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

    // phase12f hybrid pipeline: if --stage=lexer:tml is active, delegate
    // tokenization to a TML subprocess and deserialize its TMLL output.
    {
        const auto& overrides = ctx.options().stage_overrides;
        auto it = overrides.find("lexer");
        if (it != overrides.end() && it->second == "tml") {
            tml::pipeline::TmlStage stage("lexer");
            std::vector<uint8_t> input(src.preprocessed.begin(), src.preprocessed.end());
            auto run = stage.run(input);
            if (!run.success) {
                result.errors.push_back("lexer stage: " + run.error);
                if (!run.stderr_text.empty()) {
                    result.errors.push_back(run.stderr_text);
                }
                return result;
            }
            return tml::serial::read_tokens(run.stdout_bytes);
        }
    }

    // Tokenize — store Source in result so token string_views stay valid
    auto source =
        std::make_shared<lexer::Source>(lexer::Source::from_string(src.preprocessed, tk.file_path));
    lexer::Lexer lex(*source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& err : lex.errors()) {
            std::string fmt;
            if (err.span.start.line > 0) {
                fmt = std::string(err.span.start.file) + ":" + std::to_string(err.span.start.line) +
                      ":" + std::to_string(err.span.start.column) + ": ";
            }
            if (!err.code.empty())
                fmt += "[" + err.code + "] ";
            fmt += err.message;
            result.errors.push_back(std::move(fmt));
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

    // phase13d hybrid pipeline: if --stage=parser:tml is active, delegate
    // lexing+parsing to the TML frontend subprocess (main_frontend.exe) and
    // deserialize its binary AST output.
    {
        const auto& overrides = ctx.options().stage_overrides;
        auto it = overrides.find("parser");
        if (it != overrides.end() && it->second == "tml") {
            auto frontend_exe = find_tml_frontend_exe();
            if (frontend_exe.empty()) {
                result.errors.push_back(
                    "TML frontend binary not found (main_frontend.exe). "
                    "Build it first: tml build compiler-tml/src/main_frontend.tml");
                return result;
            }

            tml::testing::ProcessOptions opts;
            opts.exe_path = frontend_exe.string();
            opts.args = {pk.file_path};
            opts.timeout = std::chrono::milliseconds(30'000);

            auto proc = tml::testing::Process::launch(opts);
            if (!proc) {
                result.errors.push_back("Failed to launch TML frontend subprocess: " +
                                        frontend_exe.string());
                return result;
            }
            auto run = proc->wait();

            if (run.exit_code != 0) {
                // stderr contains JSON error array from the TML frontend.
                // If stderr is empty the frontend crashed (K001 / runtime bug):
                // fall through to the C++ parser as a silent fallback so that
                // `tml build` keeps working even when main_frontend.exe has
                // runtime issues. Only report the error when stderr is present
                // (i.e. it's a real parse error the user should see).
                if (!run.stderr_output.empty()) {
                    result.errors.push_back(run.stderr_output);
                    return result;
                }
                // Empty stderr means crash — fall through to C++ parser below.
            } else {
                // Deserialize binary AST from stdout
                try {
                    std::vector<uint8_t> bytes(run.stdout_output.begin(), run.stdout_output.end());
                    // read_ast injects file_path into every SourceLocation::file
                    // (a string_view), so we must keep a stable copy alive.
                    auto path_storage = std::make_shared<std::string>(pk.file_path);
                    parser::Module mod = serial::read_ast(bytes, *path_storage);
                    result.module =
                        std::shared_ptr<parser::Module>(new parser::Module(std::move(mod)),
                                                        [path_storage](parser::Module* p) mutable {
                                                            delete p;
                                                            path_storage.reset();
                                                        });
                    result.success = true;
                } catch (const std::exception& e) {
                    result.errors.push_back(std::string("AST deserialization failed: ") + e.what());
                }
                return result;
            }
            // TML frontend crashed without diagnostics — fall through to C++ parser.
        }
    }

    // F-027: validated `<source>.ast.bin` sidecar fast-path.
    // A sibling AST-binary cache (produced by the TML frontend) can skip
    // lex+parse, but ONLY when it is at least as new as the source file — so an
    // edited source is never silently overridden by a stale cached AST. The
    // `.ast.bin` format is written by the frozen TML frontend
    // (lib/std/src/serial/ast.tml) and embeds no source hash, so freshness is
    // validated by mtime. The ReadSource dependency is recorded either way
    // (via ctx.read_source) so red-green invalidation stays correct, and the
    // result flows through the query cache instead of being re-deserialized on
    // every call (the old QueryContext::parse_module short-circuit did both
    // wrong). A corrupt/incompatible sidecar falls through to a real parse
    // rather than breaking a valid compile.
    {
        const std::string cache_path = pk.file_path + ".ast.bin";
        std::error_code ec_exist;
        if (fs::exists(cache_path, ec_exist) && !ec_exist) {
            auto src = ctx.read_source(pk.file_path); // records ReadSource dep + fingerprint
            std::error_code ec_src, ec_bin;
            auto src_mtime = fs::last_write_time(pk.file_path, ec_src);
            auto bin_mtime = fs::last_write_time(cache_path, ec_bin);
            const bool fresh = src.success && !ec_src && !ec_bin && src_mtime <= bin_mtime;
            if (fresh) {
                try {
                    std::ifstream in(cache_path, std::ios::binary);
                    if (in) {
                        in.seekg(0, std::ios::end);
                        const auto sz = static_cast<std::streamsize>(in.tellg());
                        in.seekg(0, std::ios::beg);
                        std::vector<uint8_t> bytes(static_cast<size_t>(sz));
                        if (sz > 0) {
                            in.read(reinterpret_cast<char*>(bytes.data()), sz);
                        }
                        // read_ast injects file_path into every SourceLocation::file
                        // (a string_view); keep a stable copy alive for the module.
                        auto path_storage = std::make_shared<std::string>(pk.file_path);
                        parser::Module mod = serial::read_ast(bytes, *path_storage);
                        result.module = std::shared_ptr<parser::Module>(
                            new parser::Module(std::move(mod)),
                            [path_storage](parser::Module* p) mutable {
                                delete p;
                                path_storage.reset();
                            });
                        result.success = true;
                        return result;
                    }
                } catch (const std::exception& e) {
                    TML_LOG_WARN("query", "ignoring unreadable .ast.bin sidecar for "
                                              << pk.file_path << ": " << e.what());
                }
            } else if (src.success) {
                TML_LOG_DEBUG("query", "stale .ast.bin sidecar ignored (source newer): "
                                           << pk.file_path);
            }
        }
    }

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
            std::string fmt;
            if (err.span.start.line > 0) {
                fmt = std::string(err.span.start.file) + ":" + std::to_string(err.span.start.line) +
                      ":" + std::to_string(err.span.start.column) + ": ";
            }
            if (!err.code.empty())
                fmt += "[" + err.code + "] ";
            fmt += err.message;
            for (const auto& note : err.notes)
                fmt += "\n  note: " + note;
            result.errors.push_back(std::move(fmt));
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
            if (err.is_cascading)
                continue; // suppress redundant follow-on errors
            std::string fmt;
            if (err.span.start.line > 0) {
                fmt = std::string(err.span.start.file) + ":" + std::to_string(err.span.start.line) +
                      ":" + std::to_string(err.span.start.column) + ": ";
            }
            if (!err.code.empty())
                fmt += "[" + err.code + "] ";
            fmt += err.message;
            for (const auto& note : err.notes)
                fmt += "\n  note: " + note;
            result.errors.push_back(std::move(fmt));
        }
        return result;
    }

    // Emit type checker warnings to stderr (non-blocking)
    if (checker.has_warnings()) {
        for (const auto& w : checker.warnings()) {
            TML_LOG_WARN("typecheck", w.span.start.file << ":" << w.span.start.line << ":"
                                                        << w.span.start.column << ": " << w.code
                                                        << ": " << w.message);
        }
    }

    result.env =
        std::make_shared<types::TypeEnv>(std::move(std::get<types::TypeEnv>(check_result)));
    result.registry = registry;
    result.success = true;

    // Phase 8.5 W5: register every transitively-loaded `.tml` source as a
    // ReadSource query dependency of this TypecheckModule. The query system
    // hashes each ReadSource's content and folds it into TypecheckModule's
    // input fingerprint via `compute_input_fingerprint` (which combines the
    // output fingerprints of all recorded dependencies). When the user edits
    // any of those files, the next run computes a different input fingerprint
    // and `verify_all_inputs_green` returns false, so the cached IR is rebuilt.
    //
    // Without this loop, library and package modules loaded via the legacy
    // `load_native_module → load_module_from_file → ifstream` path would not
    // appear in the dependency graph at all, and edits to them would silently
    // replay broken IR until `cache/incr` was deleted manually (the W5 bug).
    if (result.env) {
        for (const auto& src : result.env->loaded_source_files()) {
            // Skip the main file — it's already registered via parse_module → tokenize →
            // read_source.
            if (src == tk.file_path) {
                continue;
            }
            // ctx.read_source() force-evaluates the ReadSource query if not
            // already cached, records this query as a dep on the active query
            // (i.e. TypecheckModule), and produces a stable fingerprint for
            // the file content.
            ctx.read_source(src);
        }
    }

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
        // Export per-binding ownership facts BEFORE the stack-local checker is
        // destroyed (phase26b Step 2.1). These flow to the AST codegen via the
        // cached BorrowcheckResult (the codegen_unit -> borrowcheck query edge
        // already exists) and become the basis for fact-based drop suppression.
        result.ownership = borrow_checker.ownership_facts();
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

    if (hir_builder.has_errors()) {
        // HIR lowering produced errors — log them and mark as failed
        for (const auto& err : hir_builder.errors()) {
            TML_LOG_ERROR("query", "[" << err.code << "] " << err.message);
        }
        return result;
    }

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

    const std::string& pipeline_dir = ctx.options().pipeline_output_dir;

    // THIR → MIR pipeline (consolidated — HIR path removed)
    auto thir = ctx.thir_lower(mk.file_path, mk.module_name);
    if (!thir.success) {
        return result;
    }

    // Report THIR diagnostics (exhaustiveness warnings, etc.)
    for (const auto& diag : thir.diagnostics) {
        TML_LOG_WARN("thir", diag);
    }

    // Dump THIR summary
    if (!pipeline_dir.empty() && thir.thir_module) {
        std::string thir_dump = "THIR Module: " + mk.module_name + "\n";
        thir_dump += "Functions (" + std::to_string(thir.thir_module->functions.size()) + "):\n";
        for (size_t i = 0; i < thir.thir_module->functions.size(); ++i) {
            thir_dump +=
                "  [" + std::to_string(i) + "] " + thir.thir_module->functions[i].name + "\n";
        }
        thir_dump += "Structs (" + std::to_string(thir.thir_module->structs.size()) + ")\n";
        thir_dump += "Enums (" + std::to_string(thir.thir_module->enums.size()) + ")\n";
        pipeline_write(pipeline_dir, mk.module_name + ".thir", thir_dump);
    }

    // Build MIR from THIR
    mir::ThirMirBuilder thir_mir_builder(*tc.env);
    mir_module = thir_mir_builder.build(*thir.thir_module);

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

    // Apply MIR optimizations (even O0 runs basic strength reduction)
    int opt_level = ctx.options().optimization_level;
    {
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

        // Configure pipeline dump if enabled
        if (!pipeline_dir.empty()) {
            pm.set_pipeline_dir(pipeline_dir, mk.module_name);
        }

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

    // Run MIR validation pass — abort on null types (like Rust ICE).
    // A null type in MIR means the MIR builder has a bug. Silently falling back
    // to i32 masks the bug and produces wrong code. Abort immediately.
    {
        auto vr = mir::validate_module(mir_module);
        if (!vr.is_clean()) {
            for (const auto& w : vr.warnings) {
                TML_LOG_ERROR("mir", w);
            }
            TML_LOG_ERROR("mir", "[ICE] MIR validation failed for '"
                                     << mk.module_name << "': " << vr.null_type_count
                                     << " null type(s), " << vr.missing_terminator_count
                                     << " missing terminator(s), " << vr.empty_block_count
                                     << " empty block(s). "
                                     << "This is a compiler bug — please report it.");
            result.success = false;
            return result;
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

    // Check if there are local generic types or usage of imported generic enums
    // (MIR codegen doesn't support generic enum construction yet)
    bool has_local_generics = false;

    // Helper: check if a parser type references a generic type whose monomorphization
    // is only handled by the AST codegen path (`generate_pending_instantiations`).
    // The MIR codegen path skips that worklist, so any reference to one of these
    // generic types from imported modules would produce undefined symbols at link
    // time (e.g. `Arc__I32::new`, `Maybe__I32::is_just`, etc.).
    //
    // This set covers built-in generic enums plus the std::sync / std::collections
    // generic wrappers that are commonly used and depend on the legacy instantiator.
    static const std::unordered_set<std::string> generic_ast_only_names = {
        // Built-in generic enums
        "Maybe",
        "Outcome",
        "Poll",
        // std::sync wrappers
        "Arc",
        "Rc",
        "Weak",
        "Mutex",
        "RwLock",
        "Shared",
        "Sync",
        // smart pointers / cells
        "Box",
        "Pin",
        "Cell",
        "RefCell",
        "Heap",
        // Generic collections that monomorphize through the legacy path
        "Vec",
        "BTreeMap",
        "BTreeSet",
        "HashSet",
    };
    std::function<bool(const parser::TypePtr&)> uses_generic_enum;
    uses_generic_enum = [&](const parser::TypePtr& type) -> bool {
        if (!type)
            return false;
        if (type->is<parser::NamedType>()) {
            const auto& named = type->as<parser::NamedType>();
            if (named.generics.has_value() && !named.generics->args.empty()) {
                if (!named.path.segments.empty() &&
                    generic_ast_only_names.count(named.path.segments.back()))
                    return true;
                // Recurse into generic arguments — nested generic types like
                // `Outcome[I32, Arc[I32]]` or `List[Maybe[Str]]` must also force fallback.
                for (const auto& arg : named.generics->args) {
                    if (arg.is_type() && uses_generic_enum(arg.as_type()))
                        return true;
                }
            }
        }
        return false;
    };

    // Helper: check if a struct/enum decorator list contains a `@derive(...)` whose
    // emitter lives only in the AST codegen path (gen_derive_*). The MIR codegen
    // path doesn't run these derive emitters, so the resulting symbols (e.g.
    // `Point::runtime_type_info`, `Point_from_json`, `DefaultPoint_default`) would
    // never be defined and would fail at link time.
    static const std::unordered_set<std::string> ast_only_derive_names = {
        "Reflect",
        "Default",
        "FromJson",
        "ToJson",
    };
    auto has_ast_only_derive = [](const std::vector<parser::Decorator>& decorators) -> bool {
        for (const auto& dec : decorators) {
            if (dec.name != "derive")
                continue;
            for (const auto& arg : dec.args) {
                if (!arg)
                    continue;
                // Derive args are typically IdentExpr or PathExpr nodes naming the trait.
                if (arg->is<parser::IdentExpr>()) {
                    if (ast_only_derive_names.count(arg->as<parser::IdentExpr>().name))
                        return true;
                } else if (arg->is<parser::PathExpr>()) {
                    const auto& pe = arg->as<parser::PathExpr>();
                    if (!pe.path.segments.empty() &&
                        ast_only_derive_names.count(pe.path.segments.back()))
                        return true;
                }
            }
        }
        return false;
    };

    for (const auto& decl : parsed.module->decls) {
        if (decl->is<parser::UnionDecl>()) {
            // Unions are only supported in the AST codegen path (gen_union_decl).
            // Force AST codegen by setting has_local_generics = true.
            has_local_generics = true;
            break;
        } else if (decl->is<parser::StructDecl>()) {
            const auto& sd = decl->as<parser::StructDecl>();
            if (!sd.generics.empty()) {
                has_local_generics = true;
                break;
            }
            // @derive(Reflect/Default/FromJson/ToJson) requires the AST derive
            // emitters — MIR codegen does not run them.
            if (has_ast_only_derive(sd.decorators)) {
                has_local_generics = true;
                break;
            }
            // Field types may reference imported generic wrappers (e.g. Arc[T]).
            for (const auto& field : sd.fields) {
                if (uses_generic_enum(field.type)) {
                    has_local_generics = true;
                    break;
                }
            }
            if (has_local_generics)
                break;
        } else if (decl->is<parser::EnumDecl>()) {
            const auto& ed = decl->as<parser::EnumDecl>();
            if (!ed.generics.empty()) {
                has_local_generics = true;
                break;
            }
            if (has_ast_only_derive(ed.decorators)) {
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
        } else if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            // Generic functions require AST codegen for monomorphization
            if (!func.generics.empty()) {
                has_local_generics = true;
                break;
            }
            // Check if function uses generic enum types in signature or body
            if (func.return_type.has_value() && uses_generic_enum(*func.return_type)) {
                has_local_generics = true;
                break;
            }
            for (const auto& param : func.params) {
                if (uses_generic_enum(param.type)) {
                    has_local_generics = true;
                    break;
                }
            }
            // Also check let/var type annotations in function body
            if (!has_local_generics && func.body.has_value()) {
                for (const auto& stmt : func.body->stmts) {
                    if (stmt->is<parser::LetStmt>()) {
                        const auto& let_stmt = stmt->as<parser::LetStmt>();
                        if (let_stmt.type_annotation.has_value() &&
                            uses_generic_enum(*let_stmt.type_annotation)) {
                            has_local_generics = true;
                            break;
                        }
                    }
                }
            }
            if (has_local_generics)
                break;
        }
    }

    // Use MIR-based codegen when safe, AST codegen otherwise.
    // Template literals are now handled natively by MIR (inline to_string + str_concat_opt).
    // When Cranelift backend is selected, always use MIR path.
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
        // Generate the C entry point (@main wrapper) for standalone executables,
        // or tml_test_N entry for v3 test suites.
        codegen_opts.generate_exe_main = ctx.options().generate_exe_main;
        if (!ctx.options().generate_exe_main && ctx.options().test_entry_index >= 0) {
            // F-023: name the entry from a FILE-STABLE id (not the suite
            // position) so the emitted IR is identical regardless of where this
            // file sits in the suite. The dispatcher computes the same id from
            // the same file_path and declares/calls this symbol.
            codegen_opts.test_entry_name =
                "tml_test_" + std::to_string(stable_test_symbol_id(ck.file_path));
        } else if (!ctx.options().generate_exe_main) {
            codegen_opts.test_entry_name = "tml_test_entry";
        }
        // Force internal linkage when compiling as part of a suite (same as AST path).
        if (!ctx.options().generate_exe_main) {
            codegen_opts.force_internal_linkage = true;
        }
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

            // Dump LLVM IR if pipeline dump is enabled
            if (!ctx.options().pipeline_output_dir.empty()) {
                pipeline_write(ctx.options().pipeline_output_dir, ck.module_name + ".ll",
                               result.llvm_ir);
            }
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
        llvm_gen_options.lazy_library_defs = true; // Only emit library defs actually used
        if (!ctx.options().target_triple.empty()) {
            llvm_gen_options.target_triple = ctx.options().target_triple;
        }
        // v3 test system: generate tml_test_N entry instead of main
        if (!ctx.options().generate_exe_main) {
            llvm_gen_options.generate_dll_entry = true;
            // F-023: the `s{id}_` internal-symbol prefix and the `tml_test_<id>`
            // entry use a FILE-STABLE id (not the suite position), so the module
            // body is position-independent and its CodegenUnit cache entry stays
            // GREEN across suite membership changes.
            llvm_gen_options.suite_test_index =
                static_cast<int>(stable_test_symbol_id(ck.file_path));
            // Force internal linkage when compiling as part of a suite.
            // Multiple test files in the same suite each emit library function
            // definitions; internal linkage prevents duplicate symbol errors
            // at link time.
            llvm_gen_options.force_internal_linkage = true;
        }
        // Pre-compiled stdlib: skip emit_module_pure_tml_functions() and use
        // declarations only. The full definitions come from a pre-compiled .obj.
        if (ctx.options().cached_library_state) {
            llvm_gen_options.cached_library_state = ctx.options().cached_library_state;
            llvm_gen_options.library_decls_only = ctx.options().library_decls_only;
        }

        codegen::LLVMIRGen llvm_gen(*tc.env, llvm_gen_options);
        // Install borrow-checker ownership facts (phase26b Step 2.2). Facts come
        // from the cached BorrowcheckResult forced at the top of this provider.
        llvm_gen.set_ownership_facts(bc.ownership);
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
