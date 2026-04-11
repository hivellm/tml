TML_MODULE("codegen_x86")

//! # LLVM IR Generator - @derive(Deserialize) Implementation
//!
//! This file implements the `@derive(Deserialize)` derive macro.
//! Deserialize generates: `func from_json(s: Str) -> Outcome[Self, Str]` (static)
//!
//! Parses JSON into structs and enums.
//! Uses runtime JSON parsing functions for the heavy lifting.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Deserialize) decorator
static bool has_derive_deserialize(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive" || deco.name == "auto") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Deserialize" || name == "deserialize") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Deserialize) decorator
static bool has_derive_deserialize(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive" || deco.name == "auto") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Deserialize" || name == "deserialize") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// ============================================================================
// Deserialize Generation for Structs
// ============================================================================

/// Generate from_json() method for a struct with @derive(Deserialize)
/// This is a static method that parses JSON and returns Outcome[Self, Str]
void LLVMIRGen::gen_derive_deserialize_struct(const parser::StructDecl& s) {
    if (!has_derive_deserialize(s)) {
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

    std::string func_name = "@" + mangle_impl_method(type_name, "from_json");

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

    // Ensure Outcome[TypeName, Str] type is defined
    auto self_type = std::make_shared<types::Type>();
    self_type->kind = types::NamedType{type_name, "", {}};
    auto str_type = types::make_str();
    std::vector<types::TypePtr> outcome_type_args = {self_type, str_type};
    std::string outcome_mangled = require_enum_instantiation("Outcome", outcome_type_args);
    std::string outcome_type = "%struct." + outcome_mangled;

    // Error message constants
    std::string parse_err = "@.deser_" + suite_prefix + type_name + "_parse_err";
    std::string field_err = "@.deser_" + suite_prefix + type_name + "_field_err";

    type_defs_buffer_ << "; @derive(Deserialize) string constants for " << type_name << "\n";
    type_defs_buffer_ << parse_err << " = private constant [18 x i8] c\"JSON parse failed\\00\"\n";
    type_defs_buffer_ << field_err << " = private constant [19 x i8] c\"Missing JSON field\\00\"\n";

    // Field name constants for JSON parsing
    for (const auto& field : fields) {
        std::string field_const = "@.deser_" + suite_prefix + type_name + "_f_" + field.name;
        type_defs_buffer_ << field_const << " = private constant [" << (field.name.size() + 1)
                          << " x i8] c\"" << field.name << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // Emit JSON runtime declarations once (guard with static flag)
    if (!json_runtime_declared_) {
        json_runtime_declared_ = true;
        // Handle-based JSON runtime API:
        // tml_json_parse(ptr) -> i64 handle (-1 on error)
        // tml_json_free(i64 handle) -> void
        // tml_json_object_get_i64(i64 handle, ptr key) -> i64 value
        // tml_json_object_get_f64(i64 handle, ptr key) -> double value
        // tml_json_object_get_string(i64 handle, ptr key) -> ptr (static buffer)
        // tml_json_object_get_bool(i64 handle, ptr key) -> i32 (1/0)
        // tml_json_object_get(i64 handle, ptr key) -> i64 sub-handle
        // tml_json_to_string(i64 handle) -> ptr (static buffer)
        type_defs_buffer_ << "declare i64 @tml_json_parse(ptr)\n";
        type_defs_buffer_ << "declare void @tml_json_free(i64)\n";
        type_defs_buffer_ << "declare i64 @tml_json_object_get_i64(i64, ptr)\n";
        type_defs_buffer_ << "declare ptr @tml_json_object_get_string(i64, ptr)\n";
        type_defs_buffer_ << "declare double @tml_json_object_get_f64(i64, ptr)\n";
        type_defs_buffer_ << "declare i32 @tml_json_object_get_bool(i64, ptr)\n";
        type_defs_buffer_ << "declare i64 @tml_json_object_get(i64, ptr)\n";
        type_defs_buffer_ << "declare ptr @tml_json_to_string(i64)\n";
        type_defs_buffer_ << "\n";
    }

    // Emit function definition - static method returning Outcome[Self, Str]
    type_defs_buffer_ << "; @derive(Deserialize) for " << type_name << "\n";
    type_defs_buffer_ << "define internal " << outcome_type << " " << func_name
                      << "(ptr %json_str) {\n";
    type_defs_buffer_ << "entry:\n";

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Parse JSON using runtime function: tml_json_parse(str) -> i64 handle (-1 on error)
    std::string json_handle = fresh_temp();
    type_defs_buffer_ << "  " << json_handle << " = call i64 @tml_json_parse(ptr %json_str)\n";

    // Check if parse succeeded (handle == -1 means error)
    std::string is_err = fresh_temp();
    type_defs_buffer_ << "  " << is_err << " = icmp eq i64 " << json_handle << ", -1\n";
    type_defs_buffer_ << "  br i1 " << is_err << ", label %parse_error, label %parse_ok\n\n";

    type_defs_buffer_ << "parse_error:\n";
    // Return Err("JSON parse failed")
    std::string err_result = fresh_temp();
    type_defs_buffer_ << "  " << err_result << " = alloca " << outcome_type << "\n";
    std::string err_tag = fresh_temp();
    type_defs_buffer_ << "  " << err_tag << " = getelementptr inbounds " << outcome_type << ", ptr "
                      << err_result << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 1, ptr " << err_tag << " ; Err tag\n";
    std::string err_payload = fresh_temp();
    type_defs_buffer_ << "  " << err_payload << " = getelementptr inbounds " << outcome_type
                      << ", ptr " << err_result << ", i32 0, i32 1\n";
    std::string err_msg = fresh_temp();
    type_defs_buffer_ << "  " << err_msg << " = getelementptr inbounds [19 x i8], ptr " << parse_err
                      << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store ptr " << err_msg << ", ptr " << err_payload << "\n";
    std::string err_ret = fresh_temp();
    type_defs_buffer_ << "  " << err_ret << " = load " << outcome_type << ", ptr " << err_result
                      << "\n";
    type_defs_buffer_ << "  ret " << outcome_type << " " << err_ret << "\n\n";

    type_defs_buffer_ << "parse_ok:\n";
    // Allocate result struct
    std::string result_ptr = fresh_temp();
    type_defs_buffer_ << "  " << result_ptr << " = alloca " << llvm_type << "\n";

    // Extract each field from JSON
    for (const auto& field : fields) {
        std::string field_const = "@.deser_" + suite_prefix + type_name + "_f_" + field.name;
        std::string field_name_ptr = fresh_temp();
        type_defs_buffer_ << "  " << field_name_ptr << " = getelementptr inbounds ["
                          << (field.name.size() + 1) << " x i8], ptr " << field_const
                          << ", i32 0, i32 0\n";

        std::string field_ptr = fresh_temp();
        type_defs_buffer_ << "  " << field_ptr << " = getelementptr inbounds " << llvm_type
                          << ", ptr " << result_ptr << ", i32 0, i32 " << field.index << "\n";

        if (field.llvm_type == "ptr") {
            // String field - tml_json_object_get_string(handle, key) -> ptr
            std::string str_val = fresh_temp();
            type_defs_buffer_ << "  " << str_val << " = call ptr @tml_json_object_get_string(i64 "
                              << json_handle << ", ptr " << field_name_ptr << ")\n";
            type_defs_buffer_ << "  store ptr " << str_val << ", ptr " << field_ptr << "\n";
        } else if (field.llvm_type == "i1") {
            // Bool field - tml_json_object_get_bool(handle, key) -> i32
            std::string bool_val = fresh_temp();
            type_defs_buffer_ << "  " << bool_val << " = call i32 @tml_json_object_get_bool(i64 "
                              << json_handle << ", ptr " << field_name_ptr << ")\n";
            std::string bool_i1 = fresh_temp();
            type_defs_buffer_ << "  " << bool_i1 << " = trunc i32 " << bool_val << " to i1\n";
            type_defs_buffer_ << "  store i1 " << bool_i1 << ", ptr " << field_ptr << "\n";
        } else if (field.llvm_type == "i32") {
            // I32 field - tml_json_object_get_i64(handle, key) -> i64, then trunc
            std::string i64_val = fresh_temp();
            type_defs_buffer_ << "  " << i64_val << " = call i64 @tml_json_object_get_i64(i64 "
                              << json_handle << ", ptr " << field_name_ptr << ")\n";
            std::string i32_val = fresh_temp();
            type_defs_buffer_ << "  " << i32_val << " = trunc i64 " << i64_val << " to i32\n";
            type_defs_buffer_ << "  store i32 " << i32_val << ", ptr " << field_ptr << "\n";
        } else if (field.llvm_type == "i64") {
            // I64 field - tml_json_object_get_i64(handle, key) -> i64
            std::string i64_val = fresh_temp();
            type_defs_buffer_ << "  " << i64_val << " = call i64 @tml_json_object_get_i64(i64 "
                              << json_handle << ", ptr " << field_name_ptr << ")\n";
            type_defs_buffer_ << "  store i64 " << i64_val << ", ptr " << field_ptr << "\n";
        } else if (field.llvm_type == "double" || field.llvm_type == "float") {
            // Float field - tml_json_object_get_f64(handle, key) -> double
            std::string f64_val = fresh_temp();
            type_defs_buffer_ << "  " << f64_val << " = call double @tml_json_object_get_f64(i64 "
                              << json_handle << ", ptr " << field_name_ptr << ")\n";
            if (field.llvm_type == "float") {
                std::string f32_val = fresh_temp();
                type_defs_buffer_ << "  " << f32_val << " = fptrunc double " << f64_val
                                  << " to float\n";
                type_defs_buffer_ << "  store float " << f32_val << ", ptr " << field_ptr << "\n";
            } else {
                type_defs_buffer_ << "  store double " << f64_val << ", ptr " << field_ptr << "\n";
            }
        } else if (field.llvm_type == "i8" || field.llvm_type == "i16") {
            // Small int - tml_json_object_get_i64 then trunc
            std::string i64_val = fresh_temp();
            type_defs_buffer_ << "  " << i64_val << " = call i64 @tml_json_object_get_i64(i64 "
                              << json_handle << ", ptr " << field_name_ptr << ")\n";
            std::string small_val = fresh_temp();
            type_defs_buffer_ << "  " << small_val << " = trunc i64 " << i64_val << " to "
                              << field.llvm_type << "\n";
            type_defs_buffer_ << "  store " << field.llvm_type << " " << small_val << ", ptr "
                              << field_ptr << "\n";
        } else {
            // Non-primitive - recursively call from_json
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            // Get nested object as sub-handle, then serialize to string for from_json
            std::string sub_handle = fresh_temp();
            type_defs_buffer_ << "  " << sub_handle << " = call i64 @tml_json_object_get(i64 "
                              << json_handle << ", ptr " << field_name_ptr << ")\n";
            std::string nested_json = fresh_temp();
            type_defs_buffer_ << "  " << nested_json << " = call ptr @tml_json_to_string(i64 "
                              << sub_handle << ")\n";
            // Free the sub-handle (it was cloned by tml_json_object_get)
            type_defs_buffer_ << "  call void @tml_json_free(i64 " << sub_handle << ")\n";

            // Call nested from_json recursively
            std::string nested_outcome_type;
            {
                // Build Outcome[FieldType, Str] for the nested call return
                auto nested_self_type = std::make_shared<types::Type>();
                nested_self_type->kind = types::NamedType{field_type_name, "", {}};
                auto nested_str_type = types::make_str();
                std::vector<types::TypePtr> nested_args = {nested_self_type, nested_str_type};
                std::string nested_mangled = require_enum_instantiation("Outcome", nested_args);
                nested_outcome_type = "%struct." + nested_mangled;
            }

            std::string nested_func = "@" + mangle_impl_method(field_type_name, "from_json");
            std::string nested_result = fresh_temp();
            type_defs_buffer_ << "  " << nested_result << " = call " << nested_outcome_type << " "
                              << nested_func << "(ptr " << nested_json << ")\n";

            // Check if nested deserialization succeeded (tag == 0 means Ok)
            std::string nested_alloca = fresh_temp();
            type_defs_buffer_ << "  " << nested_alloca << " = alloca " << nested_outcome_type
                              << "\n";
            type_defs_buffer_ << "  store " << nested_outcome_type << " " << nested_result
                              << ", ptr " << nested_alloca << "\n";
            std::string nested_tag_ptr = fresh_temp();
            type_defs_buffer_ << "  " << nested_tag_ptr << " = getelementptr inbounds "
                              << nested_outcome_type << ", ptr " << nested_alloca
                              << ", i32 0, i32 0\n";
            std::string nested_tag = fresh_temp();
            type_defs_buffer_ << "  " << nested_tag << " = load i32, ptr " << nested_tag_ptr
                              << "\n";
            std::string nested_is_err = fresh_temp();
            type_defs_buffer_ << "  " << nested_is_err << " = icmp ne i32 " << nested_tag
                              << ", 0\n";

            std::string nested_ok_label = "nested_ok_" + field.name;
            std::string nested_err_label = "nested_err_" + field.name;
            type_defs_buffer_ << "  br i1 " << nested_is_err << ", label %" << nested_err_label
                              << ", label %" << nested_ok_label << "\n\n";

            // Error path — propagate the nested error
            type_defs_buffer_ << nested_err_label << ":\n";
            type_defs_buffer_ << "  call void @tml_json_free(i64 " << json_handle << ")\n";
            // Build Err result with the nested error message
            std::string prop_err = fresh_temp();
            type_defs_buffer_ << "  " << prop_err << " = alloca " << outcome_type << "\n";
            std::string prop_tag = fresh_temp();
            type_defs_buffer_ << "  " << prop_tag << " = getelementptr inbounds " << outcome_type
                              << ", ptr " << prop_err << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store i32 1, ptr " << prop_tag << " ; Err tag\n";
            std::string prop_payload = fresh_temp();
            type_defs_buffer_ << "  " << prop_payload << " = getelementptr inbounds "
                              << outcome_type << ", ptr " << prop_err << ", i32 0, i32 1\n";
            // Copy error payload from nested result
            std::string nested_payload_ptr = fresh_temp();
            type_defs_buffer_ << "  " << nested_payload_ptr << " = getelementptr inbounds "
                              << nested_outcome_type << ", ptr " << nested_alloca
                              << ", i32 0, i32 1\n";
            std::string nested_err_val = fresh_temp();
            type_defs_buffer_ << "  " << nested_err_val << " = load ptr, ptr " << nested_payload_ptr
                              << "\n";
            type_defs_buffer_ << "  store ptr " << nested_err_val << ", ptr " << prop_payload
                              << "\n";
            std::string prop_ret = fresh_temp();
            type_defs_buffer_ << "  " << prop_ret << " = load " << outcome_type << ", ptr "
                              << prop_err << "\n";
            type_defs_buffer_ << "  ret " << outcome_type << " " << prop_ret << "\n\n";

            // Ok path — extract value and store in field
            type_defs_buffer_ << nested_ok_label << ":\n";
            std::string nested_ok_payload = fresh_temp();
            type_defs_buffer_ << "  " << nested_ok_payload << " = getelementptr inbounds "
                              << nested_outcome_type << ", ptr " << nested_alloca
                              << ", i32 0, i32 1\n";
            // Copy the nested struct value to the field
            type_defs_buffer_ << "  call void @llvm.memcpy.p0.p0.i64(ptr " << field_ptr << ", ptr "
                              << nested_ok_payload << ", i64 "
                              << "ptrtoint(ptr getelementptr(" << field.llvm_type
                              << ", ptr null, i32 1) to i64)"
                              << ", i1 false)\n";
        }
    }

    // Free JSON handle
    type_defs_buffer_ << "  call void @tml_json_free(i64 " << json_handle << ")\n";

    // Build Ok(result) and return
    std::string ok_result = fresh_temp();
    type_defs_buffer_ << "  " << ok_result << " = alloca " << outcome_type << "\n";
    std::string ok_tag = fresh_temp();
    type_defs_buffer_ << "  " << ok_tag << " = getelementptr inbounds " << outcome_type << ", ptr "
                      << ok_result << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 0, ptr " << ok_tag << " ; Ok tag\n";
    std::string ok_payload = fresh_temp();
    type_defs_buffer_ << "  " << ok_payload << " = getelementptr inbounds " << outcome_type
                      << ", ptr " << ok_result << ", i32 0, i32 1\n";

    // Copy struct data to payload
    std::string struct_val = fresh_temp();
    type_defs_buffer_ << "  " << struct_val << " = load " << llvm_type << ", ptr " << result_ptr
                      << "\n";
    type_defs_buffer_ << "  store " << llvm_type << " " << struct_val << ", ptr " << ok_payload
                      << "\n";

    std::string ok_ret = fresh_temp();
    type_defs_buffer_ << "  " << ok_ret << " = load " << outcome_type << ", ptr " << ok_result
                      << "\n";
    type_defs_buffer_ << "  ret " << outcome_type << " " << ok_ret << "\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Deserialize Generation for Enums
// ============================================================================

/// Generate from_json() method for an enum with @derive(Deserialize)
void LLVMIRGen::gen_derive_deserialize_enum(const parser::EnumDecl& e) {
    if (!has_derive_deserialize(e)) {
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

    std::string func_name = "@" + mangle_impl_method(type_name, "from_json");

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

    // String constants
    std::string variant_key = "@.deser_" + suite_prefix + type_name + "_vkey";
    std::string parse_err = "@.deser_" + suite_prefix + type_name + "_perr";
    std::string variant_err = "@.deser_" + suite_prefix + type_name + "_verr";

    type_defs_buffer_ << "; @derive(Deserialize) string constants for " << type_name << "\n";
    type_defs_buffer_ << variant_key << " = private constant [8 x i8] c\"variant\\00\"\n";
    type_defs_buffer_ << parse_err << " = private constant [19 x i8] c\"JSON parse failed\\00\"\n";
    type_defs_buffer_ << variant_err << " = private constant [16 x i8] c\"Unknown variant\\00\"\n";

    // Variant name constants
    for (const auto& variant : e.variants) {
        std::string var_const = "@.deser_" + suite_prefix + type_name + "_v_" + variant.name;
        type_defs_buffer_ << var_const << " = private constant [" << (variant.name.size() + 1)
                          << " x i8] c\"" << variant.name << "\\00\"\n";
    }
    type_defs_buffer_ << "\n";

    // Emit function definition
    type_defs_buffer_ << "; @derive(Deserialize) for " << type_name << "\n";
    type_defs_buffer_ << "define internal " << outcome_type << " " << func_name
                      << "(ptr %json_str) {\n";
    type_defs_buffer_ << "entry:\n";

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Parse JSON using handle-based API
    std::string json_handle = fresh_temp();
    type_defs_buffer_ << "  " << json_handle << " = call i64 @tml_json_parse(ptr %json_str)\n";

    // Check parse result (handle == -1 means error)
    std::string is_err = fresh_temp();
    type_defs_buffer_ << "  " << is_err << " = icmp eq i64 " << json_handle << ", -1\n";
    type_defs_buffer_ << "  br i1 " << is_err << ", label %parse_error, label %get_variant\n\n";

    type_defs_buffer_ << "parse_error:\n";
    std::string err_result = fresh_temp();
    type_defs_buffer_ << "  " << err_result << " = alloca " << outcome_type << "\n";
    std::string err_tag = fresh_temp();
    type_defs_buffer_ << "  " << err_tag << " = getelementptr inbounds " << outcome_type << ", ptr "
                      << err_result << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 1, ptr " << err_tag << "\n";
    std::string err_payload = fresh_temp();
    type_defs_buffer_ << "  " << err_payload << " = getelementptr inbounds " << outcome_type
                      << ", ptr " << err_result << ", i32 0, i32 1\n";
    std::string err_msg = fresh_temp();
    type_defs_buffer_ << "  " << err_msg << " = getelementptr inbounds [19 x i8], ptr " << parse_err
                      << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store ptr " << err_msg << ", ptr " << err_payload << "\n";
    std::string err_ret = fresh_temp();
    type_defs_buffer_ << "  " << err_ret << " = load " << outcome_type << ", ptr " << err_result
                      << "\n";
    type_defs_buffer_ << "  ret " << outcome_type << " " << err_ret << "\n\n";

    type_defs_buffer_ << "get_variant:\n";
    // Get "variant" field from JSON
    std::string vkey_ptr = fresh_temp();
    type_defs_buffer_ << "  " << vkey_ptr << " = getelementptr inbounds [8 x i8], ptr "
                      << variant_key << ", i32 0, i32 0\n";
    std::string variant_str = fresh_temp();
    type_defs_buffer_ << "  " << variant_str << " = call ptr @tml_json_object_get_string(i64 "
                      << json_handle << ", ptr " << vkey_ptr << ")\n";
    type_defs_buffer_ << "  call void @tml_json_free(i64 " << json_handle << ")\n";

    // Compare with each variant name
    int tag = 0;
    for (const auto& variant : e.variants) {
        std::string var_const = "@.deser_" + suite_prefix + type_name + "_v_" + variant.name;
        std::string var_ptr = fresh_temp();
        type_defs_buffer_ << "  " << var_ptr << " = getelementptr inbounds ["
                          << (variant.name.size() + 1) << " x i8], ptr " << var_const
                          << ", i32 0, i32 0\n";
        std::string cmp = fresh_temp();
        type_defs_buffer_ << "  " << cmp << " = call i32 @strcmp(ptr " << variant_str << ", ptr "
                          << var_ptr << ")\n";
        std::string is_match = fresh_temp();
        type_defs_buffer_ << "  " << is_match << " = icmp eq i32 " << cmp << ", 0\n";
        type_defs_buffer_ << "  br i1 " << is_match << ", label %match_" << tag << ", label %check_"
                          << (tag + 1) << "\n\n";

        type_defs_buffer_ << "match_" << tag << ":\n";
        // Return Ok with this variant
        std::string ok_result = fresh_temp();
        type_defs_buffer_ << "  " << ok_result << " = alloca " << outcome_type << "\n";
        std::string ok_tag_ptr = fresh_temp();
        type_defs_buffer_ << "  " << ok_tag_ptr << " = getelementptr inbounds " << outcome_type
                          << ", ptr " << ok_result << ", i32 0, i32 0\n";
        type_defs_buffer_ << "  store i32 0, ptr " << ok_tag_ptr << "\n";
        std::string ok_payload = fresh_temp();
        type_defs_buffer_ << "  " << ok_payload << " = getelementptr inbounds " << outcome_type
                          << ", ptr " << ok_result << ", i32 0, i32 1\n";
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
    type_defs_buffer_ << "  " << unk_tag << " = getelementptr inbounds " << outcome_type << ", ptr "
                      << unk_result << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 1, ptr " << unk_tag << "\n";
    std::string unk_payload = fresh_temp();
    type_defs_buffer_ << "  " << unk_payload << " = getelementptr inbounds " << outcome_type
                      << ", ptr " << unk_result << ", i32 0, i32 1\n";
    std::string unk_msg = fresh_temp();
    type_defs_buffer_ << "  " << unk_msg << " = getelementptr inbounds [16 x i8], ptr "
                      << variant_err << ", i32 0, i32 0\n";
    type_defs_buffer_ << "  store ptr " << unk_msg << ", ptr " << unk_payload << "\n";
    std::string unk_ret = fresh_temp();
    type_defs_buffer_ << "  " << unk_ret << " = load " << outcome_type << ", ptr " << unk_result
                      << "\n";
    type_defs_buffer_ << "  ret " << outcome_type << " " << unk_ret << "\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
