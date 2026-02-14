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

#include "codegen/llvm/llvm_ir_gen.hpp"

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

void LLVMIRGen::emit_coverage(const std::string& func_name) {
    if (options_.coverage_enabled) {
        std::string func_name_str = add_string_literal(func_name);
        emit_line("  call void @tml_cover_func(ptr " + func_name_str + ")");
    }
}

void LLVMIRGen::emit_coverage_report_calls(const std::string& coverage_output_str,
                                           bool check_quiet) {
    if (!options_.coverage_enabled) {
        return;
    }
    if (check_quiet && options_.coverage_quiet) {
        return;
    }
    emit_line("  call void @print_coverage_report()");
    if (!coverage_output_str.empty()) {
        emit_line("  call void @write_coverage_html(ptr " + coverage_output_str + ")");
    }
}

void LLVMIRGen::report_error(const std::string& msg, const SourceSpan& span) {
    errors_.push_back(LLVMGenError{msg, span, {}, ""});
}

void LLVMIRGen::report_error(const std::string& msg, const SourceSpan& span,
                             const std::string& code) {
    errors_.push_back(LLVMGenError{msg, span, {}, code});
}

auto LLVMIRGen::coerce_closure_to_fn_ptr(const std::string& val) -> std::string {
    if (last_expr_type_ == "{ ptr, ptr }") {
        // Extract fn_ptr (index 0) from the fat pointer
        std::string fn_ptr = fresh_reg();
        emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + val + ", 0");
        last_expr_type_ = "ptr";
        return fn_ptr;
    }
    return val;
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

auto LLVMIRGen::is_library_method(const std::string& type_name, const std::string& method) const
    -> bool {
    if (!env_.module_registry()) {
        return false;
    }

    // First check if type_name::method is directly registered (top-level functions)
    std::string qualified_name = type_name + "::" + method;
    const auto& all_modules = env_.module_registry()->get_all_modules();
    for (const auto& [mod_name, mod] : all_modules) {
        if (mod.functions.find(qualified_name) != mod.functions.end()) {
            return true;
        }
        // Also check if the TYPE itself is from this module (impl methods)
        if (mod.structs.find(type_name) != mod.structs.end()) {
            return true;
        }
        // Check enums
        if (mod.enums.find(type_name) != mod.enums.end()) {
            return true;
        }
        // Check classes
        if (mod.classes.find(type_name) != mod.classes.end()) {
            return true;
        }
    }
    return false;
}

} // namespace tml::codegen
