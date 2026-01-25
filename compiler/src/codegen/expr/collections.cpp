//! # LLVM IR Generator - Collections and Paths
//!
//! This file implements array literals, indexing, and path expressions.
//!
//! ## Array Literals
//!
//! `[1, 2, 3]` creates a fixed-size array on the stack:
//! ```llvm
//! %arr = alloca [3 x i64]
//! ; store each element at its index
//! ```
//!
//! ## Array Indexing
//!
//! `arr[i]` generates GEP and load:
//! ```llvm
//! %ptr = getelementptr [N x T], ptr %arr, i64 0, i64 %i
//! %val = load T, ptr %ptr
//! ```
//!
//! ## Path Expressions
//!
//! Multi-segment paths like `Module::func` or `Enum::Variant` are
//! resolved and generate the appropriate call or enum constructor.

#include "codegen/llvm_ir_gen.hpp"
#include "common.hpp"

#include <iostream>

namespace tml::codegen {

auto LLVMIRGen::gen_array(const parser::ArrayExpr& arr) -> std::string {
    // Array literals create fixed-size arrays on the stack: [1, 2, 3] -> [3 x i64]
    // This matches Rust's array semantics where [T; N] is a fixed-size array

    if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
        // [elem1, elem2, elem3, ...]
        const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
        size_t count = elements.size();

        if (count == 0) {
            // Empty array - use expected type if available
            std::string elem_type =
                !expected_literal_type_.empty() ? expected_literal_type_ : "i64";
            last_expr_type_ = "[0 x " + elem_type + "]";
            return "zeroinitializer";
        }

        // Use expected_literal_type_ if set (from struct field context), otherwise infer from first
        // element
        std::string llvm_elem_type;
        if (!expected_literal_type_.empty()) {
            llvm_elem_type = expected_literal_type_;
        } else {
            types::TypePtr elem_type = infer_expr_type(*elements[0]);
            llvm_elem_type = llvm_type_from_semantic(elem_type, true);
        }

        // Generate array type [N x elem_type]
        std::string array_type = "[" + std::to_string(count) + " x " + llvm_elem_type + "]";

        // Allocate array on stack
        std::string arr_ptr = fresh_reg();
        emit_line("  " + arr_ptr + " = alloca " + array_type);

        // Store each element using GEP
        for (size_t i = 0; i < elements.size(); ++i) {
            std::string val = gen_expr(*elements[i]);
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_type + ", ptr " + arr_ptr +
                      ", i32 0, i32 " + std::to_string(i));
            emit_line("  store " + llvm_elem_type + " " + val + ", ptr " + elem_ptr);
        }

        // Load and return the entire array value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + array_type + ", ptr " + arr_ptr);

        // Track the array type for later use
        last_expr_type_ = array_type;

        return result;
    } else {
        // [expr; count] - repeat expression count times
        const auto& pair = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);

        // Get the count - must be a compile-time constant
        size_t count = 0;
        if (pair.second->is<parser::LiteralExpr>()) {
            const auto& lit = pair.second->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                const auto& val = lit.token.int_value();
                count = static_cast<size_t>(val.value);
            }
        }

        if (count == 0) {
            // Runtime count or zero - use expected type if available
            std::string elem_type =
                !expected_literal_type_.empty() ? expected_literal_type_ : "i64";
            last_expr_type_ = "[0 x " + elem_type + "]";
            return "zeroinitializer";
        }

        // Use expected_literal_type_ if set, otherwise infer from expression
        std::string llvm_elem_type;
        if (!expected_literal_type_.empty()) {
            llvm_elem_type = expected_literal_type_;
        } else {
            types::TypePtr elem_type = infer_expr_type(*pair.first);
            llvm_elem_type = llvm_type_from_semantic(elem_type, true);
        }
        std::string array_type = "[" + std::to_string(count) + " x " + llvm_elem_type + "]";

        // Generate initial value once
        std::string init_val = gen_expr(*pair.first);

        // Allocate array on stack
        std::string arr_ptr = fresh_reg();
        emit_line("  " + arr_ptr + " = alloca " + array_type);

        // Store the same value in each element
        for (size_t i = 0; i < count; ++i) {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_type + ", ptr " + arr_ptr +
                      ", i32 0, i32 " + std::to_string(i));
            emit_line("  store " + llvm_elem_type + " " + init_val + ", ptr " + elem_ptr);
        }

        // Load and return the entire array value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + array_type + ", ptr " + arr_ptr);

        last_expr_type_ = array_type;

        return result;
    }
}

