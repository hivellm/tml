TML_MODULE("codegen_x86")

//! # LLVM IR Generator - @derive(Debug) Implementation
//!
//! This file implements the `@derive(Debug)` derive macro.
//! Debug generates: `func debug_string(this) -> Str`
//!
//! ## Generated Code Pattern
//!
//! For a struct like:
//! ```tml
//! @derive(Debug)
//! type Point {
//!     x: I32,
//!     y: I32
//! }
//! ```
//!
//! We generate a function that returns "Point { x: <value>, y: <value> }"

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Debug) decorator
static bool has_derive_debug(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Debug") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Debug) decorator
static bool has_derive_debug(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Debug") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Get the appropriate to_string function for a primitive type
/// Phase 45: All types use TML Display behavior impls
static std::string get_to_string_func(const std::string& llvm_type) {
    if (llvm_type == "i1") {
        return "tml_Bool_to_string";
    } else if (llvm_type == "i8") {
        return "tml_I8_to_string";
    } else if (llvm_type == "i16") {
        return "tml_I16_to_string";
    } else if (llvm_type == "i32") {
        return "tml_I32_to_string";
    } else if (llvm_type == "i64" || llvm_type == "i128") {
        return "tml_I64_to_string";
    } else if (llvm_type == "float") {
        return "tml_F32_to_string";
    } else if (llvm_type == "double") {
        return "tml_F64_to_string";
    }
    return "";
}

/// Check if a type is a primitive that can be converted to string
static bool is_primitive_stringable(const std::string& llvm_type) {
    return llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" ||
           llvm_type == "i64" || llvm_type == "i128" || llvm_type == "float" ||
           llvm_type == "double" || llvm_type == "ptr";
}

// ============================================================================
// Debug Generation for Structs
// ============================================================================

/// Generate debug_string() method for a struct with @derive(Debug)
void LLVMIRGen::gen_derive_debug_struct(const parser::StructDecl& s) {
    if (!has_derive_debug(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_debug_string";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);
    // Register as allocating function for Str temp tracking
    allocating_functions_.insert("debug_string");

    // Get field info for this struct
    auto fields_it = struct_fields_.find(type_name);
    if (fields_it == struct_fields_.end()) {
        return; // No field info, can't generate
    }
    const auto& fields = fields_it->second;

    // Create string constant for the type name and braces
    std::string type_name_const = "@.debug_" + suite_prefix + type_name + "_name";
    std::string open_brace_const = "@.debug_" + suite_prefix + type_name + "_open";
    std::string close_brace_const = "@.debug_" + suite_prefix + type_name + "_close";
    std::string separator_const = "@.debug_" + suite_prefix + type_name + "_sep";
    std::string colon_const = "@.debug_" + suite_prefix + type_name + "_colon";

    // Emit string constants
    type_defs_buffer_ << "; @derive(Debug) string constants for " << type_name << "\n";
    type_defs_buffer_ << type_name_const << " = private constant [" << (type_name.size() + 1)
                      << " x i8] c\"" << type_name << "\\00\"\n";
    type_defs_buffer_ << open_brace_const << " = private constant [4 x i8] c\" { \\00\"\n";
    type_defs_buffer_ << close_brace_const << " = private constant [3 x i8] c\" }\\00\"\n";
    type_defs_buffer_ << separator_const << " = private constant [3 x i8] c\", \\00\"\n";
    type_defs_buffer_ << colon_const << " = private constant [3 x i8] c\": \\00\"\n";

    // Emit field name constants
    for (const auto& field : fields) {
        std::string field_const = "@.debug_" + suite_prefix + type_name + "_f_" + field.name;
        type_defs_buffer_ << field_const << " = private constant [" << (field.name.size() + 1)
                          << " x i8] c\"" << field.name << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // Emit function definition
    type_defs_buffer_ << "; @derive(Debug) for " << type_name << "\n";
    type_defs_buffer_ << "define internal ptr " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Start with type name
    std::string current = fresh_temp();
    type_defs_buffer_ << "  " << current << " = getelementptr [" << (type_name.size() + 1)
                      << " x i8], ptr " << type_name_const << ", i32 0, i32 0\n";

    // Add opening brace
    std::string open = fresh_temp();
    type_defs_buffer_ << "  " << open << " = getelementptr [4 x i8], ptr " << open_brace_const
                      << ", i32 0, i32 0\n";
    std::string with_open = fresh_temp();
    type_defs_buffer_ << "  " << with_open << " = call ptr @str_concat_opt(ptr " << current
                      << ", ptr " << open << ")\n";
    current = with_open;

    // Add each field
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];

        // Add field name
        std::string field_const = "@.debug_" + suite_prefix + type_name + "_f_" + field.name;
        std::string field_name = fresh_temp();
        type_defs_buffer_ << "  " << field_name << " = getelementptr [" << (field.name.size() + 1)
                          << " x i8], ptr " << field_const << ", i32 0, i32 0\n";

        std::string with_name = fresh_temp();
        type_defs_buffer_ << "  " << with_name << " = call ptr @str_concat_opt(ptr " << current
                          << ", ptr " << field_name << ")\n";

        // Add colon
        std::string colon = fresh_temp();
        type_defs_buffer_ << "  " << colon << " = getelementptr [3 x i8], ptr " << colon_const
                          << ", i32 0, i32 0\n";
        std::string with_colon = fresh_temp();
        type_defs_buffer_ << "  " << with_colon << " = call ptr @str_concat_opt(ptr " << with_name
                          << ", ptr " << colon << ")\n";

        // Get field value and convert to string
        std::string field_ptr = fresh_temp();
        type_defs_buffer_ << "  " << field_ptr << " = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";

        std::string value_str;
        if (field.llvm_type == "ptr") {
            // String type - use directly
            value_str = fresh_temp();
            type_defs_buffer_ << "  " << value_str << " = load ptr, ptr " << field_ptr << "\n";
        } else if (is_primitive_stringable(field.llvm_type)) {
            // Primitive type - convert to string
            std::string val = fresh_temp();
            type_defs_buffer_ << "  " << val << " = load " << field.llvm_type << ", ptr "
                              << field_ptr << "\n";

            std::string to_string_func = get_to_string_func(field.llvm_type);
            value_str = fresh_temp();

            // Phase 45: All primitives use TML Display, taking native types directly
            if (field.llvm_type == "i128") {
                std::string trunc = fresh_temp();
                type_defs_buffer_ << "  " << trunc << " = trunc i128 " << val << " to i64\n";
                type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func
                                  << "(i64 " << trunc << ")\n";
            } else {
                type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func << "("
                                  << field.llvm_type << " " << val << ")\n";
            }
        } else {
            // Non-primitive type - call debug_string() on the field
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_debug_func =
                "@tml_" + suite_prefix + field_type_name + "_debug_string";
            value_str = fresh_temp();
            type_defs_buffer_ << "  " << value_str << " = call ptr " << field_debug_func << "(ptr "
                              << field_ptr << ")\n";
        }

        std::string with_value = fresh_temp();
        type_defs_buffer_ << "  " << with_value << " = call ptr @str_concat_opt(ptr " << with_colon
                          << ", ptr " << value_str << ")\n";
        current = with_value;

        // Add separator if not last field
        if (i < fields.size() - 1) {
            std::string sep = fresh_temp();
            type_defs_buffer_ << "  " << sep << " = getelementptr [3 x i8], ptr " << separator_const
                              << ", i32 0, i32 0\n";
            std::string with_sep = fresh_temp();
            type_defs_buffer_ << "  " << with_sep << " = call ptr @str_concat_opt(ptr " << current
                              << ", ptr " << sep << ")\n";
            current = with_sep;
        }
    }

    // Add closing brace
    std::string close = fresh_temp();
    type_defs_buffer_ << "  " << close << " = getelementptr [3 x i8], ptr " << close_brace_const
                      << ", i32 0, i32 0\n";
    std::string result = fresh_temp();
    type_defs_buffer_ << "  " << result << " = call ptr @str_concat_opt(ptr " << current << ", ptr "
                      << close << ")\n";

    type_defs_buffer_ << "  ret ptr " << result << "\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Debug Generation for Enums
// ============================================================================

/// Generate debug_string() method for an enum with @derive(Debug)
void LLVMIRGen::gen_derive_debug_enum(const parser::EnumDecl& e) {
    if (!has_derive_debug(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_debug_string";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Emit variant name constants
    type_defs_buffer_ << "; @derive(Debug) string constants for " << type_name << "\n";
    for (const auto& variant : e.variants) {
        std::string variant_const = "@.debug_" + suite_prefix + type_name + "_v_" + variant.name;
        std::string full_name = type_name + "::" + variant.name;
        type_defs_buffer_ << variant_const << " = private constant [" << (full_name.size() + 1)
                          << " x i8] c\"" << full_name << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // For simple enums, just return the variant name
    type_defs_buffer_ << "; @derive(Debug) for " << type_name << "\n";
    type_defs_buffer_ << "define internal ptr " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    // Load tag
    type_defs_buffer_ << "  %tag_ptr = getelementptr " << llvm_type
                      << ", ptr %this, i32 0, i32 0\n";
    type_defs_buffer_ << "  %tag = load i32, ptr %tag_ptr\n";

    // Switch on tag to get variant name
    type_defs_buffer_ << "  switch i32 %tag, label %default [\n";
    for (size_t tag_value = 0; tag_value < e.variants.size(); ++tag_value) {
        type_defs_buffer_ << "    i32 " << tag_value << ", label %variant_" << tag_value << "\n";
    }
    type_defs_buffer_ << "  ]\n\n";

    // Generate labels for each variant
    int tag_idx = 0;
    for (const auto& variant : e.variants) {
        std::string variant_const = "@.debug_" + suite_prefix + type_name + "_v_" + variant.name;
        std::string full_name = type_name + "::" + variant.name;

        type_defs_buffer_ << "variant_" << tag_idx << ":\n";
        type_defs_buffer_ << "  %name_" << tag_idx << " = getelementptr [" << (full_name.size() + 1)
                          << " x i8], ptr " << variant_const << ", i32 0, i32 0\n";
        type_defs_buffer_ << "  ret ptr %name_" << tag_idx << "\n\n";
        tag_idx++;
    }

    // Default case (should never happen)
    type_defs_buffer_ << "default:\n";
    type_defs_buffer_ << "  ret ptr null\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
