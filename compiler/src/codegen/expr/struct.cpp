//! # LLVM IR Generator - Struct Expressions
//!
//! This file implements struct construction and field access.
//!
//! ## Struct Construction
//!
//! `Point { x: 10, y: 20 }` generates:
//! ```llvm
//! %ptr = alloca %struct.Point
//! %field0 = getelementptr %struct.Point, ptr %ptr, i32 0, i32 0
//! store i32 10, ptr %field0
//! %field1 = getelementptr %struct.Point, ptr %ptr, i32 0, i32 1
//! store i32 20, ptr %field1
//! ```
//!
//! ## Field Access
//!
//! `point.x` generates a GEP and load for the field at its index.
//!
//! ## Key Functions
//!
//! | Function            | Purpose                           |
//! |---------------------|-----------------------------------|
//! | `gen_struct_expr`   | Construct struct value            |
//! | `gen_struct_expr_ptr`| Return pointer to struct         |
//! | `gen_field`         | Access field                      |
//! | `get_field_index`   | Look up field position            |

#include "codegen/llvm_ir_gen.hpp"

#include <cctype>
#include <iostream>

namespace tml::codegen {

// Generate struct expression, returning pointer to allocated struct
auto LLVMIRGen::gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string {
    std::string base_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type;

    // Handle Self type - resolve to current_impl_type_ if set
    // This handles cases like `Self { ptr: addr }` inside impl blocks
    if (base_name == "Self" && !current_impl_type_.empty()) {
        struct_type = "%struct." + current_impl_type_;
        // Allocate and initialize struct
        std::string ptr = fresh_reg();
        emit_line("  " + ptr + " = alloca " + struct_type);

        // Get struct name for field lookup
        std::string struct_name_for_lookup = current_impl_type_;

        for (size_t i = 0; i < s.fields.size(); ++i) {
            const std::string& field_name = s.fields[i].first;
            int field_idx = get_field_index(struct_name_for_lookup, field_name);
            std::string field_val = gen_expr(*s.fields[i].second);
            std::string field_type = get_field_type(struct_name_for_lookup, field_name);
            if (field_type.empty()) {
                field_type = last_expr_type_;
            }

            std::string field_ptr = fresh_reg();
            emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + ptr +
                      ", i32 0, i32 " + std::to_string(field_idx));
            emit_line("  store " + field_type + " " + field_val + ", ptr " + field_ptr);
        }

        last_expr_type_ = struct_type;
        return ptr;
    }

    // Check if the return type context tells us what type to use
    // This handles cases like `return RawPtr { addr: x }` where return type is RawPtr[U8]
    std::string ret_type_prefix = "%struct." + base_name + "__";
    if (!current_ret_type_.empty() && current_ret_type_.starts_with(ret_type_prefix)) {
        struct_type = current_ret_type_;
        // Allocate and initialize struct
        std::string ptr = fresh_reg();
        emit_line("  " + ptr + " = alloca " + struct_type);

        // Get struct name for field lookup (remove %struct. prefix)
        std::string struct_name_for_lookup = current_ret_type_.substr(8);

        for (size_t i = 0; i < s.fields.size(); ++i) {
            const std::string& field_name = s.fields[i].first;
            int field_idx = get_field_index(struct_name_for_lookup, field_name);
            std::string field_val = gen_expr(*s.fields[i].second);
            std::string field_type = get_field_type(struct_name_for_lookup, field_name);
            if (field_type.empty()) {
                field_type = last_expr_type_;
            }

            std::string field_ptr = fresh_reg();
            emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + ptr +
                      ", i32 0, i32 " + std::to_string(field_idx));
            emit_line("  store " + field_type + " " + field_val + ", ptr " + field_ptr);
        }

