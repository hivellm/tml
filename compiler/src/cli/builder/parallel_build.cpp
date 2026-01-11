//! # Parallel Build System
//!
//! This file implements multi-threaded compilation for the TML compiler.
//! It coordinates parallel compilation of multiple source files while
//! respecting inter-module dependencies.
//!
//! ## Architecture
//!
//! ```text
//! ParallelBuilder
//!   ├─ DependencyGraph      # Tracks file dependencies (DAG)
//!   ├─ BuildQueue           # Thread-safe job queue
//!   └─ Worker threads       # Parallel compilation workers
//!
//! Build Flow:
//! 1. discover_source_files() - Find all .tml files
//! 2. resolve_dependencies()  - Parse imports, build DAG
//! 3. worker_thread()         - Compile files in parallel
//!    └─ compile_job()        - Lex → Parse → Check → Codegen → Object
//! ```
//!
//! ## Dependency Resolution
//!
//! Files are compiled in topological order based on `use` statements:
//! - Files with no dependencies compile first
//! - When a file completes, dependents become ready
//! - Circular dependencies fall back to sequential build
//!
//! ## Thread Safety
//!
//! | Component        | Synchronization                          |
//! |------------------|------------------------------------------|
//! | DependencyGraph  | Mutex-protected maps                     |
//! | BuildQueue       | Mutex + condition variable               |
//! | BuildStats       | Atomic counters                          |
//! | LLVM IR files    | Thread-unique filenames                  |

#include "parallel_build.hpp"

#include "borrow/checker.hpp"
#include "cli/builder/compiler_setup.hpp"
#include "cli/builder/object_compiler.hpp"
#include "cli/commands/cmd_build.hpp"
#include "cli/utils.hpp"
#include "codegen/llvm_ir_gen.hpp"
#include "common.hpp"
#include "hir/hir.hpp"
#include "hir/hir_builder.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/hir_mir_builder.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

namespace tml::cli {

// ============================================================================
// DependencyGraph Implementation
// ============================================================================
//
// The dependency graph tracks which files depend on which other files.
// It maintains:
// - deps_: file → [dependencies]
// - rdeps_: file → [dependents] (reverse mapping)
// - pending_count_: file → number of unfinished dependencies
// - completed_: set of finished files

/// Adds a file and its dependencies to the graph.
///
/// Only internal dependencies (files being built) are counted.
/// External dependencies (stdlib, etc.) are ignored.
void DependencyGraph::add_file(const std::string& file, const std::vector<std::string>& deps) {
    std::lock_guard<std::mutex> lock(mutex_);

    deps_[file] = deps;
    pending_count_[file] = 0;

    // Count only internal dependencies (files we're building)
    for (const auto& dep : deps) {
        if (deps_.find(dep) != deps_.end()) {
            pending_count_[file]++;
            rdeps_[dep].push_back(file);
        }
    }
}

/// Returns files that are ready to compile (no pending dependencies).
std::vector<std::string> DependencyGraph::get_ready_files() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> ready;
    for (const auto& [file, count] : pending_count_) {
        if (count == 0 && completed_.find(file) == completed_.end()) {
            ready.push_back(file);
        }
    }
    return ready;
}

/// Marks a file as completed and notifies its dependents.
///
/// When a file completes, all files that depend on it have their
/// pending count decremented. This may make them ready to compile.
void DependencyGraph::mark_complete(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);

    completed_.insert(file);

    // Decrement pending count for all dependents
    auto it = rdeps_.find(file);
    if (it != rdeps_.end()) {
        for (const auto& dependent : it->second) {
            if (pending_count_.find(dependent) != pending_count_.end()) {
                pending_count_[dependent]--;
            }
        }
    }
}

bool DependencyGraph::all_complete() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completed_.size() == deps_.size();
}

