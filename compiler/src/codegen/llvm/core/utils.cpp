TML_MODULE("codegen_x86")

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
    // Auto-detect runtime function references for dead declaration elimination.
    // Scans for @symbol patterns and marks them as needed in the catalog.
    if (!runtime_catalog_index_.empty() && needed_runtime_decls_.size() < runtime_catalog_.size()) {
        size_t pos = code.find('@');
        while (pos != std::string::npos) {
            pos++; // skip '@'
            size_t end = pos;
            while (end < code.size() && (std::isalnum(static_cast<unsigned char>(code[end])) ||
                                         code[end] == '_' || code[end] == '.'))
                end++;
            if (end > pos) {
                auto it = runtime_catalog_index_.find(code.substr(pos, end - pos));
                if (it != runtime_catalog_index_.end())
                    require_runtime_decl(runtime_catalog_[it->second].name);
            }
            pos = code.find('@', end);
        }
    }
    output_ << code << "\n";
}

// ============ Entry-Block Alloca Hoisting ============

auto LLVMIRGen::emit_hoisted_alloca(const std::string& type, const std::string& align)
    -> std::string {
    std::string reg = fresh_reg();
    std::string line = "  " + reg + " = alloca " + type;
    if (!align.empty())
        line += ", align " + align;
    if (alloca_hoisting_active_) {
        entry_allocas_.push_back(line);
    } else {
        // Not inside a function body (e.g., during module-level codegen),
        // emit directly as before
        emit_line(line);
    }
    return reg;
}

void LLVMIRGen::begin_alloca_hoisting() {
    entry_allocas_.clear();
    // Emit a unique marker that we'll replace with hoisted allocas at function end
    alloca_hoisting_marker_ = "; @HOISTED_ALLOCAS_" + std::to_string(temp_counter_) + "@";
    emit_line(alloca_hoisting_marker_);
    alloca_hoisting_active_ = true;
}

void LLVMIRGen::end_alloca_hoisting() {
    if (!alloca_hoisting_active_)
        return;
    alloca_hoisting_active_ = false;

    // Build the alloca block to replace marker with
    std::string alloca_block;
    for (const auto& line : entry_allocas_) {
        alloca_block += line + "\n";
    }
    entry_allocas_.clear();

    // Replace the marker in output_ with the hoisted allocas.
    // Use rfind (reverse search) — the marker is near the end of the stream
    // since it was emitted at the start of the CURRENT function.
    // This avoids O(n) scanning from the beginning of multi-megabyte output.
    std::string full = output_.str();
    auto pos = full.rfind(alloca_hoisting_marker_);
    if (pos != std::string::npos) {
        // Replace marker line (marker + newline) with alloca block
        full.replace(pos, alloca_hoisting_marker_.size() + 1, alloca_block);
        output_.str(full);
        output_.seekp(0, std::ios_base::end);
    }
    alloca_hoisting_marker_.clear();
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
    auto it = string_literal_dedup_.find(value);
    if (it != string_literal_dedup_.end()) {
        return it->second;
    }
    std::string name = "@.str." + std::to_string(string_literals_.size());
    string_literals_.emplace_back(name, value);
    string_literal_dedup_.emplace(value, name);
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