        last_expr_type_ = struct_type;
        return ptr;
    }

    // Check if this is a generic struct
    auto generic_it = pending_generic_structs_.find(base_name);
    if (generic_it != pending_generic_structs_.end() && !s.fields.empty()) {
        // This is a generic struct - infer type arguments from field values
        const parser::StructDecl* decl = generic_it->second;

        // Build substitution map by matching field types
        std::vector<types::TypePtr> type_args;
        std::unordered_map<std::string, types::TypePtr> inferred_generics;

        for (const auto& generic_param : decl->generics) {
            inferred_generics[generic_param.name] = nullptr;
        }

        // Match fields to infer generic types
        for (size_t fi = 0; fi < s.fields.size() && fi < decl->fields.size(); ++fi) {
            const auto& field_decl = decl->fields[fi];
            // Check if field type is a generic parameter
            if (field_decl.type && field_decl.type->is<parser::NamedType>()) {
                const auto& ftype = field_decl.type->as<parser::NamedType>();
                std::string type_name =
                    ftype.path.segments.empty() ? "" : ftype.path.segments.back();
                auto gen_it = inferred_generics.find(type_name);
                if (gen_it != inferred_generics.end() && !gen_it->second) {
                    // This field's type is a generic parameter - infer from value
                    gen_it->second = infer_expr_type(*s.fields[fi].second);
                }
            }
        }

        // Build type_args in order
        for (const auto& generic_param : decl->generics) {
            auto inferred = inferred_generics[generic_param.name];
            type_args.push_back(inferred ? inferred : types::make_i32());
        }

        // Get mangled name and ensure instantiation
        std::string mangled = require_struct_instantiation(base_name, type_args);
        struct_type = "%struct." + mangled;
    } else {
        // Check if it's a class type
        auto class_def = env_.lookup_class(base_name);
        if (class_def.has_value()) {
            struct_type = "%class." + base_name;
        } else {
            // Non-generic struct - ensure type is defined (handles imported structs)
            // Use llvm_type_from_semantic to trigger type emission if needed
            auto sem_type = std::make_shared<types::Type>(types::NamedType{base_name, "", {}});
            struct_type = llvm_type_from_semantic(sem_type, true);
        }
    }

    // Allocate struct - stack for structs and eligible classes, heap otherwise
    std::string ptr = fresh_reg();
    bool is_class = struct_type.starts_with("%class.");

    // Check if this class is a value class candidate (sealed, no virtual methods)
    // Value classes are returned by value and can be stack allocated safely.
    // Non-value classes must be heap allocated since they're returned by pointer.
    bool is_value_class = false;
    if (is_class) {
        is_value_class = env_.is_value_class_candidate(base_name);
    }

    if (is_class && !is_value_class) {
        // Heap allocate for non-value classes
        // Non-value classes are returned by pointer, so stack allocation would create
        // dangling pointers when returned from factory methods.
        // Calculate actual size of the class struct using LLVM GEP trick:
        // Get the address offset from element 0 to element 1, which equals the struct size
        std::string size_ptr = fresh_reg();
        std::string size_reg = fresh_reg();
        emit_line("  " + size_ptr + " = getelementptr " + struct_type + ", ptr null, i32 1");
        emit_line("  " + size_reg + " = ptrtoint ptr " + size_ptr + " to i64");
        emit_line("  " + ptr + " = call ptr @malloc(i64 " + size_reg + ")");

        // Initialize vtable pointer (field 0) for class instances
        std::string vtable_ptr = fresh_reg();
        emit_line("  " + vtable_ptr + " = getelementptr " + struct_type + ", ptr " + ptr +
                  ", i32 0, i32 0");
        emit_line("  store ptr @vtable." + base_name + ", ptr " + vtable_ptr);
    } else {
        // Stack allocate for structs and value classes (no vtable needed)
        emit_line("  " + ptr + " = alloca " + struct_type);
    }

    // Initialize fields - look up field index by name, not expression order
    // Get the struct name for field index lookup
    std::string struct_name_for_lookup = struct_type;
    if (struct_name_for_lookup.starts_with("%struct.")) {
        struct_name_for_lookup = struct_name_for_lookup.substr(8);
    } else if (struct_name_for_lookup.starts_with("%class.")) {
        struct_name_for_lookup = struct_name_for_lookup.substr(7);
    }

    for (size_t i = 0; i < s.fields.size(); ++i) {
        const std::string& field_name = s.fields[i].first;
        std::string field_val;
        std::string field_type = "i32";

        // Look up field index by name
        int field_idx = get_field_index(struct_name_for_lookup, field_name);

        // Check if field value is a nested struct
        if (s.fields[i].second->is<parser::StructExpr>()) {
            // Nested struct - allocate and copy
            const auto& nested = s.fields[i].second->as<parser::StructExpr>();
            std::string nested_ptr = gen_struct_expr_ptr(nested);

            // Need to determine nested struct type (may also be generic)
            std::string nested_base = nested.path.segments.back();
            auto nested_generic_it = pending_generic_structs_.find(nested_base);
            if (nested_generic_it != pending_generic_structs_.end()) {
                // Generic nested struct - infer its type
                const parser::StructDecl* nested_decl = nested_generic_it->second;
                std::vector<types::TypePtr> nested_type_args;
                std::unordered_map<std::string, types::TypePtr> nested_inferred;

                for (const auto& gp : nested_decl->generics) {
                    nested_inferred[gp.name] = nullptr;
                }
                for (size_t ni = 0; ni < nested.fields.size() && ni < nested_decl->fields.size();
                     ++ni) {
                    const auto& nf = nested_decl->fields[ni];
                    if (nf.type && nf.type->is<parser::NamedType>()) {
                        const auto& nft = nf.type->as<parser::NamedType>();
                        std::string nft_name =
                            nft.path.segments.empty() ? "" : nft.path.segments.back();
                        auto ngen_it = nested_inferred.find(nft_name);
                        if (ngen_it != nested_inferred.end() && !ngen_it->second) {
                            ngen_it->second = infer_expr_type(*nested.fields[ni].second);
                        }
                    }
                }
                for (const auto& gp : nested_decl->generics) {
                    auto inf = nested_inferred[gp.name];
                    nested_type_args.push_back(inf ? inf : types::make_i32());
                }
                std::string nested_mangled =
                    require_struct_instantiation(nested_base, nested_type_args);
                field_type = "%struct." + nested_mangled;
            } else {
                field_type = "%struct." + nested_base;
            }

            std::string nested_val = fresh_reg();
            emit_line("  " + nested_val + " = load " + field_type + ", ptr " + nested_ptr);
            field_val = nested_val;
        } else {
            // Get the actual field type from the struct definition
            std::string target_field_type = get_field_type(struct_name_for_lookup, field_name);

            // Set expected type context for enum variants like Nothing
            // This allows proper type inference for generic enums
            if (target_field_type.find("%struct.Maybe__") == 0 ||
                target_field_type.find("%struct.Outcome__") == 0) {
                expected_enum_type_ = target_field_type;
            }

            // Set expected type for integer literals based on field type
            // This allows "Point { x: 10, y: 20 }" without "10 as U8"
            // Note: float/double not included - LLVM literals are always double and need fptrunc
            if (target_field_type == "i8") {
                expected_literal_type_ = "i8";
                expected_literal_is_unsigned_ = false;
            } else if (target_field_type == "i16") {
                expected_literal_type_ = "i16";
                expected_literal_is_unsigned_ = false;
            } else if (target_field_type == "i64") {
                expected_literal_type_ = "i64";
                expected_literal_is_unsigned_ = false;
            }
            // Handle array types like "[4 x i8]" - extract element type for coercion
            else if (target_field_type.starts_with("[") &&
                     target_field_type.find(" x ") != std::string::npos) {
                // Parse "[N x elem_type]" to extract elem_type
                size_t x_pos = target_field_type.find(" x ");
                if (x_pos != std::string::npos) {
                    std::string elem_type = target_field_type.substr(x_pos + 3);
                    // Remove trailing "]"
                    if (!elem_type.empty() && elem_type.back() == ']') {
                        elem_type.pop_back();
                    }
                    // Set expected literal type for array elements
                    if (elem_type == "i8" || elem_type == "i16" || elem_type == "i32" ||
                        elem_type == "i64") {
                        expected_literal_type_ = elem_type;
                        expected_literal_is_unsigned_ = false;
                    }
                }
            }

            field_val = gen_expr(*s.fields[i].second);
            std::string actual_llvm_type =
                last_expr_type_;         // Capture actual LLVM type from gen_expr
            expected_enum_type_.clear(); // Clear after expression
            expected_literal_type_.clear();
            expected_literal_is_unsigned_ = false;

            // If the expression is a pointer (like 'this') but the field expects a struct value,
            // we need to load the struct value from the pointer
            if (actual_llvm_type == "ptr" && target_field_type.starts_with("%struct.")) {
                std::string loaded = fresh_reg();
                emit_line("  " + loaded + " = load " + target_field_type + ", ptr " + field_val);
                field_val = loaded;
                field_type = target_field_type;
            }
            // If target field type is different from actual expression type, cast as needed
            // Use actual_llvm_type (from gen_expr) not inferred type, since expected_literal_type_
            // may have been used to generate the literal with the correct type already
            else if (target_field_type != actual_llvm_type && target_field_type != "i32") {
                // Cast integer types to the correct field type
                if ((actual_llvm_type == "i32" || actual_llvm_type == "i64") &&
                    (target_field_type == "i64" || target_field_type == "i32")) {
                    if (actual_llvm_type == "i32" && target_field_type == "i64") {
                        // Sign extend i32 to i64
                        std::string casted = fresh_reg();
                        emit_line("  " + casted + " = sext i32 " + field_val + " to i64");
                        field_val = casted;
                    } else if (actual_llvm_type == "i64" && target_field_type == "i32") {
                        // Truncate i64 to i32
                        std::string casted = fresh_reg();
                        emit_line("  " + casted + " = trunc i64 " + field_val + " to i32");
                        field_val = casted;
                    }
                }
                // Handle float/double conversions
                // LLVM float literals are always double, so truncate to float if needed
                else if (actual_llvm_type == "double" && target_field_type == "float") {
                    std::string casted = fresh_reg();
                    emit_line("  " + casted + " = fptrunc double " + field_val + " to float");
                    field_val = casted;
                } else if (actual_llvm_type == "float" && target_field_type == "double") {
                    std::string casted = fresh_reg();
                    emit_line("  " + casted + " = fpext float " + field_val + " to double");
                    field_val = casted;
                }
                field_type = target_field_type;
            } else {
                field_type = actual_llvm_type;
            }
        }

        std::string field_ptr;

        // Check if this is an inherited field that needs chained GEP
        bool is_inherited = false;
        if (is_class) {
            auto field_info = get_class_field_info(struct_name_for_lookup, field_name);
            if (field_info && field_info->is_inherited && !field_info->inheritance_path.empty()) {
                is_inherited = true;

                // Generate chained GEPs through inheritance path
                std::string current_ptr = ptr;
                std::string current_type = struct_type;

                for (size_t step_idx = 0; step_idx < field_info->inheritance_path.size();
                     ++step_idx) {
                    const auto& step = field_info->inheritance_path[step_idx];
                    std::string next_ptr = fresh_reg();
                    emit_line("  " + next_ptr + " = getelementptr " + current_type + ", ptr " +
                              current_ptr + ", i32 0, i32 " + std::to_string(step.index));
                    current_ptr = next_ptr;
                    current_type = "%class." + step.class_name;
                }
                field_ptr = current_ptr;
            }
        }

        if (!is_inherited) {
            // Direct field access
            field_ptr = fresh_reg();
            emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + ptr +
                      ", i32 0, i32 " + std::to_string(field_idx));
        }

        emit_line("  store " + field_type + " " + field_val + ", ptr " + field_ptr);
    }

    return ptr;
}

