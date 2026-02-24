TML_MODULE("codegen_x86")

//! # LLVM IR Generator - @derive(PartialEq, Eq) Implementation
//!
//! This file implements the `@derive(PartialEq)` and `@derive(Eq)` derive macros.
//! PartialEq generates: `func eq(this, other: ref Self) -> Bool`
//! Eq is a marker trait that requires PartialEq.
//!
//! ## Generated Code Pattern
//!
//! For a struct like:
//! ```tml
//! @derive(PartialEq)
//! type Point {
//!     x: I32,
//!     y: I32
//! }
//! ```
//!
//! We generate:
//! ```llvm
//! define i1 @tml_Point_eq(ptr %this, ptr %other) {
//! entry:
//!   %x_this = getelementptr inbounds %struct.Point, ptr %this, i32 0, i32 0
//!   %x_val_this = load i32, ptr %x_this
//!   %x_other = getelementptr inbounds %struct.Point, ptr %other, i32 0, i32 0
//!   %x_val_other = load i32, ptr %x_other
//!   %eq_x = icmp eq i32 %x_val_this, %x_val_other
//!   br i1 %eq_x, label %check_y, label %ret_false
//!
//! check_y:
//!   ; ... similar for y field ...
//!   br label %ret_true
//!
//! ret_true:
//!   ret i1 1
//!
//! ret_false:
//!   ret i1 0
//! }
//! ```

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(PartialEq) decorator
static bool has_derive_partial_eq(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "PartialEq" || name == "Eq") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(PartialEq) decorator
static bool has_derive_partial_eq(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "PartialEq" || name == "Eq") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if a type is a primitive that can be compared directly with icmp/fcmp
static bool is_primitive_comparable(const std::string& llvm_type) {
    // Integer types
    if (llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" ||
        llvm_type == "i64" || llvm_type == "i128") {
        return true;
    }
    // Floating point types
    if (llvm_type == "float" || llvm_type == "double") {
        return true;
    }
    // Pointers (compare addresses)
    if (llvm_type == "ptr") {
        return true;
    }
    return false;
}

/// Check if a type is a floating point type
static bool is_float_type(const std::string& llvm_type) {
    return llvm_type == "float" || llvm_type == "double";
}

// ============================================================================
// PartialEq Generation for Structs
// ============================================================================

/// Generate eq() method for a struct with @derive(PartialEq)
void LLVMIRGen::gen_derive_partial_eq_struct(const parser::StructDecl& s) {
    if (!has_derive_partial_eq(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_eq";

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

    // Emit function definition to type_defs_buffer_ (ensures type is defined before use)
    type_defs_buffer_ << "; @derive(PartialEq) for " << type_name << "\n";
    type_defs_buffer_ << "define internal i1 " << func_name << "(ptr %this, ptr %other) {\n";
    type_defs_buffer_ << "entry:\n";

    if (fields.empty()) {
        // Empty struct - always equal
        type_defs_buffer_ << "  ret i1 1\n";
        type_defs_buffer_ << "}\n\n";
        return;
    }

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Generate comparison for each field
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];
        std::string next_label =
            (i + 1 < fields.size()) ? "check_" + std::to_string(i + 1) : "ret_true";

        // Get field pointers
        std::string this_ptr = fresh_temp();
        std::string other_ptr = fresh_temp();
        type_defs_buffer_ << "  " << this_ptr << " = getelementptr inbounds " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";
        type_defs_buffer_ << "  " << other_ptr << " = getelementptr inbounds " << llvm_type
                          << ", ptr %other, i32 0, i32 " << field.index << "\n";

        if (is_primitive_comparable(field.llvm_type)) {
            // Primitive type - use icmp or fcmp directly
            std::string this_val = fresh_temp();
            std::string other_val = fresh_temp();
            std::string eq_result = fresh_temp();

            type_defs_buffer_ << "  " << this_val << " = load " << field.llvm_type << ", ptr "
                              << this_ptr << "\n";
            type_defs_buffer_ << "  " << other_val << " = load " << field.llvm_type << ", ptr "
                              << other_ptr << "\n";

            if (is_float_type(field.llvm_type)) {
                // Use fcmp oeq for floating point (ordered equal)
                type_defs_buffer_ << "  " << eq_result << " = fcmp oeq " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
            } else {
                // Use icmp eq for integers and pointers
                type_defs_buffer_ << "  " << eq_result << " = icmp eq " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
            }

            type_defs_buffer_ << "  br i1 " << eq_result << ", label %" << next_label
                              << ", label %ret_false\n";
        } else {
            // Non-primitive type - call eq() method on the field
            // The field type must also implement PartialEq
            std::string eq_result = fresh_temp();

            // Determine the function name for the field's eq method
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8); // Remove "%struct." prefix
            } else {
                // For non-struct types, we assume they have an eq method
                field_type_name = field.llvm_type;
            }

            std::string field_eq_func = "@tml_" + suite_prefix + field_type_name + "_eq";

            type_defs_buffer_ << "  " << eq_result << " = call i1 " << field_eq_func << "(ptr "
                              << this_ptr << ", ptr " << other_ptr << ")\n";
            type_defs_buffer_ << "  br i1 " << eq_result << ", label %" << next_label
                              << ", label %ret_false\n";
        }

        // Emit label for next field check (except for last field)
        if (i + 1 < fields.size()) {
            type_defs_buffer_ << "check_" << (i + 1) << ":\n";
        }
    }

    // Return true - all fields matched
    type_defs_buffer_ << "ret_true:\n";
    type_defs_buffer_ << "  ret i1 1\n";

    // Return false - at least one field didn't match
    type_defs_buffer_ << "ret_false:\n";
    type_defs_buffer_ << "  ret i1 0\n";

    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// PartialEq Generation for Enums
