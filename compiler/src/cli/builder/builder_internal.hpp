//! # Builder Internal Interface
//!
//! This header defines internal utilities for the build command.
//!
//! ## Build Pipeline Helpers
//!
//! The `build` namespace contains helper functions for:
//! - Cache directory management
//! - Content hashing for caching
//! - Runtime object collection
//! - File copy utilities
//!
//! ## Compilation Flow
//!
//! ```text
//! source → lexer → parser → types → borrow → codegen → object → link
//! ```

#pragma once

// Internal header for build command implementation
// Contains shared utilities, helpers, and type conversions

// Headers from src/cli/ (need cli/ prefix from src/ include path)
#include "cli/builder/build_config.hpp"
#include "cli/builder/compiler_setup.hpp"
#include "cli/builder/object_compiler.hpp"
#include "cli/builder/rlib.hpp"
#include "cli/commands/cmd_build.hpp"
#include "cli/diagnostic.hpp"
#include "cli/utils.hpp"

// Headers from include/ (no prefix needed, include/ is in path)
#include "borrow/checker.hpp"
#include "codegen/c_header_gen.hpp"
#include "codegen/llvm_ir_gen.hpp"
#include "codegen/mir_codegen.hpp"
#include "common.hpp"
#include "hir/hir.hpp"
#include "hir/hir_builder.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/hir_mir_builder.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>
#ifndef _WIN32
#include "types/module.hpp"

#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace tml::cli::build {

// ============================================================================
// Type Utilities
// ============================================================================

// Convert a parser::Type to a string representation
std::string type_to_string(const parser::Type& type);

// ============================================================================
// Cache Key Generation
// ============================================================================

// Generate a unique cache key for a file path (to avoid collisions in parallel tests)
std::string generate_cache_key(const std::string& path);

// Generate a content hash for caching compiled object files
std::string generate_content_hash(const std::string& content);

// Generate a combined hash for executable caching (source + all object files)
std::string generate_exe_hash(const std::string& source_hash,
                              const std::vector<fs::path>& obj_files);

// ============================================================================
// File Utilities
// ============================================================================

// Fast file copy using hard links when possible, falls back to regular copy
bool fast_copy_file(const fs::path& from, const fs::path& to);

// Find the project root by looking for markers like .git, CLAUDE.md, etc.
fs::path find_project_root();

// Find or create build directory for a TML project
fs::path get_build_dir(bool release = false);

// Get the global deps cache directory
fs::path get_deps_cache_dir();

// Get the global run cache directory (for tml run temporary files)
fs::path get_run_cache_dir();

// ============================================================================
// Diagnostic Helpers
// ============================================================================

// Emit a lexer error using the diagnostic emitter
void emit_lexer_error(DiagnosticEmitter& emitter, const lexer::LexerError& error);

// Emit a parser error using the diagnostic emitter
void emit_parser_error(DiagnosticEmitter& emitter, const parser::ParseError& error);

// Emit a type error using the diagnostic emitter
void emit_type_error(DiagnosticEmitter& emitter, const types::TypeError& error);

// Emit a codegen error using the diagnostic emitter
void emit_codegen_error(DiagnosticEmitter& emitter, const codegen::LLVMGenError& error);

// Emit all lexer errors
void emit_all_lexer_errors(DiagnosticEmitter& emitter, const lexer::Lexer& lex);

// Emit all parser errors
void emit_all_parser_errors(DiagnosticEmitter& emitter,
                            const std::vector<parser::ParseError>& errors);

// Emit all type errors
void emit_all_type_errors(DiagnosticEmitter& emitter, const std::vector<types::TypeError>& errors);

// Emit all codegen errors
void emit_all_codegen_errors(DiagnosticEmitter& emitter,
                             const std::vector<codegen::LLVMGenError>& errors);

// Emit a borrow error using the diagnostic emitter
void emit_borrow_error(DiagnosticEmitter& emitter, const borrow::BorrowError& error);

// Emit all borrow errors
void emit_all_borrow_errors(DiagnosticEmitter& emitter,
                            const std::vector<borrow::BorrowError>& errors);

// ============================================================================
// Module Helpers
// ============================================================================

// Helper to check if any function has @bench decorator
bool has_bench_functions(const parser::Module& module);

// Helper to get runtime object files as a vector
std::vector<fs::path> get_runtime_objects(const std::shared_ptr<types::ModuleRegistry>& registry,
                                          const parser::Module& module,
                                          const std::string& deps_cache, const std::string& clang,
                                          bool verbose);

} // namespace tml::cli::build
