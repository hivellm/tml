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

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>
#include <iostream>
#include <unordered_set>

namespace tml::codegen {

// Static helper to parse mangled type strings back to semantic types
// e.g., "ptr_ChannelNode__I32" -> PtrType{inner=NamedType{name="ChannelNode", type_args=[I32]}}
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Primitives
    if (s == "I64")
        return types::make_i64();
    if (s == "I32")
        return types::make_i32();
    if (s == "I8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I8};
        return t;
    }
    if (s == "I16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I16};
        return t;
    }
    if (s == "U8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U8};
        return t;
    }
    if (s == "U16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U16};
        return t;
    }
    if (s == "U32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U32};
        return t;
    }
    if (s == "U64") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Usize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Isize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I64};
        return t;
    }
    if (s == "F32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::F32};
        return t;
    }
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for nested generic (e.g., Mutex__I32)
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_string(arg_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::NamedType{base, "", {inner}};
            return t;
        }
    }

    // Simple struct type
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

// Generate struct expression, returning pointer to allocated struct
auto LLVMIRGen::gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string {
    std::string base_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type;

    // Check if this is a union type - unions have special initialization
    // Union literals only have one field set, and we bitcast to store it
    if (union_types_.find(base_name) != union_types_.end()) {
        std::string union_type = "%union." + base_name;

        // Allocate the union
        std::string ptr = fresh_reg();
        emit_line("  " + ptr + " = alloca " + union_type);

        // Unions only have one field provided - bitcast and store it
        if (!s.fields.empty()) {
            const std::string& field_name = s.fields[0].first;

            // Get the field type from struct_fields_ registry
            std::string field_type_llvm;
            auto fields_it = struct_fields_.find(base_name);
            if (fields_it != struct_fields_.end()) {
                for (const auto& finfo : fields_it->second) {
                    if (finfo.name == field_name) {
                        field_type_llvm = finfo.llvm_type;
                        break;
                    }
                }
            }

            // Generate the field value
            std::string field_val = gen_expr(*s.fields[0].second);
            if (field_type_llvm.empty()) {
                field_type_llvm = last_expr_type_;
            }

            // Store directly to the union pointer (all fields start at offset 0)
            emit_line("  store " + field_type_llvm + " " + field_val + ", ptr " + ptr);
        }

        last_expr_type_ = union_type;
        return ptr;
    }

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

            // Get field type BEFORE generating value - needed for generic enum variant inference
            std::string field_type = get_field_type(struct_name_for_lookup, field_name);

            // Set expected_enum_type_ if field is an enum type
            std::string saved_expected_enum_type = expected_enum_type_;
            if (!field_type.empty() && field_type.starts_with("%struct.")) {
                expected_enum_type_ = field_type;
            }

            // Set expected_literal_type_ for integer fields to allow coercion of literals
            if (field_type == "i8") {
                expected_literal_type_ = "i8";
                expected_literal_is_unsigned_ = false;
            } else if (field_type == "i16") {
                expected_literal_type_ = "i16";
                expected_literal_is_unsigned_ = false;
            } else if (field_type == "i64") {
                expected_literal_type_ = "i64";
                expected_literal_is_unsigned_ = false;
            }

            std::string field_val = gen_expr(*s.fields[i].second);

            // Mark variable as consumed if field value is an identifier (move semantics)
            if (s.fields[i].second->is<parser::IdentExpr>()) {
                const auto& ident = s.fields[i].second->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
            }

            // Restore expected types
            expected_enum_type_ = saved_expected_enum_type;
            expected_literal_type_.clear();
            expected_literal_is_unsigned_ = false;

            if (field_type.empty()) {
                field_type = last_expr_type_;
            }

            // Coerce { ptr, ptr } (fat pointer closure) to ptr (thin func pointer)
            if (last_expr_type_ == "{ ptr, ptr }" && field_type == "ptr") {
                std::string extracted = fresh_reg();
                emit_line("  " + extracted + " = extractvalue { ptr, ptr } " + field_val + ", 0");
                field_val = extracted;
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

            // Get field type BEFORE generating value - needed for generic enum variant inference
            std::string field_type = get_field_type(struct_name_for_lookup, field_name);

            // Set expected_enum_type_ if field is an enum type
            std::string saved_expected_enum_type = expected_enum_type_;
            if (!field_type.empty() && field_type.starts_with("%struct.")) {
                expected_enum_type_ = field_type;
            }

            // Set expected_literal_type_ for integer fields to allow coercion of literals
            // This handles cases like `head: -1` where -1 should be i64 not i32
            if (field_type == "i8") {
                expected_literal_type_ = "i8";
                expected_literal_is_unsigned_ = false;
            } else if (field_type == "i16") {
                expected_literal_type_ = "i16";
                expected_literal_is_unsigned_ = false;
            } else if (field_type == "i64") {
                expected_literal_type_ = "i64";
                expected_literal_is_unsigned_ = false;
            }

            std::string field_val = gen_expr(*s.fields[i].second);

            // Mark variable as consumed if field value is an identifier (move semantics)
            if (s.fields[i].second->is<parser::IdentExpr>()) {
                const auto& ident = s.fields[i].second->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
            }

            // Restore expected types
            expected_enum_type_ = saved_expected_enum_type;
            expected_literal_type_.clear();
            expected_literal_is_unsigned_ = false;

            if (field_type.empty()) {
                field_type = last_expr_type_;
            }

            // Coerce { ptr, ptr } (fat pointer closure) to ptr (thin func pointer)
            if (last_expr_type_ == "{ ptr, ptr }" && field_type == "ptr") {
                std::string extracted = fresh_reg();
                emit_line("  " + extracted + " = extractvalue { ptr, ptr } " + field_val + ", 0");
                field_val = extracted;
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
    // IMPORTANT: We must verify that the struct being created matches the generic struct
    // declaration. Multiple modules can have structs with the same base name (e.g.,
    // std::sync::Once and core::iter::sources::Once[T]). If field names don't match,
    // this is a different struct and should not be treated as generic.
    auto generic_it = pending_generic_structs_.find(base_name);
    bool is_matching_generic = false;
    if (generic_it != pending_generic_structs_.end() && !s.fields.empty()) {
        // Verify field names match the generic struct declaration
        const parser::StructDecl* decl = generic_it->second;
        if (s.fields.size() <= decl->fields.size()) {
            is_matching_generic = true;
            for (size_t i = 0; i < s.fields.size(); ++i) {
                bool found = false;
                for (const auto& decl_field : decl->fields) {
                    if (decl_field.name == s.fields[i].first) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    is_matching_generic = false;
                    break;
                }
            }
        }
    }
    if (is_matching_generic) {
        // This is a generic struct - first check if we can use existing type context

        // If we're in an impl method for this same type, use its type args
        // e.g., inside Ready[I64]::exhausted(), `return Ready { ... }` should be Ready[I64]
        if (!current_impl_type_.empty() && current_impl_type_.starts_with(base_name + "__")) {
            struct_type = "%struct." + current_impl_type_;
            // Ensure struct fields are registered (may need to trigger instantiation)
            if (struct_fields_.find(current_impl_type_) == struct_fields_.end()) {
                // Parse type args from current_impl_type_ and instantiate
                // e.g., "Ready__I64" -> base="Ready", type_args=[I64]
                std::string type_args_str = current_impl_type_.substr(base_name.size() + 2);
                std::vector<types::TypePtr> type_args;
                // Simple single-arg case: type_args_str is just "I64", "I32", etc.
                if (type_args_str == "I32")
                    type_args.push_back(types::make_i32());
                else if (type_args_str == "I64")
                    type_args.push_back(types::make_i64());
                else if (type_args_str == "Bool")
                    type_args.push_back(types::make_bool());
                else if (type_args_str == "Str")
                    type_args.push_back(types::make_str());
                else if (type_args_str == "F32")
                    type_args.push_back(types::make_primitive(types::PrimitiveKind::F32));
                else if (type_args_str == "F64")
                    type_args.push_back(types::make_f64());
                else if (type_args_str == "Unit")
                    type_args.push_back(types::make_unit());
                else {
                    // Try as named type, using parse_mangled_type_string for proper handling
                    type_args.push_back(parse_mangled_type_string(type_args_str));
                }
                require_struct_instantiation(base_name, type_args);
            }
        }
        // Or check if return type provides the context
        else if (!current_ret_type_.empty() &&
                 current_ret_type_.starts_with("%struct." + base_name + "__")) {
            struct_type = current_ret_type_;
        }
        // Otherwise infer type arguments from field values
        else {
            const parser::StructDecl* decl = generic_it->second;

            // Build substitution map by matching field types
            std::vector<types::TypePtr> type_args;
            std::unordered_map<std::string, types::TypePtr> inferred_generics;

            for (const auto& generic_param : decl->generics) {
                inferred_generics[generic_param.name] = nullptr;
            }

            // First check if we have type substitutions from enclosing generic context
            for (const auto& generic_param : decl->generics) {
                auto sub_it = current_type_subs_.find(generic_param.name);
                if (sub_it != current_type_subs_.end() && sub_it->second) {
                    inferred_generics[generic_param.name] = sub_it->second;
                }
            }

            // Match fields to infer generic types (for parameters not already substituted)
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
        }
    } else {
        // Check if it's a class type (via env or codegen registry)
        auto class_def = env_.lookup_class(base_name);
        if (class_def.has_value() || class_types_.find(base_name) != class_types_.end()) {
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

    // Handle struct update syntax (..base)
    // If base is present, first copy all fields from base, then override with specified fields
    if (s.base.has_value()) {
        // Generate the base expression to get a struct value
        std::string base_val = gen_expr(**s.base);

        // Skip store for empty structs (unit types) - "{}" has no data to copy
        if (struct_type != "{}") {
            // Store the base value into our new struct (copies all fields)
            emit_line("  store " + struct_type + " " + base_val + ", ptr " + ptr);
        }
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

            // Set expected_enum_type_ if field is a struct type (for generic enum variant
            // inference)
            std::string saved_expected_enum_type = expected_enum_type_;
            if (!target_field_type.empty() && target_field_type.starts_with("%struct.")) {
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

            // Mark variable as consumed if field value is an identifier (move semantics)
            if (s.fields[i].second->is<parser::IdentExpr>()) {
                const auto& ident = s.fields[i].second->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
            }

            std::string actual_llvm_type =
                last_expr_type_; // Capture actual LLVM type from gen_expr
            expected_enum_type_ = saved_expected_enum_type; // Restore after expression
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
                // Handle { ptr, ptr } -> ptr coercion (fat pointer closure -> thin func pointer)
                else if (actual_llvm_type == "{ ptr, ptr }" && target_field_type == "ptr") {
                    std::string casted = fresh_reg();
                    emit_line("  " + casted + " = extractvalue { ptr, ptr } " + field_val + ", 0");
                    field_val = casted;
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

    // Generate default values for fields not explicitly provided in the literal
    // Build set of provided field names
    std::unordered_set<std::string> provided_fields;
    for (const auto& [fname, fexpr] : s.fields) {
        provided_fields.insert(fname);
    }

    // Look up struct declaration for default values
    const parser::StructDecl* decl = nullptr;
    auto decl_it = struct_decls_.find(base_name);
    if (decl_it != struct_decls_.end()) {
        decl = decl_it->second;
    } else {
        // Check pending_generic_structs_ for generic structs
        auto pending_decl_it = pending_generic_structs_.find(base_name);
        if (pending_decl_it != pending_generic_structs_.end()) {
            decl = pending_decl_it->second;
        }
    }

    // If we found the declaration, generate default values for missing fields
    if (decl) {
        for (const auto& field : decl->fields) {
            // Skip fields that were explicitly provided
            if (provided_fields.find(field.name) != provided_fields.end()) {
                continue;
            }
            // Generate default value if present
            if (field.default_value.has_value()) {
                int field_idx = get_field_index(struct_name_for_lookup, field.name);
                std::string target_field_type = get_field_type(struct_name_for_lookup, field.name);

                // Set expected types for literals
                std::string saved_expected_literal = expected_literal_type_;
                bool saved_unsigned = expected_literal_is_unsigned_;
                if (target_field_type == "i8" || target_field_type == "i16" ||
                    target_field_type == "i64") {
                    expected_literal_type_ = target_field_type;
                    expected_literal_is_unsigned_ = false;
                }

                std::string default_val = gen_expr(**field.default_value);
                std::string actual_type = last_expr_type_;

                // Restore expected types
                expected_literal_type_ = saved_expected_literal;
                expected_literal_is_unsigned_ = saved_unsigned;

                // Handle type coercions if needed
                if (!target_field_type.empty() && target_field_type != actual_type) {
                    if ((actual_type == "i32" || actual_type == "i64") &&
                        (target_field_type == "i64" || target_field_type == "i32")) {
                        if (actual_type == "i32" && target_field_type == "i64") {
                            std::string casted = fresh_reg();
                            emit_line("  " + casted + " = sext i32 " + default_val + " to i64");
                            default_val = casted;
                        } else if (actual_type == "i64" && target_field_type == "i32") {
                            std::string casted = fresh_reg();
                            emit_line("  " + casted + " = trunc i64 " + default_val + " to i32");
                            default_val = casted;
                        }
                        actual_type = target_field_type;
                    }
                }

                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + ptr +
                          ", i32 0, i32 " + std::to_string(field_idx));
                emit_line("  store " +
                          (target_field_type.empty() ? actual_type : target_field_type) + " " +
                          default_val + ", ptr " + field_ptr);
            }
        }
    }

    return ptr;
}

auto LLVMIRGen::gen_struct_expr(const parser::StructExpr& s) -> std::string {
    std::string ptr = gen_struct_expr_ptr(s);
    std::string base_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type;

    // Check if this is a union type
    if (union_types_.find(base_name) != union_types_.end()) {
        std::string union_type = "%union." + base_name;

        // Load the union value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + union_type + ", ptr " + ptr);
        last_expr_type_ = union_type;
        return result;
    }

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
            // IMPORTANT: Verify field names match to avoid collisions between modules
            auto generic_it = pending_generic_structs_.find(base_name);
            bool is_matching_generic_expr = false;
            if (generic_it != pending_generic_structs_.end() && !s.fields.empty()) {
                const parser::StructDecl* decl = generic_it->second;
                if (s.fields.size() <= decl->fields.size()) {
                    is_matching_generic_expr = true;
                    for (size_t i = 0; i < s.fields.size(); ++i) {
                        bool found = false;
                        for (const auto& decl_field : decl->fields) {
                            if (decl_field.name == s.fields[i].first) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            is_matching_generic_expr = false;
                            break;
                        }
                    }
                }
            }
            if (is_matching_generic_expr) {
                const parser::StructDecl* decl = generic_it->second;
                std::vector<types::TypePtr> type_args;
                std::unordered_map<std::string, types::TypePtr> inferred_generics;

                for (const auto& generic_param : decl->generics) {
                    inferred_generics[generic_param.name] = nullptr;
                }

                // First check if we have type substitutions from enclosing generic context
                // This is critical for generic functions like channel[T]() where T is
                // not directly visible in field values but is in current_type_subs_
                for (const auto& generic_param : decl->generics) {
                    auto sub_it = current_type_subs_.find(generic_param.name);
                    if (sub_it != current_type_subs_.end() && sub_it->second) {
                        inferred_generics[generic_param.name] = sub_it->second;
                    }
                }

                // Then try to infer from field values for any remaining unresolved params
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
                    // Non-generic struct - ensure type is defined (handles imported structs)
                    // Use llvm_type_from_semantic to trigger type emission if needed
                    auto sem_type =
                        std::make_shared<types::Type>(types::NamedType{base_name, "", {}});
                    struct_type = llvm_type_from_semantic(sem_type, true);
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

// Helper to get field semantic type for struct types - uses dynamic registry
auto LLVMIRGen::get_field_semantic_type(const std::string& struct_name,
                                        const std::string& field_name) -> types::TypePtr {
    // Check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return field.semantic_type;
            }
        }
    }

    // Note: class_fields_ uses a different struct (ClassFieldInfo) without semantic_type
    // For class fields, we'd need to look up the type from the class definition
    return nullptr;
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

                // Ensure the generic struct is instantiated so its fields are registered
                // Parse the mangled name to get base_name and type_args
                // e.g., "Arc__I32" -> base="Arc", type_args=[I32]
                auto sep_pos = current_impl_type_.find("__");
                if (sep_pos != std::string::npos) {
                    std::string base_name = current_impl_type_.substr(0, sep_pos);
                    // Use semantic type from locals_ if available for proper type args
                    if (it->second.semantic_type &&
                        it->second.semantic_type->is<types::NamedType>()) {
                        const auto& named = it->second.semantic_type->as<types::NamedType>();
                        if (!named.type_args.empty()) {
                            // Apply current type substitutions to get concrete types
                            // e.g., Arc[T] with T=I32 becomes Arc[I32]
                            std::vector<types::TypePtr> concrete_args;
                            for (const auto& arg : named.type_args) {
                                if (!current_type_subs_.empty()) {
                                    concrete_args.push_back(
                                        apply_type_substitutions(arg, current_type_subs_));
                                } else {
                                    concrete_args.push_back(arg);
                                }
                            }
                            require_struct_instantiation(named.name, concrete_args);
                        }
                    }
                }
            }
        }
    } else if (field.object->is<parser::FieldExpr>()) {
        // Chained field access (e.g., this.inner.receiver_alive)
        // Generate the intermediate field access recursively
        const auto& nested_field = field.object->as<parser::FieldExpr>();

        // Get the outermost struct
        if (nested_field.object->is<parser::IdentExpr>()) {
            const auto& ident = nested_field.object->as<parser::IdentExpr>();
            std::string outer_type;
            std::string outer_ptr;

            // Special handling for 'this' in impl methods
            if (ident.name == "this" && !current_impl_type_.empty()) {
                outer_type = "%struct." + current_impl_type_;
                outer_ptr = "%this";
            } else {
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    outer_type = it->second.type;
                    outer_ptr = it->second.reg;

                    // Handle ref types - resolve the actual struct type from semantic type
                    // This fixes chained field access on ref parameters (e.g.,
                    // ref_param.field1.field2)
                    if (outer_type == "ptr" && it->second.semantic_type) {
                        types::TypePtr sem_type = it->second.semantic_type;
                        if (sem_type->is<types::RefType>()) {
                            const auto& ref = sem_type->as<types::RefType>();
                            types::TypePtr resolved_inner = ref.inner;
                            if (!current_type_subs_.empty()) {
                                resolved_inner =
                                    apply_type_substitutions(ref.inner, current_type_subs_);
                            }
                            outer_type = llvm_type_from_semantic(resolved_inner);
                            // Load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + outer_ptr);
                            outer_ptr = loaded_ptr;
                        } else if (sem_type->is<types::PtrType>()) {
                            const auto& ptr = sem_type->as<types::PtrType>();
                            types::TypePtr resolved_inner = ptr.inner;
                            if (!current_type_subs_.empty()) {
                                resolved_inner =
                                    apply_type_substitutions(ptr.inner, current_type_subs_);
                            }
                            outer_type = llvm_type_from_semantic(resolved_inner);
                            // Load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + outer_ptr);
                            outer_ptr = loaded_ptr;
                        }
                    }
                }
            }

            if (!outer_type.empty() && !outer_ptr.empty()) {
                // Get outer struct type name
                std::string outer_name = outer_type;
                if (outer_name.starts_with("%struct.")) {
                    outer_name = outer_name.substr(8);
                }

                // Check for auto-deref on the outer type (e.g., Arc[ChannelInner[T]])
                types::TypePtr outer_sem_type = infer_expr_type(*nested_field.object);
                types::TypePtr deref_target = get_deref_target_type(outer_sem_type);
                if (deref_target && !struct_has_field(outer_name, nested_field.field)) {
                    // Need to auto-deref to access the field
                    auto sep_pos = outer_name.find("__");
                    std::string base_type_name =
                        (sep_pos != std::string::npos) ? outer_name.substr(0, sep_pos) : outer_name;

                    if (base_type_name == "Arc" || base_type_name == "Shared" ||
                        base_type_name == "Rc") {
                        // Arc layout: { ptr: Ptr[ArcInner[T]] }
                        std::string arc_ptr_field = fresh_reg();
                        emit_line("  " + arc_ptr_field + " = getelementptr " + outer_type +
                                  ", ptr " + outer_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + arc_ptr_field);

                        // Get ArcInner type
                        std::string arc_inner_mangled = mangle_struct_name(
                            "ArcInner", std::vector<types::TypePtr>{deref_target});
                        std::string arc_inner_type = "%struct." + arc_inner_mangled;

                        // GEP to data field (index 2)
                        std::string data_ptr = fresh_reg();
                        emit_line("  " + data_ptr + " = getelementptr " + arc_inner_type +
                                  ", ptr " + inner_ptr + ", i32 0, i32 2");

                        // Update outer_ptr and outer_type to point to inner struct
                        outer_ptr = data_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                outer_name = require_struct_instantiation(inner_named.name,
                                                                          inner_named.type_args);
                            } else {
                                outer_name = inner_named.name;
                            }
                            outer_type = "%struct." + outer_name;
                        }
                    } else if (base_type_name == "Box" || base_type_name == "Heap") {
                        // Box layout: { ptr: Ptr[T] }
                        std::string box_ptr_field = fresh_reg();
                        emit_line("  " + box_ptr_field + " = getelementptr " + outer_type +
                                  ", ptr " + outer_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + box_ptr_field);

                        outer_ptr = inner_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                outer_name = require_struct_instantiation(inner_named.name,
                                                                          inner_named.type_args);
                            } else {
                                outer_name = inner_named.name;
                            }
                            outer_type = "%struct." + outer_name;
                        }
                    }
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

                // If nested_type is "ptr", get the semantic type for correct type inference
                // This is crucial for generic struct fields like `mutex: mut ref Mutex[T]`
                if (nested_type == "ptr") {
                    types::TypePtr field_sem_type =
                        get_field_semantic_type(outer_name, nested_field.field);
                    if (field_sem_type && !current_type_subs_.empty()) {
                        field_sem_type =
                            apply_type_substitutions(field_sem_type, current_type_subs_);
                    }
                    // Store the semantic type for later use in the struct_type == "ptr" path
                    if (field_sem_type) {
                        // Extract the inner type from Ref/Ptr
                        types::TypePtr inner_type = field_sem_type;
                        if (field_sem_type->is<types::RefType>()) {
                            inner_type = field_sem_type->as<types::RefType>().inner;
                        } else if (field_sem_type->is<types::PtrType>()) {
                            inner_type = field_sem_type->as<types::PtrType>().inner;
                        }
                        if (inner_type && inner_type->is<types::NamedType>()) {
                            const auto& named = inner_type->as<types::NamedType>();
                            if (!named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                std::string mangled =
                                    require_struct_instantiation(named.name, named.type_args);
                                struct_type = "%struct." + mangled;
                            } else {
                                struct_type = "%struct." + named.name;
                            }
                        }
                    }
                }
            }
        } else {
            // Handle deeper nesting: recursively generate the intermediate field access
            std::string nested_val = gen_expr(*field.object);
            types::TypePtr nested_sem_type = infer_expr_type(*field.object);

            // For struct types, gen_expr returns a loaded value
            // We need to store to a temp alloca if it's a struct value
            if (last_expr_type_.starts_with("%struct.")) {
                std::string temp_ptr = fresh_reg();
                emit_line("  " + temp_ptr + " = alloca " + last_expr_type_);
                emit_line("  store " + last_expr_type_ + " " + nested_val + ", ptr " + temp_ptr);
                struct_ptr = temp_ptr;
                struct_type = last_expr_type_;
            } else if (last_expr_type_ == "ptr") {
                // Pointer type - use directly
                struct_ptr = nested_val;

                // Infer the struct type from the semantic type
                // Apply type substitutions for generic contexts
                types::TypePtr resolved_sem_type = nested_sem_type;
                if (resolved_sem_type && !current_type_subs_.empty()) {
                    resolved_sem_type =
                        apply_type_substitutions(resolved_sem_type, current_type_subs_);
                }

                if (resolved_sem_type && resolved_sem_type->is<types::NamedType>()) {
                    const auto& named = resolved_sem_type->as<types::NamedType>();
                    if (!named.type_args.empty()) {
                        // Use return value to handle UNRESOLVED cases
                        std::string mangled =
                            require_struct_instantiation(named.name, named.type_args);
                        struct_type = "%struct." + mangled;
                    } else {
                        struct_type = "%struct." + named.name;
                    }
                }
            }
        }
    } else if (field.object->is<parser::UnaryExpr>()) {
        // Handle dereferenced pointer field access (e.g., (*ptr).field)
        const auto& unary = field.object->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Deref) {
            // Generate the pointer value
            struct_ptr = gen_expr(*unary.operand);

            // Infer the pointee type
            types::TypePtr ptr_type = infer_expr_type(*unary.operand);
            if (ptr_type) {
                types::TypePtr inner_type;
                if (ptr_type->is<types::PtrType>()) {
                    inner_type = ptr_type->as<types::PtrType>().inner;
                } else if (ptr_type->is<types::RefType>()) {
                    inner_type = ptr_type->as<types::RefType>().inner;
                } else if (ptr_type->is<types::NamedType>()) {
                    // Handle TML's Ptr[T] type (NamedType with name="Ptr" or "RawPtr")
                    const auto& named = ptr_type->as<types::NamedType>();
                    if ((named.name == "Ptr" || named.name == "RawPtr") &&
                        !named.type_args.empty()) {
                        inner_type = named.type_args[0];
                    }
                }

                // Apply type substitutions for generic types
                // E.g., Ptr[Node[T]] with T -> I32 becomes Node[I32]
                if (inner_type && !current_type_subs_.empty()) {
                    inner_type = apply_type_substitutions(inner_type, current_type_subs_);
                }

                if (inner_type) {
                    std::string type_name;
                    if (inner_type->is<types::NamedType>()) {
                        type_name = inner_type->as<types::NamedType>().name;
                        // Check if it's a generic type and mangle accordingly
                        const auto& named = inner_type->as<types::NamedType>();
                        if (!named.type_args.empty()) {
                            // Ensure generic struct is instantiated so fields are registered
                            // Use return value to handle UNRESOLVED cases
                            std::string mangled =
                                require_struct_instantiation(type_name, named.type_args);
                            struct_type = "%struct." + mangled;
                        } else {
                            struct_type = "%struct." + type_name;
                        }
                    } else if (inner_type->is<types::ClassType>()) {
                        type_name = inner_type->as<types::ClassType>().name;
                        struct_type = "%class." + type_name;
                    }
                }
            }
        }
    } else if (field.object->is<parser::CallExpr>() || field.object->is<parser::MethodCallExpr>()) {
        // Handle field access on function/method call return value (e.g., func().field)
        // Generate the call, which returns a struct value
        std::string call_result = gen_expr(*field.object);
        types::TypePtr call_type = infer_expr_type(*field.object);

        // Apply current type substitutions to resolve generic types
        if (call_type && !current_type_subs_.empty()) {
            call_type = apply_type_substitutions(call_type, current_type_subs_);
        }

        TML_DEBUG_LN("[GEN_FIELD] CallExpr/MethodCallExpr - field="
                     << field.field << " last_expr_type_=" << last_expr_type_
                     << " call_type=" << (call_type ? types::type_to_string(call_type) : "null"));

        // For struct return values, we need to store to a temp alloca
        if (last_expr_type_.starts_with("%struct.")) {
            std::string temp_ptr = fresh_reg();
            emit_line("  " + temp_ptr + " = alloca " + last_expr_type_);
            emit_line("  store " + last_expr_type_ + " " + call_result + ", ptr " + temp_ptr);
            struct_ptr = temp_ptr;
            struct_type = last_expr_type_;
        } else if ((last_expr_type_ == "ptr" || last_expr_type_ == "i64") && call_type &&
                   call_type->is<types::NamedType>()) {
            // Check if the semantic type is a struct - if so, treat i64 as ptr
            // This handles cases like List::get returning an i64 that is actually a struct pointer
            const auto& named = call_type->as<types::NamedType>();

            // Look up if this is a struct type
            bool is_struct_type = false;
            auto struct_def = env_.lookup_struct(named.name);
            if (struct_def) {
                is_struct_type = true;
            } else if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    if (mod.structs.find(named.name) != mod.structs.end() ||
                        mod.internal_structs.find(named.name) != mod.internal_structs.end()) {
                        is_struct_type = true;
                        break;
                    }
                }
            }

            if (is_struct_type) {
                // Convert i64 to ptr if needed
                if (last_expr_type_ == "i64") {
                    std::string ptr_val = fresh_reg();
                    emit_line("  " + ptr_val + " = inttoptr i64 " + call_result + " to ptr");
                    struct_ptr = ptr_val;
                } else {
                    struct_ptr = call_result;
                }

                // Resolve the struct type
                if (!named.type_args.empty()) {
                    std::string mangled = require_struct_instantiation(named.name, named.type_args);
                    struct_type = "%struct." + mangled;
                } else {
                    struct_type = "%struct." + named.name;
                }
            }
        } else if (last_expr_type_ == "ptr" && call_type) {
            // Pointer type - the return value is a pointer to the struct
            struct_ptr = call_result;

            // Resolve the struct type from semantic type
            types::TypePtr resolved_type = call_type;
            if (!current_type_subs_.empty()) {
                resolved_type = apply_type_substitutions(call_type, current_type_subs_);
            }

            if (resolved_type && resolved_type->is<types::NamedType>()) {
                const auto& named = resolved_type->as<types::NamedType>();
                if (!named.type_args.empty()) {
                    std::string mangled = require_struct_instantiation(named.name, named.type_args);
                    struct_type = "%struct." + mangled;
                } else {
                    struct_type = "%struct." + named.name;
                }
            }
        }
    }

    if (struct_type.empty() || struct_ptr.empty()) {
        report_error("Cannot resolve field access object", field.span, "C003");
        return "0";
    }

    // If struct_type is ptr, infer the actual struct type from the expression
    if (struct_type == "ptr") {
        types::TypePtr semantic_type = infer_expr_type(*field.object);
        TML_DEBUG_LN("[GEN_FIELD] struct_type is ptr, field="
                     << field.field << ", semantic_type="
                     << (semantic_type ? types::type_to_string(semantic_type) : "null"));
        if (semantic_type) {
            // If the semantic type is a reference or pointer, get the inner type
            // and load the pointer from the alloca first
            if (semantic_type->is<types::RefType>()) {
                const auto& ref = semantic_type->as<types::RefType>();
                // Apply type substitutions for generic impl methods
                // e.g., if inner is Mutex[T] and current_type_subs_ = {T: I32},
                // we need Mutex[I32] not Mutex[T]
                types::TypePtr resolved_inner = ref.inner;
                TML_DEBUG_LN("[GEN_FIELD] RefType inner=" << types::type_to_string(ref.inner)
                                                          << ", current_type_subs_.size="
                                                          << current_type_subs_.size());
                if (!current_type_subs_.empty()) {
                    resolved_inner = apply_type_substitutions(ref.inner, current_type_subs_);
                    TML_DEBUG_LN("[GEN_FIELD] After substitution: "
                                 << types::type_to_string(resolved_inner));
                }
                struct_type = llvm_type_from_semantic(resolved_inner);
                TML_DEBUG_LN("[GEN_FIELD] struct_type set to: " << struct_type);
                // struct_ptr points to an alloca containing a pointer to the struct
                // We need to load the pointer first
                std::string loaded_ptr = fresh_reg();
                emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                struct_ptr = loaded_ptr;
            } else if (semantic_type->is<types::PtrType>()) {
                const auto& ptr = semantic_type->as<types::PtrType>();
                // Apply type substitutions for generic impl methods
                types::TypePtr resolved_inner = ptr.inner;
                if (!current_type_subs_.empty()) {
                    resolved_inner = apply_type_substitutions(ptr.inner, current_type_subs_);
                }
                struct_type = llvm_type_from_semantic(resolved_inner);
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
                report_error("Tuple index out of bounds: " + field.field, field.span, "C003");
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

            // Mark the tuple variable as consumed when extracting elements (move semantics)
            // This prevents double-free when tuple elements are moved to new bindings
            if (field.object->is<parser::IdentExpr>()) {
                const auto& ident = field.object->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
            }

            return result;
        }
    }

    // Get struct/class/union type name
    std::string type_name = struct_type;
    bool is_union_type = false;
    if (type_name.starts_with("%struct.")) {
        type_name = type_name.substr(8);
    } else if (type_name.starts_with("%class.")) {
        type_name = type_name.substr(7);
    } else if (type_name.starts_with("%union.")) {
        type_name = type_name.substr(7);
        is_union_type = true;
    }

    // Check if this is a union type (also check registry in case type was set differently)
    if (!is_union_type && union_types_.find(type_name) != union_types_.end()) {
        is_union_type = true;
    }

    // Check for auto-deref on smart pointer types (Arc, Box, etc.)
    // If the field is not found on the smart pointer type, dereference to the inner type
    types::TypePtr obj_type = infer_expr_type(*field.object);
    types::TypePtr deref_target = get_deref_target_type(obj_type);
    if (deref_target && !struct_has_field(type_name, field.field)) {
        TML_DEBUG_LN("[GEN_FIELD] Auto-deref: " << type_name << " -> "
                                                << types::type_to_string(deref_target));

        // Generate deref code for Arc[T]:
        // 1. Load arc.ptr (field 0) to get Ptr[ArcInner[T]]
        // 2. GEP to get (*ptr).data (field 2) which is T
        // 3. Then access field.field on T

        std::string ptr_type = type_name;
        // Extract base type name from mangled name (e.g., Arc__ChannelInner__I32 -> Arc)
        auto sep_pos = ptr_type.find("__");
        std::string base_type_name =
            (sep_pos != std::string::npos) ? ptr_type.substr(0, sep_pos) : ptr_type;

        if (base_type_name == "Arc" || base_type_name == "Shared" || base_type_name == "Rc") {
            // Arc layout: { ptr: Ptr[ArcInner[T]] }
            // ArcInner layout: { strong: AtomicUsize, weak: AtomicUsize, data: T }

            // Load the inner ptr from Arc struct (field 0)
            std::string arc_ptr_field = fresh_reg();
            emit_line("  " + arc_ptr_field + " = getelementptr " + struct_type + ", ptr " +
                      struct_ptr + ", i32 0, i32 0");
            std::string inner_ptr = fresh_reg();
            emit_line("  " + inner_ptr + " = load ptr, ptr " + arc_ptr_field);

            // Get the ArcInner type - need to figure out its mangled name
            // For Arc[ChannelInner[I32]], inner is ChannelInner[I32]
            // ArcInner[ChannelInner[I32]] is the actual inner struct
            std::string arc_inner_mangled = "ArcInner";
            if (deref_target->is<types::NamedType>()) {
                arc_inner_mangled =
                    mangle_struct_name("ArcInner", std::vector<types::TypePtr>{deref_target});
            }

            // GEP to get data field of ArcInner (field index 2: strong=0, weak=1, data=2)
            std::string arc_inner_type = "%struct." + arc_inner_mangled;
            std::string data_ptr = fresh_reg();
            emit_line("  " + data_ptr + " = getelementptr " + arc_inner_type + ", ptr " +
                      inner_ptr + ", i32 0, i32 2");

            // Now update struct_ptr to point to the data and struct_type to the inner type
            struct_ptr = data_ptr;
            if (deref_target->is<types::NamedType>()) {
                const auto& inner_named = deref_target->as<types::NamedType>();
                if (!inner_named.type_args.empty()) {
                    // Use return value to handle UNRESOLVED cases
                    std::string mangled =
                        require_struct_instantiation(inner_named.name, inner_named.type_args);
                    struct_type = "%struct." + mangled;
                    type_name = mangled;
                } else {
                    struct_type = "%struct." + inner_named.name;
                    type_name = inner_named.name;
                }
            }
            TML_DEBUG_LN("[GEN_FIELD] After auto-deref: struct_type="
                         << struct_type << " type_name=" << type_name);
        } else if (base_type_name == "Box" || base_type_name == "Heap") {
            // Box/Heap layout: { ptr: Ptr[T] }
            // Simply load the ptr and access the field on T

            // Load the inner ptr from Box struct (field 0)
            std::string box_ptr_field = fresh_reg();
            emit_line("  " + box_ptr_field + " = getelementptr " + struct_type + ", ptr " +
                      struct_ptr + ", i32 0, i32 0");
            std::string inner_ptr = fresh_reg();
            emit_line("  " + inner_ptr + " = load ptr, ptr " + box_ptr_field);

            // Update struct_ptr and struct_type
            struct_ptr = inner_ptr;
            if (deref_target->is<types::NamedType>()) {
                const auto& inner_named = deref_target->as<types::NamedType>();
                if (!inner_named.type_args.empty()) {
                    // Use return value to handle UNRESOLVED cases
                    std::string mangled =
                        require_struct_instantiation(inner_named.name, inner_named.type_args);
                    struct_type = "%struct." + mangled;
                    type_name = mangled;
                } else {
                    struct_type = "%struct." + inner_named.name;
                    type_name = inner_named.name;
                }
            }
        } else if (base_type_name == "Ptr" || base_type_name == "RawPtr") {
            // Ptr[T] is already a pointer to T
            // struct_ptr is the pointer value, just update the type info
            if (deref_target->is<types::NamedType>()) {
                const auto& inner_named = deref_target->as<types::NamedType>();
                if (!inner_named.type_args.empty()) {
                    // Use return value to handle UNRESOLVED cases
                    std::string mangled =
                        require_struct_instantiation(inner_named.name, inner_named.type_args);
                    struct_type = "%struct." + mangled;
                    type_name = mangled;
                } else {
                    struct_type = "%struct." + inner_named.name;
                    type_name = inner_named.name;
                }
            }
            TML_DEBUG_LN("[GEN_FIELD] Ptr auto-deref: struct_type=" << struct_type
                                                                    << " type_name=" << type_name);
        }
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

    // Union field access - load directly from union pointer (all fields at offset 0)
    if (is_union_type) {
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + field_type + ", ptr " + struct_ptr);
        last_expr_type_ = field_type;
        return result;
    }

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
