//! # LLVM IR Generator - Core Utilities
//!
//! This file implements fundamental codegen utilities.
//!
//! ## Register Allocation
//!
//! | Method        | Returns         | Example        |
//! |---------------|-----------------|----------------|
//! | `fresh_reg`   | Unique register | `%t0`, `%t1`   |
//! | `fresh_label` | Unique label    | `if.then0`     |
//!
//! ## Output Emission
//!
//! | Method      | Description                    |
//! |-------------|--------------------------------|
//! | `emit`      | Emit raw text (no newline)     |
//! | `emit_line` | Emit text with newline         |
//!
//! ## String Literals
//!
//! `add_string_literal()` registers a string constant and returns its
//! global variable name (`@.str.0`, `@.str.1`, etc.). These are emitted
//! in the module preamble.
//!
//! ## Error Reporting
//!
//! `report_error()` collects codegen errors for later reporting.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

LLVMIRGen::LLVMIRGen(const types::TypeEnv& env, LLVMGenOptions options)
    : env_(env), options_(std::move(options)) {}

auto LLVMIRGen::fresh_reg() -> std::string {
    return "%t" + std::to_string(temp_counter_++);
}

auto LLVMIRGen::fresh_label(const std::string& prefix) -> std::string {
    return prefix + std::to_string(label_counter_++);
}

void LLVMIRGen::emit(const std::string& code) {
    output_ << code;
}

void LLVMIRGen::emit_line(const std::string& code) {
    output_ << code << "\n";
}

void LLVMIRGen::report_error(const std::string& msg, const SourceSpan& span) {
    errors_.push_back(LLVMGenError{msg, span, {}});
}

auto LLVMIRGen::add_string_literal(const std::string& value) -> std::string {
    std::string name = "@.str." + std::to_string(string_literals_.size());
    string_literals_.emplace_back(name, value);
    return name;
}

auto LLVMIRGen::get_suite_prefix() const -> std::string {
    // Suite prefix is only used for test-local functions (current_module_prefix_ empty)
    // Library functions should NOT have suite prefix - they're shared across tests
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        return "s" + std::to_string(options_.suite_test_index) + "_";
    }
    return "";
}

} // namespace tml::codegen
