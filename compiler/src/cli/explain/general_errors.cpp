//! # General Error Explanations
//!
//! Error codes E001-E006 for general compiler errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_general_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"E001", R"EX(
File not found [E001]

The specified source file does not exist or cannot be read.

Example:

    tml build nonexistent.tml   // file does not exist

How to fix:

1. Check the file path for typos
2. Verify the file exists: the path is relative to the current directory
3. Check file permissions
)EX"},

        {"E002", R"EX(
I/O error [E002]

An error occurred while reading or writing a file. This can happen due
to permission issues, disk full, or corrupted files.

How to fix:

1. Check file permissions
2. Verify disk space is available
3. Check that the output directory exists
)EX"},

        {"E003", R"EX(
Internal compiler error [E003]

An unexpected internal error occurred in the compiler. This is a bug
in the TML compiler.

Please report this issue with:
1. The TML source file that triggered the error
2. The compiler version (`tml --version`)
3. The full error output
)EX"},

        {"E004", R"EX(
Command error [E004]

An invalid CLI command or arguments were provided. The command does not
exist or was invoked with incorrect options.

Example:

    tml bild main.tml          // 'bild' is not a valid command

How to fix:

    tml build main.tml         // correct command name

Available commands:

    tml build    Build a TML source file
    tml run      Build and run a TML source file
    tml test     Run tests
    tml check    Type check without compiling
    tml fmt      Format source files
    tml lint     Lint source files
    tml doc      Generate documentation
    tml explain  Explain an error code
    tml init     Initialize a new project

Use `tml --help` to see all available commands and options.
)EX"},

        {"E005", R"EX(
Configuration error [E005]

An error was found in the project configuration file (tml.toml) or
build configuration.

Common causes:

1. Malformed TOML syntax in tml.toml
2. Invalid configuration key or value
3. Incompatible configuration options

How to fix:

1. Check tml.toml for syntax errors
2. Verify configuration keys are spelled correctly
3. Refer to the documentation for valid configuration options

Example tml.toml:

    [project]
    name = "my_project"
    version = "1.0.0"

    [build]
    optimize = "O2"
)EX"},

        {"E006", R"EX(
Dependency error [E006]

An error occurred during dependency resolution. A required module,
library, or package could not be found or has conflicting versions.

Common causes:

1. A `use` statement references a module that is not installed
2. Circular dependencies between modules
3. Version conflict between dependencies
4. Missing standard library modules

How to fix:

1. Verify the module path is correct in `use` statements
2. Check that required packages are installed
3. Resolve version conflicts by updating dependencies
4. Ensure the standard library is properly installed

    use std::collections::HashMap    // requires std library
    use my_lib::utils                // requires my_lib package
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