auto LLVMIRGen::gen_index(const parser::IndexExpr& idx) -> std::string {
    // For fixed-size arrays: arr[i] -> GEP + load
    // For dynamic lists: arr[i] -> list_get(arr, i)

    // First, infer the type of the object to determine if it's an array or list
    types::TypePtr obj_type = infer_expr_type(*idx.object);

    // Check if it's a SliceType or RefType containing SliceType
    // Slices are fat pointers: { ptr, len } - we need to extract the ptr and index into it
    const types::SliceType* slice_type_ptr = nullptr;
    bool is_ref_slice = false;

    if (obj_type && obj_type->is<types::SliceType>()) {
        slice_type_ptr = &obj_type->as<types::SliceType>();
    } else if (obj_type && obj_type->is<types::RefType>()) {
        const auto& ref_type = obj_type->as<types::RefType>();
        if (ref_type.inner && ref_type.inner->is<types::SliceType>()) {
            slice_type_ptr = &ref_type.inner->as<types::SliceType>();
            is_ref_slice = true;
        }
    }

    if (slice_type_ptr) {
        std::string elem_llvm_type = llvm_type_from_semantic(slice_type_ptr->element, true);

        // Generate the slice value (this gives us the fat pointer struct or a ptr to it)
        std::string slice_val = gen_expr(*idx.object);

        // For ref [T], slice_val is a ptr to the slice struct
        // For [T], slice_val is the slice struct value itself
        std::string slice_ptr;
        if (is_ref_slice) {
            // slice_val is already a ptr to { ptr, i64 }
            slice_ptr = slice_val;
        } else {
            // Need to store the slice struct to get a ptr
            slice_ptr = fresh_reg();
            emit_line("  " + slice_ptr + " = alloca { ptr, i64 }");
            emit_line("  store { ptr, i64 } " + slice_val + ", ptr " + slice_ptr);
        }

        // Extract the data pointer from the slice struct (field 0)
        std::string data_ptr_ptr = fresh_reg();
        emit_line("  " + data_ptr_ptr + " = getelementptr { ptr, i64 }, ptr " + slice_ptr +
                  ", i32 0, i32 0");
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = load ptr, ptr " + data_ptr_ptr);

        // Generate the index
        std::string index_val = gen_expr(*idx.index);
        std::string index_type = last_expr_type_;

        // Convert index to i64 for GEP if needed
        std::string index_i64 = index_val;
        if (index_type == "i32") {
            index_i64 = fresh_reg();
            emit_line("  " + index_i64 + " = sext i32 " + index_val + " to i64");
        }

        // GEP to get element pointer (using element array, not fixed array)
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr " + elem_llvm_type + ", ptr " + data_ptr +
                  ", i64 " + index_i64);

        // Load and return the element
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + elem_llvm_type + ", ptr " + elem_ptr);

        last_expr_type_ = elem_llvm_type;
        return result;
    }

    // Check if it's an ArrayType
    if (obj_type && obj_type->is<types::ArrayType>()) {
        const auto& arr_type = obj_type->as<types::ArrayType>();
        std::string elem_llvm_type = llvm_type_from_semantic(arr_type.element, true);
        std::string array_llvm_type =
            "[" + std::to_string(arr_type.size) + " x " + elem_llvm_type + "]";

        // Try to get a direct pointer to the array if it's a local variable
        // This is critical for array modification to work correctly
        std::string arr_ptr;
        if (idx.object->is<parser::IdentExpr>()) {
            const auto& ident = idx.object->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                // Use the local's register directly (it's already a pointer)
                arr_ptr = it->second.reg;
            }
        }

        if (arr_ptr.empty()) {
            // Fallback: Generate the array value and store it to a temp
            std::string arr_val = gen_expr(*idx.object);
            arr_ptr = fresh_reg();
            emit_line("  " + arr_ptr + " = alloca " + array_llvm_type);
            emit_line("  store " + array_llvm_type + " " + arr_val + ", ptr " + arr_ptr);
        }

        // Generate the index
        std::string index_val = gen_expr(*idx.index);
        std::string index_type = last_expr_type_;

        // Convert index to i64 if needed for GEP consistency
        std::string index_i64 = index_val;
        if (index_type != "i64") {
            index_i64 = fresh_reg();
            if (index_type == "i32") {
                emit_line("  " + index_i64 + " = sext i32 " + index_val + " to i64");
            } else if (index_type == "i8" || index_type == "i16") {
                emit_line("  " + index_i64 + " = sext " + index_type + " " + index_val + " to i64");
            } else {
                // Assume unsigned, use zext
                emit_line("  " + index_i64 + " = zext " + index_type + " " + index_val + " to i64");
            }
        }

        // GEP to get element pointer (use i64 for index to match 64-bit systems)
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr " + array_llvm_type + ", ptr " + arr_ptr +
                  ", i64 0, i64 " + index_i64);

        // Load and return the element
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + elem_llvm_type + ", ptr " + elem_ptr);

        last_expr_type_ = elem_llvm_type;
        return result;
    }

    // Check if the last expression type indicates an array (from gen_array)
    if (last_expr_type_.starts_with("[") && last_expr_type_.find(" x ") != std::string::npos) {
        // Parse array type: [N x type]
        size_t x_pos = last_expr_type_.find(" x ");
        size_t end_bracket = last_expr_type_.rfind("]");
        if (x_pos != std::string::npos && end_bracket != std::string::npos) {
            std::string elem_type = last_expr_type_.substr(x_pos + 3, end_bracket - (x_pos + 3));
            std::string array_type = last_expr_type_;

            // Generate the array value and store it
            std::string arr_val = gen_expr(*idx.object);
            std::string arr_ptr = fresh_reg();
            emit_line("  " + arr_ptr + " = alloca " + array_type);
            emit_line("  store " + array_type + " " + arr_val + ", ptr " + arr_ptr);

            // Generate the index
            std::string index_val = gen_expr(*idx.index);
            std::string idx_type = last_expr_type_;

            // Convert index to i64 if needed for GEP consistency
            std::string index_i64 = index_val;
            if (idx_type != "i64") {
                index_i64 = fresh_reg();
                if (idx_type == "i32") {
                    emit_line("  " + index_i64 + " = sext i32 " + index_val + " to i64");
                } else {
                    emit_line("  " + index_i64 + " = zext " + idx_type + " " + index_val +
                              " to i64");
                }
            }

            // GEP to get element pointer (use i64 for index)
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_type + ", ptr " + arr_ptr +
                      ", i64 0, i64 " + index_i64);

            // Load and return the element
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_type + ", ptr " + elem_ptr);

            last_expr_type_ = elem_type;
            return result;
        }
    }

    // Fall back to list_get for dynamic lists
    std::string arr_ptr = gen_expr(*idx.object);
    std::string index_val = gen_expr(*idx.index);
    std::string index_type = last_expr_type_;

    // Convert index to i64 if needed (list_get expects i64)
    std::string index_i64 = index_val;
    if (index_type == "i32") {
        index_i64 = fresh_reg();
        emit_line("  " + index_i64 + " = sext i32 " + index_val + " to i64");
    }

    std::string result = fresh_reg();
    emit_line("  " + result + " = call i64 @list_get(ptr " + arr_ptr + ", i64 " + index_i64 + ")");

    last_expr_type_ = "i64";
    return result;
}

