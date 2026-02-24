TML_MODULE("codegen_x86")

//! # LLVM IR Generator - @derive(Serialize) Implementation
//!
//! This file implements the `@derive(Serialize)` derive macro.
//! Serialize generates: `func to_json(this) -> Str`
//!
//! Produces JSON representation of structs and enums.
//! For structs: {"field1": value1, "field2": value2}
//! For enums: {"variant": "VariantName"} or {"variant": "Name", "value": ...}

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Serialize) decorator
static bool has_derive_serialize(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Serialize") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Serialize) decorator
static bool has_derive_serialize(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Serialize") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Get the appropriate to_string function for a primitive type (for JSON values)
/// Phase 45: All types use TML Display behavior impls
static std::string get_json_value_func(const std::string& llvm_type) {
    if (llvm_type == "i1") {
        return "tml_Bool_to_string"; // Will produce "true" or "false"
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

/// Check if a type is a primitive that can be serialized directly
static bool is_primitive_serializable(const std::string& llvm_type) {
    return llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" ||
           llvm_type == "i64" || llvm_type == "i128" || llvm_type == "float" ||
           llvm_type == "double" || llvm_type == "ptr";
}

// ============================================================================
// Serialize Generation for Structs
// ============================================================================

/// Generate to_json() method for a struct with @derive(Serialize)
void LLVMIRGen::gen_derive_serialize_struct(const parser::StructDecl& s) {
    if (!has_derive_serialize(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_to_json";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);
    // Register as allocating function for Str temp tracking
    allocating_functions_.insert("to_json");

    // Get field info for this struct
    auto fields_it = struct_fields_.find(type_name);
    if (fields_it == struct_fields_.end()) {
        return; // No field info, can't generate
    }
    const auto& fields = fields_it->second;

    // Create string constants for JSON formatting
    std::string open_brace = "@.json_" + suite_prefix + type_name + "_open";
    std::string close_brace = "@.json_" + suite_prefix + type_name + "_close";
    std::string separator = "@.json_" + suite_prefix + type_name + "_sep";
    std::string colon = "@.json_" + suite_prefix + type_name + "_colon";
    std::string quote = "@.json_" + suite_prefix + type_name + "_quote";

    // Emit string constants (use \22 for double quote in LLVM IR)
    type_defs_buffer_ << "; @derive(Serialize) string constants for " << type_name << "\n";
    type_defs_buffer_ << open_brace << " = private constant [2 x i8] c\"{\\00\"\n";
    type_defs_buffer_ << close_brace << " = private constant [2 x i8] c\"}\\00\"\n";
    type_defs_buffer_ << separator << " = private constant [3 x i8] c\", \\00\"\n";
    type_defs_buffer_ << colon << " = private constant [4 x i8] c\"\\22: \\00\"\n";
    type_defs_buffer_ << quote << " = private constant [2 x i8] c\"\\22\\00\"\n";

    // Emit field name constants (use \22 for double quote)
    for (const auto& field : fields) {
        std::string field_const = "@.json_" + suite_prefix + type_name + "_f_" + field.name;
        type_defs_buffer_ << field_const << " = private constant [" << (field.name.size() + 2)
                          << " x i8] c\"\\22" << field.name << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // Emit function definition
    type_defs_buffer_ << "; @derive(Serialize) for " << type_name << "\n";
    type_defs_buffer_ << "define internal ptr " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Start with opening brace
    std::string current = fresh_temp();
    type_defs_buffer_ << "  " << current << " = getelementptr inbounds [2 x i8], ptr " << open_brace
                      << ", i32 0, i32 0\n";

    // Add each field
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];

        // Add field name with quote
        std::string field_const = "@.json_" + suite_prefix + type_name + "_f_" + field.name;
        std::string field_name = fresh_temp();
        type_defs_buffer_ << "  " << field_name << " = getelementptr inbounds ["
                          << (field.name.size() + 2) << " x i8], ptr " << field_const
                          << ", i32 0, i32 0\n";

        std::string with_name = fresh_temp();
        type_defs_buffer_ << "  " << with_name << " = call ptr @str_concat_opt(ptr " << current
                          << ", ptr " << field_name << ")\n";

        // Add colon
        std::string colon_ptr = fresh_temp();
        type_defs_buffer_ << "  " << colon_ptr << " = getelementptr inbounds [4 x i8], ptr "
                          << colon << ", i32 0, i32 0\n";
        std::string with_colon = fresh_temp();
        type_defs_buffer_ << "  " << with_colon << " = call ptr @str_concat_opt(ptr " << with_name
                          << ", ptr " << colon_ptr << ")\n";

        // Get field value and convert to JSON
        std::string field_ptr = fresh_temp();
        type_defs_buffer_ << "  " << field_ptr << " = getelementptr inbounds " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";

        std::string value_str;
        bool needs_quotes = false;

        if (field.llvm_type == "ptr") {
            // String type - load and quote
            value_str = fresh_temp();
            type_defs_buffer_ << "  " << value_str << " = load ptr, ptr " << field_ptr << "\n";
            needs_quotes = true;
        } else if (is_primitive_serializable(field.llvm_type)) {
            // Primitive type - convert to string
            std::string val = fresh_temp();
            type_defs_buffer_ << "  " << val << " = load " << field.llvm_type << ", ptr "
                              << field_ptr << "\n";

            std::string to_string_func = get_json_value_func(field.llvm_type);
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
            // Non-primitive type - call to_json() on the field
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_json_func = "@tml_" + suite_prefix + field_type_name + "_to_json";
            value_str = fresh_temp();
            type_defs_buffer_ << "  " << value_str << " = call ptr " << field_json_func << "(ptr "
                              << field_ptr << ")\n";
        }

        std::string with_value;
        if (needs_quotes) {
            // Add quote, value, quote
            std::string quote_ptr = fresh_temp();
            type_defs_buffer_ << "  " << quote_ptr << " = getelementptr inbounds [2 x i8], ptr "
                              << quote << ", i32 0, i32 0\n";
            std::string with_open_quote = fresh_temp();
            type_defs_buffer_ << "  " << with_open_quote << " = call ptr @str_concat_opt(ptr "
                              << with_colon << ", ptr " << quote_ptr << ")\n";
            std::string with_str = fresh_temp();
            type_defs_buffer_ << "  " << with_str << " = call ptr @str_concat_opt(ptr "
                              << with_open_quote << ", ptr " << value_str << ")\n";
            with_value = fresh_temp();
            type_defs_buffer_ << "  " << with_value << " = call ptr @str_concat_opt(ptr "
                              << with_str << ", ptr " << quote_ptr << ")\n";
        } else {
            with_value = fresh_temp();
            type_defs_buffer_ << "  " << with_value << " = call ptr @str_concat_opt(ptr "
                              << with_colon << ", ptr " << value_str << ")\n";
        }
        current = with_value;

        // Add separator if not last field
        if (i < fields.size() - 1) {
            std::string sep = fresh_temp();
            type_defs_buffer_ << "  " << sep << " = getelementptr inbounds [3 x i8], ptr "
                              << separator << ", i32 0, i32 0\n";
            std::string with_sep = fresh_temp();
            type_defs_buffer_ << "  " << with_sep << " = call ptr @str_concat_opt(ptr " << current
                              << ", ptr " << sep << ")\n";
            current = with_sep;
        }
    }

    // Add closing brace
    std::string close = fresh_temp();
    type_defs_buffer_ << "  " << close << " = getelementptr inbounds [2 x i8], ptr " << close_brace
                      << ", i32 0, i32 0\n";
    std::string result = fresh_temp();
    type_defs_buffer_ << "  " << result << " = call ptr @str_concat_opt(ptr " << current << ", ptr "
                      << close << ")\n";

    type_defs_buffer_ << "  ret ptr " << result << "\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Serialize Generation for Enums
// ============================================================================

/// Generate to_json() method for an enum with @derive(Serialize)
void LLVMIRGen::gen_derive_serialize_enum(const parser::EnumDecl& e) {
    if (!has_derive_serialize(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_to_json";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Emit variant name constants as JSON strings {"variant": "Name"}
    // Use \22 for double quotes in LLVM IR
    type_defs_buffer_ << "; @derive(Serialize) string constants for " << type_name << "\n";
    for (const auto& variant : e.variants) {
        std::string variant_const = "@.json_" + suite_prefix + type_name + "_v_" + variant.name;
        // Format: {"variant": "Name"} -> use \22 for quotes
        std::string json_str = "{\\22variant\\22: \\22" + variant.name + "\\22}";
        // Calculate length: {"variant": "Name"} where each " is 1 byte
        // { + " + variant + " + : + space + " + name + " + } + null
        // = 1 + 1 + 7 + 1 + 1 + 1 + 1 + name.size() + 1 + 1 + 1 = 16 + name.size()
        size_t len = 16 + variant.name.size();
        type_defs_buffer_ << variant_const << " = private constant [" << len << " x i8] c\""
                          << json_str << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // For simple enums, return JSON object with variant
    type_defs_buffer_ << "; @derive(Serialize) for " << type_name << "\n";
    type_defs_buffer_ << "define internal ptr " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    // Load tag
    type_defs_buffer_ << "  %tag_ptr = getelementptr inbounds " << llvm_type
                      << ", ptr %this, i32 0, i32 0\n";
    type_defs_buffer_ << "  %tag = load i32, ptr %tag_ptr\n";

    // Switch on tag to get variant JSON
    type_defs_buffer_ << "  switch i32 %tag, label %default [\n";
    for (size_t tag_value = 0; tag_value < e.variants.size(); ++tag_value) {
        type_defs_buffer_ << "    i32 " << tag_value << ", label %variant_" << tag_value << "\n";
    }
    type_defs_buffer_ << "  ]\n\n";

    // Generate labels for each variant
    int tag_idx = 0;
    for (const auto& variant : e.variants) {
        std::string variant_const = "@.json_" + suite_prefix + type_name + "_v_" + variant.name;
        // Calculate length: 16 + name.size()
        size_t len = 16 + variant.name.size();

        type_defs_buffer_ << "variant_" << tag_idx << ":\n";
        type_defs_buffer_ << "  %json_" << tag_idx << " = getelementptr inbounds [" << len
                          << " x i8], ptr " << variant_const << ", i32 0, i32 0\n";
        type_defs_buffer_ << "  ret ptr %json_" << tag_idx << "\n\n";
        tag_idx++;
    }

    // Default case
    type_defs_buffer_ << "default:\n";
    type_defs_buffer_ << "  ret ptr null\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
