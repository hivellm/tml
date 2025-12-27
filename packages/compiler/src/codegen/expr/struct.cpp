// LLVM IR generator - Struct expressions
// Handles: gen_struct_expr, gen_struct_expr_ptr, gen_field, get_field_index, get_field_type

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// Generate struct expression, returning pointer to allocated struct
auto LLVMIRGen::gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string {
    std::string base_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type;

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
        // Non-generic struct
        struct_type = "%struct." + base_name;
    }

    // Allocate struct on stack
    std::string ptr = fresh_reg();
    emit_line("  " + ptr + " = alloca " + struct_type);

    // Initialize fields - look up field index by name, not expression order
    // Get the struct name for field index lookup
    std::string struct_name_for_lookup = struct_type;
    if (struct_name_for_lookup.starts_with("%struct.")) {
        struct_name_for_lookup = struct_name_for_lookup.substr(8);
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
            field_val = gen_expr(*s.fields[i].second);
            // Infer field type for proper store
            types::TypePtr expr_type = infer_expr_type(*s.fields[i].second);
            field_type = llvm_type_from_semantic(expr_type);
        }

        std::string field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + ptr +
                  ", i32 0, i32 " + std::to_string(field_idx));
        emit_line("  store " + field_type + " " + field_val + ", ptr " + field_ptr);
    }

    return ptr;
}

auto LLVMIRGen::gen_struct_expr(const parser::StructExpr& s) -> std::string {
    std::string ptr = gen_struct_expr_ptr(s);
    std::string base_name = s.path.segments.empty() ? "anon" : s.path.segments.back();
    std::string struct_type;

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
        struct_type = "%struct." + base_name;
    }

    // Load the struct value
    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + struct_type + ", ptr " + ptr);

    return result;
}

// Helper to get field index for struct types - uses dynamic registry
auto LLVMIRGen::get_field_index(const std::string& struct_name,
                                const std::string& field_name) -> int {
    // First check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
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
auto LLVMIRGen::get_field_type(const std::string& struct_name,
                               const std::string& field_name) -> std::string {
    // First check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
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

auto LLVMIRGen::gen_field(const parser::FieldExpr& field) -> std::string {
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

    // Get struct type name
    std::string type_name = struct_type;
    if (type_name.starts_with("%struct.")) {
        type_name = type_name.substr(8);
    }

    // Get field index and type
    int field_idx = get_field_index(type_name, field.field);
    std::string field_type = get_field_type(type_name, field.field);

    // Use getelementptr to access field, then load
    std::string field_ptr = fresh_reg();
    emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + struct_ptr +
              ", i32 0, i32 " + std::to_string(field_idx));

    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + field_type + ", ptr " + field_ptr);
    last_expr_type_ = field_type;
    return result;
}

} // namespace tml::codegen