auto LLVMIRGen::gen_struct_expr(const parser::StructExpr& s) -> std::string {
    std::string ptr = gen_struct_expr_ptr(s);
    std::string base_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type;

    // Handle Self type - resolve to current_impl_type_
    if (base_name == "Self" && !current_impl_type_.empty()) {
        struct_type = "%struct." + current_impl_type_;
    } else {
        // Check if return type context tells us what type to use
        std::string ret_type_prefix = "%struct." + base_name + "__";
        if (!current_ret_type_.empty() && current_ret_type_.starts_with(ret_type_prefix)) {
            struct_type = current_ret_type_;
        } else {
            // Check if this is a generic struct - same logic as gen_struct_expr_ptr
            auto generic_it = pending_generic_structs_.find(base_name);
            if (generic_it != pending_generic_structs_.end() && !s.fields.empty()) {
                const parser::StructDecl* decl = generic_it->second;
                std::vector<types::TypePtr> type_args;
                std::unordered_map<std::string, types::TypePtr> inferred_generics;

                for (const auto& generic_param : decl->generics) {
                    inferred_generics[generic_param.name] = nullptr;
                }
                for (size_t fi = 0; fi < s.fields.size() && fi < decl->fields.size(); ++fi) {
                    const auto& field_decl = decl->fields[fi];
                    if (field_decl.type && field_decl.type->is<parser::NamedType>()) {
                        const auto& ftype = field_decl.type->as<parser::NamedType>();
                        std::string type_name =
                            ftype.path.segments.empty() ? "" : ftype.path.segments.back();
                        auto gen_it = inferred_generics.find(type_name);
                        if (gen_it != inferred_generics.end() && !gen_it->second) {
                            gen_it->second = infer_expr_type(*s.fields[fi].second);
                        }
                    }
                }
                for (const auto& generic_param : decl->generics) {
                    auto inferred = inferred_generics[generic_param.name];
                    type_args.push_back(inferred ? inferred : types::make_i32());
                }
                std::string mangled = require_struct_instantiation(base_name, type_args);
                struct_type = "%struct." + mangled;
            } else {
                // Check if it's a class type
                auto class_def = env_.lookup_class(base_name);
                if (class_def.has_value()) {
                    struct_type = "%class." + base_name;
                } else {
                    struct_type = "%struct." + base_name;
                }
            }
        }
    }

    // For classes, return the pointer directly (reference type)
    // For structs, load and return the value
    bool is_class = struct_type.starts_with("%class.");
    if (is_class) {
        last_expr_type_ = "ptr";
        return ptr;
    }

    // Load the struct value
    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + struct_type + ", ptr " + ptr);

    // Set last_expr_type_ for proper type tracking (e.g., for enum payloads)
    last_expr_type_ = struct_type;

    return result;
}

