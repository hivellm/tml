//! # LLVM IR Generator - @derive(Duplicate, Copy) Implementation
//!
//! This file implements the `@derive(Duplicate)` and `@derive(Copy)` derive macros.
//! Duplicate generates: `func duplicate(this) -> Self`
//! Copy is a marker trait that requires Duplicate (bitwise copy semantics).
//!
//! ## Generated Code Pattern
//!
//! For a struct like:
//! ```tml
//! @derive(Duplicate)
//! type Point {
//!     x: I32,
//!     y: I32
//! }
//! ```
//!
//! We generate:
//! ```llvm
//! define void @tml_Point_duplicate(ptr sret(%struct.Point) %ret, ptr %this) {
//! entry:
//!   %x_ptr = getelementptr %struct.Point, ptr %this, i32 0, i32 0
//!   %x_val = load i32, ptr %x_ptr
//!   %ret_x = getelementptr %struct.Point, ptr %ret, i32 0, i32 0
//!   store i32 %x_val, ptr %ret_x
//!   ; ... similar for y field ...
//!   ret void
//! }
//! ```

#include "codegen/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Duplicate) or @derive(Copy) decorator
static bool has_derive_duplicate(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Duplicate" || name == "Copy") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Duplicate) or @derive(Copy) decorator
static bool has_derive_duplicate(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Duplicate" || name == "Copy") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if a type is a primitive that can be copied directly
static bool is_primitive_copyable(const std::string& llvm_type) {
    // Integer types
    if (llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" ||
        llvm_type == "i32" || llvm_type == "i64" || llvm_type == "i128") {
        return true;
    }
    // Floating point types
    if (llvm_type == "float" || llvm_type == "double") {
        return true;
    }
    // Pointers
    if (llvm_type == "ptr") {
        return true;
    }
    return false;
}

// ============================================================================
// Duplicate Generation for Structs
// ============================================================================

/// Generate duplicate() method for a struct with @derive(Duplicate)
void LLVMIRGen::gen_derive_duplicate_struct(const parser::StructDecl& s) {
    if (!has_derive_duplicate(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_duplicate";

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

    // Emit function definition with direct struct return
    type_defs_buffer_ << "; @derive(Duplicate) for " << type_name << "\n";
    type_defs_buffer_ << "define " << llvm_type << " " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    if (fields.empty()) {
        // Empty struct - return undef (zero-initialized)
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

    // Copy each field
    for (const auto& field : fields) {
        std::string src_ptr = fresh_temp();
        std::string dst_ptr = fresh_temp();

        // Get source and destination pointers
        type_defs_buffer_ << "  " << src_ptr << " = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";
        type_defs_buffer_ << "  " << dst_ptr << " = getelementptr " << llvm_type
                          << ", ptr %ret, i32 0, i32 " << field.index << "\n";

        if (is_primitive_copyable(field.llvm_type)) {
            // Primitive type - direct copy
            std::string val = fresh_temp();
            type_defs_buffer_ << "  " << val << " = load " << field.llvm_type
                              << ", ptr " << src_ptr << "\n";
            type_defs_buffer_ << "  store " << field.llvm_type << " " << val
                              << ", ptr " << dst_ptr << "\n";
        } else {
            // Non-primitive type - call duplicate() on the field
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_dup_func = "@tml_" + suite_prefix + field_type_name + "_duplicate";

            // Call duplicate and store result
            std::string dup_result = fresh_temp();
            type_defs_buffer_ << "  " << dup_result << " = call " << field.llvm_type << " "
                              << field_dup_func << "(ptr " << src_ptr << ")\n";
            type_defs_buffer_ << "  store " << field.llvm_type << " " << dup_result
                              << ", ptr " << dst_ptr << "\n";
        }
    }

    // Load and return the result
    std::string result = fresh_temp();
    type_defs_buffer_ << "  " << result << " = load " << llvm_type << ", ptr %ret\n";
    type_defs_buffer_ << "  ret " << llvm_type << " " << result << "\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Duplicate Generation for Enums
// ============================================================================

/// Generate duplicate() method for an enum with @derive(Duplicate)
void LLVMIRGen::gen_derive_duplicate_enum(const parser::EnumDecl& e) {
    if (!has_derive_duplicate(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_duplicate";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // For now, do a simple load/return copy for enums
    // This works for Copy types but may need refinement for complex payloads
    type_defs_buffer_ << "; @derive(Duplicate) for " << type_name << "\n";
    type_defs_buffer_ << "define " << llvm_type << " " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    // Load and return the entire enum struct
    type_defs_buffer_ << "  %val = load " << llvm_type << ", ptr %this\n";
    type_defs_buffer_ << "  ret " << llvm_type << " %val\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
