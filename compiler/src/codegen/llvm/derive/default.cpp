//! # LLVM IR Generator - @derive(Default) Implementation
//!
//! This file implements the `@derive(Default)` derive macro.
//! Default generates: `func default() -> Self` (static method)
//!
//! ## Generated Code Pattern
//!
//! For a struct like:
//! ```tml
//! @derive(Default)
//! type Point {
//!     x: I32,
//!     y: I32
//! }
//! ```
//!
//! We generate:
//! ```llvm
//! define %struct.Point @tml_Point_default() {
//! entry:
//!   %ret = alloca %struct.Point
//!   %x_ptr = getelementptr %struct.Point, ptr %ret, i32 0, i32 0
//!   store i32 0, ptr %x_ptr
//!   %y_ptr = getelementptr %struct.Point, ptr %ret, i32 0, i32 1
//!   store i32 0, ptr %y_ptr
//!   %result = load %struct.Point, ptr %ret
//!   ret %struct.Point %result
//! }
//! ```

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Default) decorator
static bool has_derive_default(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Default") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Default) decorator
static bool has_derive_default(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Default") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Get the default value literal for a primitive LLVM type
static std::string get_default_value(const std::string& llvm_type) {
    // Integer types
    if (llvm_type == "i1") {
        return "0"; // false
    }
    if (llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" || llvm_type == "i64" ||
        llvm_type == "i128") {
        return "0";
    }
    // Floating point types
    if (llvm_type == "float") {
        return "0.0";
    }
    if (llvm_type == "double") {
        return "0.0";
    }
    // Pointers
    if (llvm_type == "ptr") {
        return "null";
    }
    return ""; // Non-primitive, needs method call
}

// ============================================================================
// Default Generation for Structs
// ============================================================================

/// Generate default() method for a struct with @derive(Default)
void LLVMIRGen::gen_derive_default_struct(const parser::StructDecl& s) {
    if (!has_derive_default(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_default";

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

    // Emit function definition - default() is a static method, no 'this' parameter
    type_defs_buffer_ << "; @derive(Default) for " << type_name << "\n";
    type_defs_buffer_ << "define internal " << llvm_type << " " << func_name << "() {\n";
    type_defs_buffer_ << "entry:\n";

    if (fields.empty()) {
        // Empty struct - return zeroinitializer
        type_defs_buffer_ << "  ret " << llvm_type << " zeroinitializer\n";
        type_defs_buffer_ << "}\n\n";
        return;
    }

    // Allocate result on stack
    type_defs_buffer_ << "  %ret = alloca " << llvm_type << "\n";

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Initialize each field to its default value
    for (const auto& field : fields) {
        std::string field_ptr = fresh_temp();
        type_defs_buffer_ << "  " << field_ptr << " = getelementptr " << llvm_type
                          << ", ptr %ret, i32 0, i32 " << field.index << "\n";

        std::string default_val = get_default_value(field.llvm_type);
        if (!default_val.empty()) {
            // Primitive type - use literal default value
            type_defs_buffer_ << "  store " << field.llvm_type << " " << default_val << ", ptr "
                              << field_ptr << "\n";
        } else {
            // Non-primitive type - call default() on the field type
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_default_func = "@tml_" + suite_prefix + field_type_name + "_default";

            // Call default() and store result
            std::string default_result = fresh_temp();
            type_defs_buffer_ << "  " << default_result << " = call " << field.llvm_type << " "
                              << field_default_func << "()\n";
            type_defs_buffer_ << "  store " << field.llvm_type << " " << default_result << ", ptr "
                              << field_ptr << "\n";
        }
    }

    // Load and return the result
    std::string result = fresh_temp();
    type_defs_buffer_ << "  " << result << " = load " << llvm_type << ", ptr %ret\n";
    type_defs_buffer_ << "  ret " << llvm_type << " " << result << "\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Default Generation for Enums
// ============================================================================

/// Generate default() method for an enum with @derive(Default)
/// For enums, default returns the first variant (tag 0) with default payload if any
void LLVMIRGen::gen_derive_default_enum(const parser::EnumDecl& e) {
    if (!has_derive_default(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_default";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // For enums, default returns a zeroinitializer (first variant, tag=0)
    // This works for unit variants and variants with defaultable payloads
    type_defs_buffer_ << "; @derive(Default) for " << type_name << "\n";
    type_defs_buffer_ << "define internal " << llvm_type << " " << func_name << "() {\n";
    type_defs_buffer_ << "entry:\n";
    type_defs_buffer_ << "  ret " << llvm_type << " zeroinitializer\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