/// Detects circular dependencies using DFS.
///
/// Returns true if any cycle is found in the dependency graph.
/// Cycles prevent topological ordering and require fallback to
/// sequential compilation.
bool DependencyGraph::has_cycles() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Use DFS to detect cycles
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> rec_stack;

    std::function<bool(const std::string&)> has_cycle_dfs = [&](const std::string& node) -> bool {
        visited.insert(node);
        rec_stack.insert(node);

        auto it = deps_.find(node);
        if (it != deps_.end()) {
            for (const auto& dep : it->second) {
                // Only check internal dependencies
                if (deps_.find(dep) == deps_.end())
                    continue;

                if (rec_stack.find(dep) != rec_stack.end()) {
                    return true; // Cycle found
                }
                if (visited.find(dep) == visited.end() && has_cycle_dfs(dep)) {
                    return true;
                }
            }
        }

        rec_stack.erase(node);
        return false;
    };

    for (const auto& [file, _] : deps_) {
        if (visited.find(file) == visited.end()) {
            if (has_cycle_dfs(file)) {
                return true;
            }
        }
    }

    return false;
}

std::vector<std::string> DependencyGraph::topological_sort() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> temp_visited;

    std::function<void(const std::string&)> visit = [&](const std::string& node) {
        if (visited.find(node) != visited.end())
            return;
        if (temp_visited.find(node) != temp_visited.end())
            return; // Cycle, skip

        temp_visited.insert(node);

        auto it = deps_.find(node);
        if (it != deps_.end()) {
            for (const auto& dep : it->second) {
                if (deps_.find(dep) != deps_.end()) {
                    visit(dep);
                }
            }
        }

        temp_visited.erase(node);
        visited.insert(node);
        result.push_back(node);
    };

    for (const auto& [file, _] : deps_) {
        visit(file);
    }

    return result;
}

// ============================================================================
// BuildQueue Implementation
// ============================================================================
//
// Thread-safe queue for build jobs. Workers wait on the condition
// variable until a job is available or the stop flag is set.

/// Pushes a job to the queue and notifies waiting workers.
void BuildQueue::push(std::shared_ptr<BuildJob> job) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push(job);
    cv.notify_one();
}

/// Pops a job from the queue, waiting up to timeout_ms.
///
/// Returns nullptr if the queue is empty after the timeout.
std::shared_ptr<BuildJob> BuildQueue::pop(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex);

    if (queue.empty()) {
        cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [this] { return !queue.empty() || stop_flag; });
    }

    if (queue.empty()) {
        return nullptr;
    }

    auto job = queue.front();
    queue.pop();
    return job;
}

void BuildQueue::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    stop_flag = true;
    cv.notify_all();
}

bool BuildQueue::is_empty() {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
}

size_t BuildQueue::size() {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
}

// ============================================================================
// ParallelBuilder Implementation
// ============================================================================
//
// Main orchestrator for parallel builds. Manages the dependency graph,
// job queue, and worker threads.

/// Constructs a ParallelBuilder with the specified thread count.
///
/// If num_threads is 0, defaults to 8 threads.
ParallelBuilder::ParallelBuilder(int num_threads) : num_threads(num_threads) {
    if (this->num_threads == 0) {
        // Default to 8 threads for optimal parallel performance
        this->num_threads = 8;
    }
}

void ParallelBuilder::add_file(const fs::path& source_file, const fs::path& output_file) {
    auto job = std::make_shared<BuildJob>();
    job->source_file = source_file;
    job->output_file = output_file;
    job->content_hash = generate_hash(source_file);
    jobs.push_back(job);
    job_map[source_file.string()] = job;
}

std::string ParallelBuilder::generate_hash(const fs::path& source_file) {
    try {
        std::ifstream file(source_file, std::ios::binary);
        if (!file)
            return "";

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        std::hash<std::string> hasher;
        size_t hash = hasher(content);

        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << hash;
        return oss.str();
    } catch (...) {
        return "";
    }
}