// ============================================================================

/// Generate eq() method for an enum with @derive(PartialEq)
void LLVMIRGen::gen_derive_partial_eq_enum(const parser::EnumDecl& e) {
    if (!has_derive_partial_eq(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_eq";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Emit function definition
    type_defs_buffer_ << "; @derive(PartialEq) for " << type_name << "\n";
    type_defs_buffer_ << "define internal i1 " << func_name << "(ptr %this, ptr %other) {\n";
    type_defs_buffer_ << "entry:\n";

    // First, compare tags
    std::string this_tag_ptr = fresh_temp();
    std::string other_tag_ptr = fresh_temp();
    std::string this_tag = fresh_temp();
    std::string other_tag = fresh_temp();
    std::string tags_eq = fresh_temp();

    type_defs_buffer_ << "  " << this_tag_ptr << " = getelementptr inbounds " << llvm_type
                      << ", ptr %this, i32 0, i32 0\n";
    type_defs_buffer_ << "  " << other_tag_ptr << " = getelementptr inbounds " << llvm_type
                      << ", ptr %other, i32 0, i32 0\n";
    type_defs_buffer_ << "  " << this_tag << " = load i32, ptr " << this_tag_ptr << "\n";
    type_defs_buffer_ << "  " << other_tag << " = load i32, ptr " << other_tag_ptr << "\n";
    type_defs_buffer_ << "  " << tags_eq << " = icmp eq i32 " << this_tag << ", " << other_tag
                      << "\n";
    type_defs_buffer_ << "  br i1 " << tags_eq << ", label %compare_payload, label %ret_false\n";

    // Compare payloads based on variant
    type_defs_buffer_ << "compare_payload:\n";

    // Check if any variant has a payload
    bool has_any_payload = false;
    for (const auto& variant : e.variants) {
        if ((variant.tuple_fields.has_value() && !variant.tuple_fields->empty()) ||
            (variant.struct_fields.has_value() && !variant.struct_fields->empty())) {
            has_any_payload = true;
            break;
        }
    }

    if (!has_any_payload) {
        // No variants have payloads - just return true if tags match
        type_defs_buffer_ << "  br label %ret_true\n";
    } else {
        // Generate switch to handle each variant's payload comparison
        type_defs_buffer_ << "  switch i32 " << this_tag << ", label %ret_true [\n";

        for (size_t i = 0; i < e.variants.size(); ++i) {
            const auto& variant = e.variants[i];
            bool has_payload =
                (variant.tuple_fields.has_value() && !variant.tuple_fields->empty()) ||
                (variant.struct_fields.has_value() && !variant.struct_fields->empty());
            if (has_payload) {
                type_defs_buffer_ << "    i32 " << i << ", label %variant_" << i << "\n";
            }
        }
        type_defs_buffer_ << "  ]\n";

        // Generate comparison code for each variant with payload
        for (size_t i = 0; i < e.variants.size(); ++i) {
            const auto& variant = e.variants[i];

            bool has_tuple = variant.tuple_fields.has_value() && !variant.tuple_fields->empty();
            bool has_struct = variant.struct_fields.has_value() && !variant.struct_fields->empty();

            if (!has_tuple && !has_struct) {
                continue; // Skip unit variants
            }

            type_defs_buffer_ << "variant_" << i << ":\n";

            if (has_tuple) {
                // Tuple variant - compare each tuple element
                // Get payload pointers (index 1 in enum struct)
                std::string this_payload = fresh_temp();
                std::string other_payload = fresh_temp();
                type_defs_buffer_ << "  " << this_payload << " = getelementptr inbounds "
                                  << llvm_type << ", ptr %this, i32 0, i32 1\n";
                type_defs_buffer_ << "  " << other_payload << " = getelementptr inbounds "
                                  << llvm_type << ", ptr %other, i32 0, i32 1\n";

                // For single-element tuple, compare the element directly
                // For now, we'll just call the eq method on the payload
                // TODO: Handle multi-element tuples properly
                std::string eq_result = fresh_temp();

                // Determine payload type from the enum's union
                // For generic enums like Maybe[T], this needs instantiation info
                // For now, emit a direct memory comparison as fallback
                // This is a simplification - proper implementation needs type info
                type_defs_buffer_ << "  ; TODO: Proper tuple variant comparison\n";
                type_defs_buffer_ << "  br label %ret_true\n";
            } else if (has_struct) {
                // Struct variant - compare each field
                // TODO: Implement struct variant comparison
                type_defs_buffer_ << "  ; TODO: Proper struct variant comparison\n";
                type_defs_buffer_ << "  br label %ret_true\n";
            }
        }
    }

    // Return true
    type_defs_buffer_ << "ret_true:\n";
    type_defs_buffer_ << "  ret i1 1\n";

    // Return false
    type_defs_buffer_ << "ret_false:\n";
    type_defs_buffer_ << "  ret i1 0\n";

    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