auto LLVMIRGen::gen_path(const parser::PathExpr& path) -> std::string {
    // Path expressions like Color::Red resolve to enum variant values
    // Join path segments with ::
    std::string full_path;
    for (size_t i = 0; i < path.path.segments.size(); ++i) {
        if (i > 0)
            full_path += "::";
        full_path += path.path.segments[i];
    }

    // Check for global constants (like I32::MIN, I32::MAX)
    // First check local global_constants_
    auto const_it = global_constants_.find(full_path);
    std::string const_value;
    if (const_it != global_constants_.end()) {
        const_value = const_it->second;
    } else if (env_.module_registry()) {
        // Search all imported modules for the constant
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto mod_const_it = mod.constants.find(full_path);
            if (mod_const_it != mod.constants.end()) {
                const_value = mod_const_it->second;
                break;
            }
        }
    }

    if (!const_value.empty()) {
        // Determine the type based on the prefix (I32::, U64::, etc.)
        if (path.path.segments.size() >= 1) {
            std::string type_name = path.path.segments[0];
            if (type_name == "I8") {
                last_expr_type_ = "i8";
            } else if (type_name == "I16") {
                last_expr_type_ = "i16";
            } else if (type_name == "I32") {
                last_expr_type_ = "i32";
            } else if (type_name == "I64") {
                last_expr_type_ = "i64";
            } else if (type_name == "U8") {
                last_expr_type_ = "i8";
            } else if (type_name == "U16") {
                last_expr_type_ = "i16";
            } else if (type_name == "U32") {
                last_expr_type_ = "i32";
            } else if (type_name == "U64") {
                last_expr_type_ = "i64";
            } else {
                last_expr_type_ = "i64"; // Default to i64
            }
        } else {
            last_expr_type_ = "i64";
        }
        return const_value;
    }

    // Check for class static field access (e.g., Counter::count)
    if (path.path.segments.size() == 2) {
        std::string class_name = path.path.segments[0];
        std::string field_name = path.path.segments[1];
        auto class_def = env_.lookup_class(class_name);
        if (class_def.has_value()) {
            for (const auto& field : class_def->fields) {
                if (field.name == field_name && field.is_static) {
                    // Load from global variable @class.ClassName.fieldname
                    std::string global_name = "@class." + class_name + "." + field_name;
                    std::string llvm_type = llvm_type_from_semantic(field.type);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + llvm_type + ", ptr " + global_name);
                    last_expr_type_ = llvm_type;
                    return result;
                }
            }
        }
    }

    // Look up in enum variants
    auto it = enum_variants_.find(full_path);

    // If not found directly, try resolving the type name through module registry
    // This handles enums from imported modules (e.g., Alignment::Left from core::fmt::traits)
    // when the codegen env doesn't have the import (since env is from test file, not from
    // the module currently being generated)
    if (it == enum_variants_.end() && path.path.segments.size() == 2) {
        std::string type_name = path.path.segments[0];
        std::string variant_name = path.path.segments[1];

        // First try env_.lookup_enum which handles local and imported enums
        auto enum_def = env_.lookup_enum(type_name);

        // If not found, search all modules in the registry
        if (!enum_def.has_value() && env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                auto mod_enum_it = mod.enums.find(type_name);
                if (mod_enum_it != mod.enums.end()) {
                    enum_def = mod_enum_it->second;
                    break;
                }
            }
        }

        if (enum_def.has_value() && !enum_def->variants.empty()) {
            // Check if this is a generic enum that needs instantiation
            if (!enum_def->type_params.empty()) {
                // Generic enum - determine the correct instantiation
                std::string mangled_name;

                // Check expected_enum_type_ first (set by assignment target type)
                if (!expected_enum_type_.empty() &&
                    expected_enum_type_.find("%struct." + type_name + "__") != std::string::npos) {
                    // Extract mangled name from expected type
                    mangled_name = expected_enum_type_.substr(8); // Remove "%struct."
                }
                // Check current_type_subs_ (e.g., inside generic impl method)
                else if (!current_type_subs_.empty()) {
                    std::vector<types::TypePtr> type_args;
                    bool all_resolved = true;
                    for (const auto& param : enum_def->type_params) {
                        auto sub_it = current_type_subs_.find(param);
                        if (sub_it != current_type_subs_.end() && sub_it->second) {
                            type_args.push_back(sub_it->second);
                        } else {
                            all_resolved = false;
                            break;
                        }
                    }
                    if (all_resolved && !type_args.empty()) {
                        mangled_name = require_enum_instantiation(type_name, type_args);
                    }
                }

                if (!mangled_name.empty()) {
                    // Use the instantiated enum type
                    // Find variant index
                    int variant_idx = -1;
                    for (size_t i = 0; i < enum_def->variants.size(); ++i) {
                        if (enum_def->variants[i].first == variant_name) {
                            variant_idx = static_cast<int>(i);
                            break;
                        }
                    }

                    if (variant_idx >= 0) {
                        std::string struct_type = "%struct." + mangled_name;

                        // Allocate and initialize the generic enum
                        std::string alloca_reg = fresh_reg();
                        emit_line("  " + alloca_reg + " = alloca " + struct_type + ", align 8");

                        // Set the tag
                        std::string tag_ptr = fresh_reg();
                        emit_line("  " + tag_ptr + " = getelementptr inbounds " + struct_type +
                                  ", ptr " + alloca_reg + ", i32 0, i32 0");
                        emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " +
                                  tag_ptr);

                        // Load the value
                        std::string result = fresh_reg();
                        emit_line("  " + result + " = load " + struct_type + ", ptr " + alloca_reg);
                        last_expr_type_ = struct_type;
                        return result;
                    }
                }
            }

            // Non-generic enum: register variants if not already done
            std::string first_key = type_name + "::" + enum_def->variants[0].first;
            if (enum_variants_.find(first_key) == enum_variants_.end()) {
                int tag = 0;
                for (const auto& variant : enum_def->variants) {
                    std::string key = type_name + "::" + variant.first;
                    enum_variants_[key] = tag++;
                }
                // Also register the struct type if not done
                if (struct_types_.find(type_name) == struct_types_.end()) {
                    std::string struct_type_name = "%struct." + type_name;
                    // Use type_defs_buffer_ for type definitions, not emit_line()
                    // emit_line() would put the definition inside the current function
                    type_defs_buffer_ << struct_type_name << " = type { i32 }\n";
                    struct_types_[type_name] = struct_type_name;
                }
            }
            // Try lookup again
            it = enum_variants_.find(full_path);
        }
    }

    if (it != enum_variants_.end()) {
        // For enum variants, we need to create a struct { i32 } value
        // Extract the enum type name (first segment)
        std::string enum_name = path.path.segments[0];
        std::string struct_type = "%struct." + enum_name;

        // Allocate the enum struct on stack
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + struct_type);

        // Get pointer to the tag field (GEP with indices 0, 0)
        std::string tag_ptr = fresh_reg();
        emit_line("  " + tag_ptr + " = getelementptr " + struct_type + ", ptr " + alloca_reg +
                  ", i32 0, i32 0");

        // Store the tag value
        emit_line("  store i32 " + std::to_string(it->second) + ", ptr " + tag_ptr);

        // Load the entire struct value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + struct_type + ", ptr " + alloca_reg);

        // Mark last expr type
        last_expr_type_ = struct_type;

        return result;
    }

    // Not found - might be a function or module path
    report_error("Unknown path: " + full_path, path.span);
    return "0";
}

} // namespace tml::codegen
