//! # C Header Generator
//!
//! This module generates C header files (`.h`) from TML modules for FFI.
//! The generated headers allow C/C++ code to call TML functions.
//!
//! ## Features
//!
//! - Automatic `extern "C"` wrapper for C++ compatibility
//! - Include guard generation
//! - Type mapping from TML to C types
//!
//! ## Usage
//!
//! ```bash
//! tml build module.tml --emit-header
//! ```

#pragma once

#include <parser/ast.hpp>
#include <string>
#include <types/env.hpp>
#include <vector>

namespace tml::codegen {

/// Options for C header generation.
struct CHeaderGenOptions {
    bool add_extern_c = true;       ///< Add `extern "C"` wrapper for C++.
    bool add_include_guards = true; ///< Add `#ifndef` header guards.
    std::string guard_prefix = "";  ///< Guard prefix (empty = auto from module name).
};

/// Result of C header generation.
struct CHeaderGenResult {
    bool success = false;       ///< True if generation succeeded.
    std::string header_content; ///< Generated header content.
    std::string error_message;  ///< Error message if failed.
};

/// C header file generator for FFI.
///
/// Generates C header files from TML modules, enabling C/C++ code
/// to call TML functions. Only public functions are exported.
class CHeaderGen {
public:
    /// Creates a header generator with the given type environment.
    explicit CHeaderGen(const types::TypeEnv& env, CHeaderGenOptions options = {});

    /// Generates a C header from a module (public functions only).
    CHeaderGenResult generate(const parser::Module& module);

private:
    [[maybe_unused]] const types::TypeEnv& env_;
    CHeaderGenOptions options_;

    // Map TML type to C type
    std::string map_type_to_c(const parser::TypePtr& type);

    // Generate function declaration
    std::string gen_func_decl(const parser::FuncDecl& func);

    // Generate include guard name from module name
    std::string gen_guard_name(const std::string& module_name);
};

} // namespace tml::codegen