std::vector<std::string> ParallelBuilder::parse_imports(const fs::path& source_file) {
    std::vector<std::string> imports;

    try {
        std::ifstream file(source_file);
        if (!file)
            return imports;

        std::string line;
        // Simple regex to match `use module::path` or `use <local_module>`
        std::regex use_regex(R"(^\s*use\s+([a-zA-Z_][a-zA-Z0-9_:]*)\s*;?)");
        std::regex use_local_regex(R"(^\s*use\s+<([a-zA-Z_][a-zA-Z0-9_]*)>\s*;?)");

        while (std::getline(file, line)) {
            std::smatch match;
            if (std::regex_search(line, match, use_regex)) {
                imports.push_back(match[1].str());
            } else if (std::regex_search(line, match, use_local_regex)) {
                // Local module reference
                imports.push_back(match[1].str());
            }
        }
    } catch (...) {
        // Ignore parse errors for dependency extraction
    }

    return imports;
}

void ParallelBuilder::resolve_dependencies() {
    // Parse imports from all source files
    for (auto& job : jobs) {
        job->dependencies = parse_imports(job->source_file);
    }

    // Build a module name to file path mapping
    std::unordered_map<std::string, std::string> module_to_file;
    for (const auto& job : jobs) {
        std::string module_name = job->source_file.stem().string();
        module_to_file[module_name] = job->source_file.string();
    }

    // Add files to dependency graph
    for (const auto& job : jobs) {
        std::vector<std::string> file_deps;
        for (const auto& dep : job->dependencies) {
            // Check if this is a local file dependency
            auto it = module_to_file.find(dep);
            if (it != module_to_file.end() && it->second != job->source_file.string()) {
                file_deps.push_back(it->second);
            }
        }
        dep_graph.add_file(job->source_file.string(), file_deps);
    }

    // Check for circular dependencies
    if (dep_graph.has_cycles()) {
        std::cerr << "Warning: Circular dependencies detected, falling back to sequential build\n";
        // Queue all files anyway
        for (auto& job : jobs) {
            if (!job->queued) {
                job->queued = true;
                ready_queue.push(job);
            }
        }
        return;
    }

    // Queue files with no dependencies first
    auto ready = dep_graph.get_ready_files();
    for (const auto& file : ready) {
        auto it = job_map.find(file);
        if (it != job_map.end() && !it->second->queued) {
            it->second->queued = true;
            ready_queue.push(it->second);
        }
    }
}

bool ParallelBuilder::is_cached(std::shared_ptr<BuildJob> job) {
    if (options.no_cache) {
        return false;
    }

    // Check if object file exists
    if (!fs::exists(job->output_file)) {
        return false;
    }

    // Check if object file is newer than source
    try {
        auto src_time = fs::last_write_time(job->source_file);
        auto obj_time = fs::last_write_time(job->output_file);
        return obj_time >= src_time;
    } catch (...) {
        return false;
    }
}

bool ParallelBuilder::build(bool verbose) {
    if (jobs.empty()) {
        return true;
    }

    stats.reset();
    stats.total_files = static_cast<int>(jobs.size());
    options.verbose = verbose;

    // Resolve dependencies and populate ready queue
    resolve_dependencies();

    if (ready_queue.is_empty() && !jobs.empty()) {
        // All files have dependencies - could be a cycle or external deps only
        // Queue all files
        for (auto& job : jobs) {
            if (!job->queued) {
                job->queued = true;
                ready_queue.push(job);
            }
        }
    }

    // Launch worker threads
    std::vector<std::thread> workers;
    int actual_threads = std::min(static_cast<int>(jobs.size()), num_threads);

    if (verbose) {
        std::cout << "Compiling " << jobs.size() << " files with " << actual_threads
                  << " threads...\n";
    }

    for (int i = 0; i < actual_threads; ++i) {
        workers.emplace_back(&ParallelBuilder::worker_thread, this, verbose);
    }

    // Wait for all workers to finish
    for (auto& worker : workers) {
        worker.join();
    }

    // Check results
    bool success = (stats.failed == 0);

    if (verbose || !success) {
        // Print summary
        std::cout << "\nBuild summary:\n";
        std::cout << "  Total: " << stats.total_files << " files\n";
        std::cout << "  Compiled: " << stats.completed << " files\n";
        std::cout << "  Cached: " << stats.cached << " files\n";
        if (stats.failed > 0) {
            std::cout << "  Failed: " << stats.failed << " files\n";
        }
        std::cout << "  Time: " << (stats.elapsed_ms() / 1000.0) << "s\n";
    }

    return success;
}

