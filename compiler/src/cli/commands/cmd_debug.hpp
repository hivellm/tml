//! # Debug Commands Interface
//!
//! This header defines debug/inspection commands for compiler stages.
//!
//! ## Commands
//!
//! | Function       | Command       | Output                       |
//! |----------------|---------------|------------------------------|
//! | `run_lex()`    | `tml lex`     | Token stream                 |
//! | `run_parse()`  | `tml parse`   | AST structure                |
//! | `run_check()`  | `tml check`   | Type checking without codegen|

#pragma once
#include <string>

namespace tml::cli {

// Debug commands
int run_lex(const std::string& path, bool verbose);
int run_parse(const std::string& path, bool verbose);
int run_check(const std::string& path, bool verbose);

} // namespace tml::cli