// Helper to get field index for struct types - uses dynamic registry
auto LLVMIRGen::get_field_index(const std::string& struct_name, const std::string& field_name)
    -> int {
    // First check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return field.index;
            }
        }
    }

    // Check class_fields_ registry for class types
    auto class_it = class_fields_.find(struct_name);
    if (class_it != class_fields_.end()) {
        for (const auto& field : class_it->second) {
            if (field.name == field_name) {
                return field.index;
            }
        }
    }

    // Fallback for hardcoded types (legacy support)
    if (struct_name == "Point") {
        if (field_name == "x")
            return 0;
        if (field_name == "y")
            return 1;
    }
    if (struct_name == "Rectangle") {
        if (field_name == "origin")
            return 0;
        if (field_name == "width")
            return 1;
        if (field_name == "height")
            return 2;
    }
    return 0;
}

// Helper to get field type for struct types - uses dynamic registry
auto LLVMIRGen::get_field_type(const std::string& struct_name, const std::string& field_name)
    -> std::string {
    // First check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return field.llvm_type;
            }
        }
    }

    // Check class_fields_ registry for class types
    auto class_it = class_fields_.find(struct_name);
    if (class_it != class_fields_.end()) {
        for (const auto& field : class_it->second) {
            if (field.name == field_name) {
                return field.llvm_type;
            }
        }
    }

    // Fallback for hardcoded types (legacy support)
    if (struct_name == "Rectangle" && field_name == "origin") {
        return "%struct.Point";
    }
    return "i32";
}

