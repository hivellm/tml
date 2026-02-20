//! # LLVM IR Generator - @derive(Display) Implementation
//!
//! This file implements the `@derive(Display)` derive macro.
//! Display generates: `func to_string(this) -> Str`
//!
//! Display produces a user-friendly string representation (cleaner than Debug).
//! For structs: "value1, value2, value3" or the struct name for single-field structs
//! For enums: "VariantName"

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Display) decorator
static bool has_derive_display(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Display") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Display) decorator
static bool has_derive_display(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Display") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Get the appropriate to_string function for a primitive type
/// Phase 44: Use TML Display behavior impls for Bool, I8, I16, I32
static std::string get_display_func(const std::string& llvm_type) {
    if (llvm_type == "i1") {
        return "tml_Bool_to_string";
    } else if (llvm_type == "i8") {
        return "tml_I8_to_string";
    } else if (llvm_type == "i16") {
        return "tml_I16_to_string";
    } else if (llvm_type == "i32") {
        return "tml_I32_to_string";
    } else if (llvm_type == "i64" || llvm_type == "i128") {
        return "i64_to_str";
    } else if (llvm_type == "float" || llvm_type == "double") {
        return "f64_to_str";
    }
    return "";
}

/// Check if a type is a primitive that can be converted to string
static bool is_primitive_displayable(const std::string& llvm_type) {
    return llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" ||
           llvm_type == "i64" || llvm_type == "i128" || llvm_type == "float" ||
           llvm_type == "double" || llvm_type == "ptr";
}

// ============================================================================
// Display Generation for Structs
// ============================================================================

/// Generate to_string() method for a struct with @derive(Display)
void LLVMIRGen::gen_derive_display_struct(const parser::StructDecl& s) {
    if (!has_derive_display(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_to_string";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Get field info for this struct
    auto fields_it = struct_fields_.find(type_name);
    if (fields_it == struct_fields_.end()) {
        return; // No field info, can't generate
    }
    const auto& fields = fields_it->second;

    // Create string constants
    std::string separator_const = "@.display_" + suite_prefix + type_name + "_sep";

    // Emit string constants
    type_defs_buffer_ << "; @derive(Display) string constants for " << type_name << "\n";
    type_defs_buffer_ << separator_const << " = private constant [3 x i8] c\", \\00\"\n\n";

    // Emit function definition
    type_defs_buffer_ << "; @derive(Display) for " << type_name << "\n";
    type_defs_buffer_ << "define internal ptr " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    if (fields.empty()) {
        // Empty struct - return empty string
        std::string empty_const = "@.display_" + suite_prefix + type_name + "_empty";
        type_defs_buffer_ << "  ret ptr " << empty_const << "\n";
        type_defs_buffer_ << "}\n";
        // Add empty string constant
        type_defs_buffer_ << empty_const << " = private constant [1 x i8] c\"\\00\"\n\n";
        return;
    }

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    std::string current = "";

    // Convert each field to string and concatenate
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];

        // Get field value and convert to string
        std::string field_ptr = fresh_temp();
        type_defs_buffer_ << "  " << field_ptr << " = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";

        std::string value_str;
        if (field.llvm_type == "ptr") {
            // String type - use directly
            value_str = fresh_temp();
            type_defs_buffer_ << "  " << value_str << " = load ptr, ptr " << field_ptr << "\n";
        } else if (is_primitive_displayable(field.llvm_type)) {
            // Primitive type - convert to string
            std::string val = fresh_temp();
            type_defs_buffer_ << "  " << val << " = load " << field.llvm_type << ", ptr "
                              << field_ptr << "\n";

            std::string to_string_func = get_display_func(field.llvm_type);
            value_str = fresh_temp();

            if (field.llvm_type == "i1") {
                // Phase 44: tml_Bool_to_string takes i1 directly
                type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func
                                  << "(i1 " << val << ")\n";
            } else if (field.llvm_type == "i8" || field.llvm_type == "i16") {
                // Phase 44: tml_I8/I16_to_string take native types directly
                type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func << "("
                                  << field.llvm_type << " " << val << ")\n";
            } else if (field.llvm_type == "i32") {
                type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func
                                  << "(i32 " << val << ")\n";
            } else if (field.llvm_type == "i64" || field.llvm_type == "i128") {
                if (field.llvm_type == "i128") {
                    std::string trunc = fresh_temp();
                    type_defs_buffer_ << "  " << trunc << " = trunc i128 " << val << " to i64\n";
                    type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func
                                      << "(i64 " << trunc << ")\n";
                } else {
                    type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func
                                      << "(i64 " << val << ")\n";
                }
            } else if (field.llvm_type == "float") {
                std::string ext = fresh_temp();
                type_defs_buffer_ << "  " << ext << " = fpext float " << val << " to double\n";
                type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func
                                  << "(double " << ext << ")\n";
            } else if (field.llvm_type == "double") {
                type_defs_buffer_ << "  " << value_str << " = call ptr @" << to_string_func
                                  << "(double " << val << ")\n";
            }
        } else {
            // Non-primitive type - call to_string() on the field
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_display_func =
                "@tml_" + suite_prefix + field_type_name + "_to_string";
            value_str = fresh_temp();
            type_defs_buffer_ << "  " << value_str << " = call ptr " << field_display_func
                              << "(ptr " << field_ptr << ")\n";
        }

        if (i == 0) {
            current = value_str;
        } else {
            // Add separator and concatenate
            std::string sep = fresh_temp();
            type_defs_buffer_ << "  " << sep << " = getelementptr [3 x i8], ptr " << separator_const
                              << ", i32 0, i32 0\n";
            std::string with_sep = fresh_temp();
            type_defs_buffer_ << "  " << with_sep << " = call ptr @str_concat_opt(ptr " << current
                              << ", ptr " << sep << ")\n";
            std::string with_value = fresh_temp();
            type_defs_buffer_ << "  " << with_value << " = call ptr @str_concat_opt(ptr "
                              << with_sep << ", ptr " << value_str << ")\n";
            current = with_value;
        }
    }

    type_defs_buffer_ << "  ret ptr " << current << "\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Display Generation for Enums
// ============================================================================

/// Generate to_string() method for an enum with @derive(Display)
void LLVMIRGen::gen_derive_display_enum(const parser::EnumDecl& e) {
    if (!has_derive_display(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_to_string";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Emit variant name constants (just variant name, not type::variant)
    type_defs_buffer_ << "; @derive(Display) string constants for " << type_name << "\n";
    for (const auto& variant : e.variants) {
        std::string variant_const = "@.display_" + suite_prefix + type_name + "_v_" + variant.name;
        type_defs_buffer_ << variant_const << " = private constant [" << (variant.name.size() + 1)
                          << " x i8] c\"" << variant.name << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // For simple enums, just return the variant name
    type_defs_buffer_ << "; @derive(Display) for " << type_name << "\n";
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
        std::string variant_const = "@.display_" + suite_prefix + type_name + "_v_" + variant.name;

        type_defs_buffer_ << "variant_" << tag_idx << ":\n";
        type_defs_buffer_ << "  %name_" << tag_idx << " = getelementptr ["
                          << (variant.name.size() + 1) << " x i8], ptr " << variant_const
                          << ", i32 0, i32 0\n";
        type_defs_buffer_ << "  ret ptr %name_" << tag_idx << "\n\n";
        tag_idx++;
    }

    // Default case (should never happen)
    type_defs_buffer_ << "default:\n";
    type_defs_buffer_ << "  ret ptr null\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