void ParallelBuilder::worker_thread(bool verbose) {
    while (true) {
        auto job = ready_queue.pop(100);
        if (!job) {
            // Check if there are more jobs pending
            std::lock_guard<std::mutex> lock(job_mutex);
            if (dep_graph.all_complete() || (stats.completed + stats.failed >= stats.total_files)) {
                break;
            }
            continue;
        }

        // Check cache first
        if (is_cached(job)) {
            job->completed = true;
            job->cached = true;
            stats.cached++;
            stats.completed++;

            if (verbose) {
                std::cout << "[cached] " << job->source_file.filename().string() << "\n";
            }

            notify_dependents(job);
            continue;
        }

        if (!compile_job(job, verbose)) {
            job->failed = true;
            stats.failed++;
        } else {
            job->completed = true;
            stats.completed++;
            notify_dependents(job);
        }
    }
}

void ParallelBuilder::notify_dependents(std::shared_ptr<BuildJob> job) {
    dep_graph.mark_complete(job->source_file.string());

    // Check if any new files are now ready
    auto ready = dep_graph.get_ready_files();
    for (const auto& file : ready) {
        auto it = job_map.find(file);
        if (it != job_map.end() && !it->second->queued && !it->second->completed &&
            !it->second->failed) {
            it->second->queued = true;
            ready_queue.push(it->second);
        }
    }
}

