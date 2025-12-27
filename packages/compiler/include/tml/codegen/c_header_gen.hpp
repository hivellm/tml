#pragma once

#include <string>
#include <tml/parser/ast.hpp>
#include <tml/types/env.hpp>
#include <vector>

namespace tml::codegen {

// C header generation options
struct CHeaderGenOptions {
    bool add_extern_c = true;       // Add extern "C" wrapper for C++ compatibility
    bool add_include_guards = true; // Add #ifndef header guards
    std::string guard_prefix = "";  // Prefix for include guards (empty = auto from module name)
};

// Result of C header generation
struct CHeaderGenResult {
    bool success = false;
    std::string header_content;
    std::string error_message;
};

// C Header Generator
// Generates C header files (.h) from TML modules for FFI
class CHeaderGen {
public:
    explicit CHeaderGen(const types::TypeEnv& env, CHeaderGenOptions options = {});

    // Generate C header from a module
    // Only exports public functions
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
