TML_MODULE("codegen_x86")

//! # LLVM IR Generator - @derive(FromStr) Implementation
//!
//! This file implements the `@derive(FromStr)` derive macro.
//! FromStr generates: `func from_str(s: Str) -> Outcome[Self, Str]` (static)
//!
//! Parses a string into the type.
//! For enums: matches variant names (case-sensitive)
//! For structs: not supported (returns Err)

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(FromStr) decorator
static bool has_derive_fromstr(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "FromStr") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(FromStr) decorator
static bool has_derive_fromstr(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "FromStr") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// ============================================================================
// FromStr Generation for Structs
// ============================================================================

/// Generate from_str() method for a struct with @derive(FromStr)
/// Note: FromStr for structs is not well-defined, so we just return an error
void LLVMIRGen::gen_derive_fromstr_struct(const parser::StructDecl& s) {
    if (!has_derive_fromstr(s)) {
        return;
    }

    // Skip generic structs - they need to be instantiated first
    if (!s.generics.empty()) {
        return;
    }

    std::string type_name = s.name;
    std::string llvm_type = "%struct." + type_name;

    // Add suite prefix for test-local types
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    std::string func_name = "@tml_" + suite_prefix + type_name + "_from_str";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Ensure Outcome[TypeName, Str] type is defined
    auto self_type = std::make_shared<types::Type>();
    self_type->kind = types::NamedType{type_name, "", {}};
    auto str_type = types::make_str();
    std::vector<types::TypePtr> outcome_type_args = {self_type, str_type};
    std::string outcome_mangled = require_enum_instantiation("Outcome", outcome_type_args);
    std::string outcome_type = "%struct." + outcome_mangled;

    // Error message constant
    std::string err_const = "@.fromstr_" + suite_prefix + type_name + "_err";

    type_defs_buffer_ << "; @derive(FromStr) string constants for " << type_name << "\n";
    type_defs_buffer_
        << err_const
        << " = private constant [32 x i8] c\"FromStr not supported for struct\\00\"\n\n";

    // Emit function definition - just returns error
    type_defs_buffer_ << "; @derive(FromStr) for " << type_name << "\n";
    type_defs_buffer_ << "define internal " << outcome_type << " " << func_name << "(ptr %s) {\n";
    type_defs_buffer_ << "entry:\n";

    // Return Err with error message
    type_defs_buffer_ << "  %result = alloca " << outcome_type << "\n";
    type_defs_buffer_ << "  %tag_ptr = getelementptr " << outcome_type
                      << ", ptr %result, i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 1, ptr %tag_ptr ; Err tag\n";
    type_defs_buffer_ << "  %payload = getelementptr " << outcome_type
                      << ", ptr %result, i32 0, i32 1\n";
    type_defs_buffer_ << "  %err_str = getelementptr [32 x i8], ptr " << err_const
                      << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store ptr %err_str, ptr %payload\n";
    type_defs_buffer_ << "  %ret = load " << outcome_type << ", ptr %result\n";
    type_defs_buffer_ << "  ret " << outcome_type << " %ret\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// FromStr Generation for Enums
// ============================================================================

/// Generate from_str() method for an enum with @derive(FromStr)
void LLVMIRGen::gen_derive_fromstr_enum(const parser::EnumDecl& e) {
    if (!has_derive_fromstr(e)) {
        return;
    }

    // Skip generic enums
    if (!e.generics.empty()) {
        return;
    }

    std::string type_name = e.name;
    std::string llvm_type = "%struct." + type_name;

    // Add suite prefix for test-local types
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    std::string func_name = "@tml_" + suite_prefix + type_name + "_from_str";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Ensure Outcome[TypeName, Str] type is defined
    auto self_type = std::make_shared<types::Type>();
    self_type->kind = types::NamedType{type_name, "", {}};
    auto str_type = types::make_str();
    std::vector<types::TypePtr> outcome_type_args = {self_type, str_type};
    std::string outcome_mangled = require_enum_instantiation("Outcome", outcome_type_args);
    std::string outcome_type = "%struct." + outcome_mangled;

    // String constants for error message and variant names
    std::string err_const = "@.fromstr_" + suite_prefix + type_name + "_err";

    type_defs_buffer_ << "; @derive(FromStr) string constants for " << type_name << "\n";
    type_defs_buffer_ << err_const << " = private constant [16 x i8] c\"Unknown variant\\00\"\n";

    // Variant name constants
    for (const auto& variant : e.variants) {
        std::string var_const = "@.fromstr_" + suite_prefix + type_name + "_v_" + variant.name;
        type_defs_buffer_ << var_const << " = private constant [" << (variant.name.size() + 1)
                          << " x i8] c\"" << variant.name << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // Emit function definition
    type_defs_buffer_ << "; @derive(FromStr) for " << type_name << "\n";
    type_defs_buffer_ << "define internal " << outcome_type << " " << func_name << "(ptr %s) {\n";
    type_defs_buffer_ << "entry:\n";

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Compare with each variant name
    int tag = 0;
    for (const auto& variant : e.variants) {
        std::string var_const = "@.fromstr_" + suite_prefix + type_name + "_v_" + variant.name;
        std::string var_ptr = fresh_temp();
        type_defs_buffer_ << "  " << var_ptr << " = getelementptr [" << (variant.name.size() + 1)
                          << " x i8], ptr " << var_const << ", i32 0, i32 0\n";
        std::string cmp = fresh_temp();
        type_defs_buffer_ << "  " << cmp << " = call i32 @strcmp(ptr %s, ptr " << var_ptr << ")\n";
        std::string is_match = fresh_temp();
        type_defs_buffer_ << "  " << is_match << " = icmp eq i32 " << cmp << ", 0\n";
        type_defs_buffer_ << "  br i1 " << is_match << ", label %match_" << tag << ", label %check_"
                          << (tag + 1) << "\n\n";

        type_defs_buffer_ << "match_" << tag << ":\n";
        // Return Ok with this variant
        std::string ok_result = fresh_temp();
        type_defs_buffer_ << "  " << ok_result << " = alloca " << outcome_type << "\n";
        std::string ok_tag_ptr = fresh_temp();
        type_defs_buffer_ << "  " << ok_tag_ptr << " = getelementptr " << outcome_type << ", ptr "
                          << ok_result << ", i32 0, i32 0\n";
        type_defs_buffer_ << "  store i32 0, ptr " << ok_tag_ptr << " ; Ok tag\n";
        std::string ok_payload = fresh_temp();
        type_defs_buffer_ << "  " << ok_payload << " = getelementptr " << outcome_type << ", ptr "
                          << ok_result << ", i32 0, i32 1\n";
        // Set enum tag
        type_defs_buffer_ << "  store i32 " << tag << ", ptr " << ok_payload << "\n";
        std::string ok_ret = fresh_temp();
        type_defs_buffer_ << "  " << ok_ret << " = load " << outcome_type << ", ptr " << ok_result
                          << "\n";
        type_defs_buffer_ << "  ret " << outcome_type << " " << ok_ret << "\n\n";

        type_defs_buffer_ << "check_" << (tag + 1) << ":\n";
        tag++;
    }

    // Unknown variant error
    std::string unk_result = fresh_temp();
    type_defs_buffer_ << "  " << unk_result << " = alloca " << outcome_type << "\n";
    std::string unk_tag = fresh_temp();
    type_defs_buffer_ << "  " << unk_tag << " = getelementptr " << outcome_type << ", ptr "
                      << unk_result << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 1, ptr " << unk_tag << " ; Err tag\n";
    std::string unk_payload = fresh_temp();
    type_defs_buffer_ << "  " << unk_payload << " = getelementptr " << outcome_type << ", ptr "
                      << unk_result << ", i32 0, i32 1\n";
    std::string unk_msg = fresh_temp();
    type_defs_buffer_ << "  " << unk_msg << " = getelementptr [16 x i8], ptr " << err_const
                      << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store ptr " << unk_msg << ", ptr " << unk_payload << "\n";
    std::string unk_ret = fresh_temp();
    type_defs_buffer_ << "  " << unk_ret << " = load " << outcome_type << ", ptr " << unk_result
                      << "\n";
    type_defs_buffer_ << "  ret " << outcome_type << " " << unk_ret << "\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