/// Compiles a single source file through the full pipeline.
///
/// ## Compilation Pipeline
///
/// 1. **Lexing**: Tokenize source code
/// 2. **Parsing**: Build AST from tokens
/// 3. **Type Checking**: Verify types and resolve symbols
/// 4. **Borrow Checking**: Verify ownership rules
/// 5. **Code Generation**: Generate LLVM IR
/// 6. **Object Compilation**: Compile IR to native object file
///
/// Thread-safe: Uses unique temporary filenames per thread.
bool ParallelBuilder::compile_job(std::shared_ptr<BuildJob> job, bool verbose) {
    if (verbose) {
        std::cout << "[" << (stats.completed + stats.failed + 1) << "/" << stats.total_files
                  << "] Compiling " << job->source_file.filename().string() << "\n";
    }

    try {
        // Read source file
        std::string source_code = read_file(job->source_file.string());

        // Lexical analysis
        auto source = lexer::Source::from_string(source_code, job->source_file.string());
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();

        if (lex.has_errors()) {
            std::ostringstream err;
            for (const auto& error : lex.errors()) {
                err << job->source_file.string() << ":" << error.span.start.line << ":"
                    << error.span.start.column << ": error: " << error.message << "\n";
            }
            job->error_message = err.str();
            if (verbose) {
                std::cerr << job->error_message;
            }
            return false;
        }

        // Parsing
        parser::Parser parser(std::move(tokens));
        auto module_name = job->source_file.stem().string();
        auto parse_result = parser.parse_module(module_name);

        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
            std::ostringstream err;
            const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
            for (const auto& error : errors) {
                err << job->source_file.string() << ":" << error.span.start.line << ":"
                    << error.span.start.column << ": error: " << error.message << "\n";
            }
            job->error_message = err.str();
            if (verbose) {
                std::cerr << job->error_message;
            }
            return false;
        }

        const auto& module = std::get<parser::Module>(parse_result);

        // Type checking
        auto registry = std::make_shared<types::ModuleRegistry>();
        types::TypeChecker checker;
        checker.set_module_registry(registry);

        auto source_dir = job->source_file.parent_path();
        if (source_dir.empty()) {
            source_dir = fs::current_path();
        }
        checker.set_source_directory(source_dir.string());

        auto check_result = checker.check_module(module);

        if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
            std::ostringstream err;
            const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
            for (const auto& error : errors) {
                err << job->source_file.string() << ":" << error.span.start.line << ":"
                    << error.span.start.column << ": error: " << error.message << "\n";
            }
            job->error_message = err.str();
            if (verbose) {
                std::cerr << job->error_message;
            }
            return false;
        }

        const auto& env = std::get<types::TypeEnv>(check_result);

        // Borrow checking
        borrow::BorrowChecker borrow_checker;
        auto borrow_result = borrow_checker.check_module(module);

        if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
            std::ostringstream err;
            const auto& errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
            for (const auto& error : errors) {
                err << job->source_file.string() << ":" << error.span.start.line << ":"
                    << error.span.start.column << ": borrow error: " << error.message << "\n";
            }
            job->error_message = err.str();
            if (verbose) {
                std::cerr << job->error_message;
            }
            return false;
        }

        // Optional HIR pipeline: AST → HIR → MIR
        if (options.use_hir) {
            auto env_copy = env;
            hir::HirBuilder hir_builder(env_copy);
            auto hir_module = hir_builder.lower_module(module);

            mir::HirMirBuilder hir_mir_builder(env);
            auto mir_module = hir_mir_builder.build(hir_module);
            // TODO: Use MIR for code generation when MIR→LLVM backend is ready
            // For now, HIR is just validated but codegen still uses AST
        }

        // Code generation (from AST for now, MIR backend planned)
        codegen::LLVMGenOptions gen_options;
        gen_options.emit_comments = verbose;
        codegen::LLVMIRGen llvm_gen(env, gen_options);

        auto gen_result = llvm_gen.generate(module);
        if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
            std::ostringstream err;
            const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
            for (const auto& error : errors) {
                err << job->source_file.string() << ":" << error.span.start.line << ":"
                    << error.span.start.column << ": codegen error: " << error.message << "\n";
            }
            job->error_message = err.str();
            if (verbose) {
                std::cerr << job->error_message;
            }
            return false;
        }

        const auto& llvm_ir = std::get<std::string>(gen_result);

        // Write LLVM IR to temporary file with unique thread ID suffix to avoid race conditions
        std::ostringstream tid_suffix;
        tid_suffix << "." << std::this_thread::get_id() << ".ll";
        fs::path unique_ll_file = job->output_file;
        unique_ll_file.replace_extension(tid_suffix.str());

        {
            std::ofstream out(unique_ll_file);
            if (!out) {
                job->error_message = "Cannot write to " + unique_ll_file.string();
                return false;
            }
            out << llvm_ir;
        }

        // Compile to object file
        std::string clang = find_clang();
        if (clang.empty()) {
            job->error_message = "clang not found";
            fs::remove(unique_ll_file);
            return false;
        }

        ObjectCompileOptions obj_options;
        obj_options.optimization_level = options.optimization_level;
        obj_options.debug_info = options.debug_info;
        obj_options.verbose = verbose;

        auto obj_result =
            compile_ll_to_object(unique_ll_file, job->output_file, clang, obj_options);

        // Clean up unique .ll file
        fs::remove(unique_ll_file);

        if (!obj_result.success) {
            job->error_message = obj_result.error_message;
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        job->error_message = std::string("Exception: ") + e.what();
        if (verbose) {
            std::cerr << job->error_message << "\n";
        }
        return false;
    }
}