// Helper to get full class field info (including inheritance details)
auto LLVMIRGen::get_class_field_info(const std::string& class_name, const std::string& field_name)
    -> std::optional<ClassFieldInfo> {
    auto class_it = class_fields_.find(class_name);
    if (class_it != class_fields_.end()) {
        for (const auto& field : class_it->second) {
            if (field.name == field_name) {
                return field;
            }
        }
    }
    return std::nullopt;
}

auto LLVMIRGen::gen_field(const parser::FieldExpr& field) -> std::string {
    // Handle static field access (ClassName.field)
    if (field.object->is<parser::IdentExpr>()) {
        const auto& ident = field.object->as<parser::IdentExpr>();

        // Check if it's a class name for static field access
        std::string static_key = ident.name + "." + field.field;
        auto static_it = static_fields_.find(static_key);
        if (static_it != static_fields_.end()) {
            // Load from global static field
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + static_it->second.type + ", ptr " +
                      static_it->second.global_name);
            last_expr_type_ = static_it->second.type;
            return result;
        }
    }

    // Handle field access on struct
    std::string struct_type;
    std::string struct_ptr;

    // If the object is an identifier, look up its type
    if (field.object->is<parser::IdentExpr>()) {
        const auto& ident = field.object->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            struct_type = it->second.type;
            struct_ptr = it->second.reg;

            // Special handling for 'this' in impl methods
            if (ident.name == "this" && !current_impl_type_.empty()) {
                // 'this' is a pointer to the impl type
                struct_type = "%struct." + current_impl_type_;
                // 'this' is already a pointer parameter, not an alloca - use it directly
                // struct_ptr is already "%this" which is the direct pointer
            }
        }
    } else if (field.object->is<parser::FieldExpr>()) {
        // Chained field access (e.g., rect.origin.x)
        // First get the nested field pointer
        const auto& nested_field = field.object->as<parser::FieldExpr>();

        // Get the outermost struct
        if (nested_field.object->is<parser::IdentExpr>()) {
            const auto& ident = nested_field.object->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                std::string outer_type = it->second.type;
                std::string outer_ptr = it->second.reg;

                // Special handling for 'this' in impl methods
                if (ident.name == "this" && !current_impl_type_.empty()) {
                    outer_type = "%struct." + current_impl_type_;
                    // 'this' is already a direct pointer parameter
                }

                // Get outer struct type name
                std::string outer_name = outer_type;
                if (outer_name.starts_with("%struct.")) {
                    outer_name = outer_name.substr(8);
                }

                // Get field index for nested field
                int nested_idx = get_field_index(outer_name, nested_field.field);
                std::string nested_type = get_field_type(outer_name, nested_field.field);

                // Get pointer to nested field
                std::string nested_ptr = fresh_reg();
                emit_line("  " + nested_ptr + " = getelementptr " + outer_type + ", ptr " +
                          outer_ptr + ", i32 0, i32 " + std::to_string(nested_idx));

                struct_type = nested_type;
                struct_ptr = nested_ptr;
            }
        }
    }

    if (struct_type.empty() || struct_ptr.empty()) {
        report_error("Cannot resolve field access object", field.span);
        return "0";
    }

    // If struct_type is ptr, infer the actual struct type from the expression
    if (struct_type == "ptr") {
        types::TypePtr semantic_type = infer_expr_type(*field.object);
        if (semantic_type) {
            // If the semantic type is a reference or pointer, get the inner type
            // and load the pointer from the alloca first
            if (semantic_type->is<types::RefType>()) {
                const auto& ref = semantic_type->as<types::RefType>();
                struct_type = llvm_type_from_semantic(ref.inner);
                // struct_ptr points to an alloca containing a pointer to the struct
                // We need to load the pointer first
                std::string loaded_ptr = fresh_reg();
                emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                struct_ptr = loaded_ptr;
            } else if (semantic_type->is<types::PtrType>()) {
                const auto& ptr = semantic_type->as<types::PtrType>();
                struct_type = llvm_type_from_semantic(ptr.inner);
                // Same - load the pointer from the alloca
                std::string loaded_ptr = fresh_reg();
                emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                struct_ptr = loaded_ptr;
            } else if (semantic_type->is<types::ClassType>()) {
                // Class types are heap-allocated pointers
                // Use %class.ClassName as the struct type
                const auto& cls = semantic_type->as<types::ClassType>();
                struct_type = "%class." + cls.name;
                // For local variables, the alloca stores a pointer to the class instance
                // We need to load the pointer first (unless it's a direct parameter)
                if (field.object->is<parser::IdentExpr>()) {
                    const auto& ident = field.object->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    bool is_direct_param = (it != locals_.end() && it->second.is_direct_param);
                    if (!is_direct_param) {
                        // Local variable - load the pointer from the alloca
                        std::string loaded_ptr = fresh_reg();
                        emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                        struct_ptr = loaded_ptr;
                    }
                    // Direct parameters (this, other method params) are already pointers
                }
            } else {
                struct_type = llvm_type_from_semantic(semantic_type);
            }
        }
    }

    // Check if this is tuple element access (field name is a number like "0", "1", "2")
    bool is_tuple_access = !field.field.empty() && std::isdigit(field.field[0]);

    if (is_tuple_access) {
        // Tuple element access: tuple.0, tuple.1, etc.
        types::TypePtr obj_type = infer_expr_type(*field.object);
        if (obj_type && obj_type->is<types::TupleType>()) {
            const auto& tuple_type = obj_type->as<types::TupleType>();
            size_t idx = std::stoul(field.field);

            if (idx >= tuple_type.elements.size()) {
                report_error("Tuple index out of bounds: " + field.field, field.span);
                return "0";
            }

            // Get the element type
            types::TypePtr elem_type = tuple_type.elements[idx];
            std::string elem_llvm_type = llvm_type_from_semantic(elem_type);

            // Generate tuple type string for getelementptr
            std::string tuple_llvm_type = "{ ";
            for (size_t i = 0; i < tuple_type.elements.size(); ++i) {
                if (i > 0)
                    tuple_llvm_type += ", ";
                tuple_llvm_type += llvm_type_from_semantic(tuple_type.elements[i]);
            }
            tuple_llvm_type += " }";

            // Use getelementptr to access element, then load
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + tuple_llvm_type + ", ptr " +
                      struct_ptr + ", i32 0, i32 " + std::to_string(idx));

            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_llvm_type + ", ptr " + elem_ptr);
            last_expr_type_ = elem_llvm_type;
            return result;
        }
    }

    // Get struct/class type name
    std::string type_name = struct_type;
    if (type_name.starts_with("%struct.")) {
        type_name = type_name.substr(8);
    } else if (type_name.starts_with("%class.")) {
        type_name = type_name.substr(7);
    }

    // Check if this is a class property access (getter call)
    std::string prop_key = type_name + "." + field.field;
    auto prop_it = class_properties_.find(prop_key);
    if (prop_it != class_properties_.end() && prop_it->second.has_getter) {
        // Property access - call getter method instead of direct field access
        const auto& prop_info = prop_it->second;
        std::string getter_name =
            "@tml_" + get_suite_prefix() + type_name + "_get_" + prop_info.name;

        std::string result = fresh_reg();
        if (prop_info.is_static) {
            // Static property getter - no 'this' parameter
            emit_line("  " + result + " = call " + prop_info.llvm_type + " " + getter_name + "()");
        } else {
            // Instance property getter - pass 'this' pointer
            emit_line("  " + result + " = call " + prop_info.llvm_type + " " + getter_name +
                      "(ptr " + struct_ptr + ")");
        }
        last_expr_type_ = prop_info.llvm_type;
        return result;
    }

    // Get field index and type
    int field_idx = get_field_index(type_name, field.field);
    std::string field_type = get_field_type(type_name, field.field);

    std::string field_ptr;

    // Check if this is an inherited field (for class types)
    auto field_info = get_class_field_info(type_name, field.field);
    if (field_info && field_info->is_inherited && !field_info->inheritance_path.empty()) {
        // Generate chained GEPs through inheritance path
        std::string current_ptr = struct_ptr;
        std::string current_type = struct_type;

        for (const auto& step : field_info->inheritance_path) {
            std::string next_ptr = fresh_reg();
            emit_line("  " + next_ptr + " = getelementptr " + current_type + ", ptr " +
                      current_ptr + ", i32 0, i32 " + std::to_string(step.index));
            current_ptr = next_ptr;
            current_type = "%class." + step.class_name;
        }
        field_ptr = current_ptr;
    } else {
        // Direct field access
        field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + struct_ptr +
                  ", i32 0, i32 " + std::to_string(field_idx));
    }

    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + field_type + ", ptr " + field_ptr);
    last_expr_type_ = field_type;
    return result;
}

} // namespace tml::codegen