std::vector<fs::path> ParallelBuilder::get_object_files() const {
    std::vector<fs::path> result;
    for (const auto& job : jobs) {
        if (job->completed && !job->failed) {
            result.push_back(job->output_file);
        }
    }
    return result;
}

// ============================================================================
// File Discovery
// ============================================================================

/// Recursively discovers all .tml source files in a directory.
///
/// Excludes files in `build/` directories to avoid recompiling artifacts.
std::vector<fs::path> discover_source_files(const fs::path& root_dir) {
    std::vector<fs::path> files;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                std::string path_str = path.string();

                // Skip build artifacts only (include test files for benchmarking)
                if (path_str.find("\\build\\") != std::string::npos ||
                    path_str.find("/build/") != std::string::npos) {
                    continue;
                }

                // Include all .tml files
                if (path.extension() == ".tml") {
                    files.push_back(path);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error discovering source files: " << e.what() << "\n";
    }

    std::sort(files.begin(), files.end());
    return files;
}

// ============================================================================
// Entry Point
// ============================================================================

/// Entry point for `tml build-all` command.
///
/// ## Arguments
///
/// | Argument      | Description                              |
/// |---------------|------------------------------------------|
/// | `-jN`         | Use N threads for compilation            |
/// | `--clean`     | Clean cache before building              |
/// | `--no-cache`  | Disable incremental caching              |
/// | `--lto`       | Enable Link-Time Optimization            |
/// | `-O0...-O3`   | Set optimization level                   |
int run_parallel_build(const std::vector<std::string>& args, bool verbose) {
    // Parse arguments
    int num_threads = 0;
    bool clean = false;
    bool no_cache = false;
    bool lto = false;
    bool use_hir = false;
    int opt_level = tml::CompilerOptions::optimization_level;

    for (const auto& arg : args) {
        if (arg.starts_with("-j")) {
            if (arg.length() > 2) {
                num_threads = std::stoi(arg.substr(2));
            }
        } else if (arg == "--clean") {
            clean = true;
        } else if (arg == "--no-cache") {
            no_cache = true;
        } else if (arg == "--lto") {
            lto = true;
        } else if (arg == "--use-hir") {
            use_hir = true;
        } else if (arg == "-O0") {
            opt_level = 0;
        } else if (arg == "-O1") {
            opt_level = 1;
        } else if (arg == "-O2") {
            opt_level = 2;
        } else if (arg == "-O3") {
            opt_level = 3;
        }
    }

    // Discover source files
    fs::path cwd = fs::current_path();
    auto source_files = discover_source_files(cwd);

    if (source_files.empty()) {
        std::cout << "No source files found to build\n";
        return 0;
    }

    if (verbose) {
        std::cout << "Found " << source_files.size() << " source files\n";
    }

    // Clean build directory if requested
    if (clean) {
        fs::path build_dir = cwd / "build" / "debug" / ".cache";
        if (fs::exists(build_dir)) {
            fs::remove_all(build_dir);
            if (verbose) {
                std::cout << "Cleaned build cache\n";
            }
        }
    }

    // Create parallel builder
    ParallelBuilder builder(num_threads);

    // Set options
    ParallelBuildOptions opts;
    opts.verbose = verbose;
    opts.no_cache = no_cache;
    opts.lto = lto;
    opts.use_hir = use_hir;
    opts.optimization_level = opt_level;
    opts.debug_info = tml::CompilerOptions::debug_info;
    builder.set_options(opts);

    // Create output directory
    fs::path build_dir = cwd / "build" / "debug" / ".cache";
    fs::create_directories(build_dir);

    // Add all files to builder
    for (const auto& src : source_files) {
        fs::path output = build_dir / src.stem();
        output += get_object_extension();
        builder.add_file(src, output);
    }

    // Execute parallel build
    bool success = builder.build(verbose);

    return success ? 0 : 1;
}

} // namespace tml::cli
