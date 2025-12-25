// LLVM IR generator - Types and collections
// Handles: struct expressions, fields, arrays, indexing, paths, method calls, format print

#include "tml/codegen/llvm_ir_gen.hpp"
#include <algorithm>
#include <cctype>

namespace tml::codegen {

// Helper: infer semantic type from expression for generics instantiation
auto LLVMIRGen::infer_expr_type(const parser::Expr& expr) -> types::TypePtr {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        switch (lit.token.kind) {
            case lexer::TokenKind::IntLiteral: return types::make_i32();
            case lexer::TokenKind::FloatLiteral: return types::make_f64();
            case lexer::TokenKind::BoolLiteral: return types::make_bool();
            case lexer::TokenKind::StringLiteral: return types::make_str();
            case lexer::TokenKind::CharLiteral: return types::make_primitive(types::PrimitiveKind::Char);
            default: return types::make_i32();
        }
    }
    if (expr.is<parser::IdentExpr>()) {
        const auto& ident = expr.as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            // Use semantic type if available (for complex types like Ptr[T])
            if (it->second.semantic_type) {
                return it->second.semantic_type;
            }
            // Map LLVM type back to semantic type
            const std::string& ty = it->second.type;
            if (ty == "i32") return types::make_i32();
            if (ty == "i64") return types::make_i64();
            if (ty == "i1") return types::make_bool();
            if (ty == "float") return types::make_primitive(types::PrimitiveKind::F32);
            if (ty == "double") return types::make_f64();
            if (ty == "ptr") return types::make_str(); // Assume string for now
            // For struct types, try to extract
            if (ty.starts_with("%struct.")) {
                std::string name = ty.substr(8);
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{name, "", {}};
                return result;
            }
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        // Binary expressions: infer from left operand
        const auto& bin = expr.as<parser::BinaryExpr>();
        return infer_expr_type(*bin.left);
    }
    if (expr.is<parser::UnaryExpr>()) {
        const auto& unary = expr.as<parser::UnaryExpr>();
        return infer_expr_type(*unary.operand);
    }
    if (expr.is<parser::StructExpr>()) {
        const auto& s = expr.as<parser::StructExpr>();
        if (!s.path.segments.empty()) {
            std::string base_name = s.path.segments.back();

            // Check if this is a generic struct
            auto generic_it = pending_generic_structs_.find(base_name);
            if (generic_it != pending_generic_structs_.end() && !s.fields.empty()) {
                // Infer type arguments from field values
                const parser::StructDecl* decl = generic_it->second;
                std::vector<types::TypePtr> type_args;
                std::unordered_map<std::string, types::TypePtr> inferred_generics;

                for (const auto& gp : decl->generics) {
                    inferred_generics[gp.name] = nullptr;
                }

                for (size_t fi = 0; fi < s.fields.size() && fi < decl->fields.size(); ++fi) {
                    const auto& field_decl = decl->fields[fi];
                    if (field_decl.type && field_decl.type->is<parser::NamedType>()) {
                        const auto& ftype = field_decl.type->as<parser::NamedType>();
                        std::string ft_name = ftype.path.segments.empty() ? "" : ftype.path.segments.back();
                        auto gen_it = inferred_generics.find(ft_name);
                        if (gen_it != inferred_generics.end() && !gen_it->second) {
                            gen_it->second = infer_expr_type(*s.fields[fi].second);
                        }
                    }
                }

                for (const auto& gp : decl->generics) {
                    auto inf = inferred_generics[gp.name];
                    type_args.push_back(inf ? inf : types::make_i32());
                }

                // Return NamedType with type_args
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{base_name, "", std::move(type_args)};
                return result;
            }

            // Non-generic struct
            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{base_name, "", {}};
            return result;
        }
    }
    // Default: I32
    return types::make_i32();
}

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
                std::string type_name = ftype.path.segments.empty() ? "" : ftype.path.segments.back();
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
                for (size_t ni = 0; ni < nested.fields.size() && ni < nested_decl->fields.size(); ++ni) {
                    const auto& nf = nested_decl->fields[ni];
                    if (nf.type && nf.type->is<parser::NamedType>()) {
                        const auto& nft = nf.type->as<parser::NamedType>();
                        std::string nft_name = nft.path.segments.empty() ? "" : nft.path.segments.back();
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
                std::string nested_mangled = require_struct_instantiation(nested_base, nested_type_args);
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
        emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + ptr + ", i32 0, i32 " + std::to_string(field_idx));
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
                std::string type_name = ftype.path.segments.empty() ? "" : ftype.path.segments.back();
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
auto LLVMIRGen::get_field_index(const std::string& struct_name, const std::string& field_name) -> int {
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
        if (field_name == "x") return 0;
        if (field_name == "y") return 1;
    }
    if (struct_name == "Rectangle") {
        if (field_name == "origin") return 0;
        if (field_name == "width") return 1;
        if (field_name == "height") return 2;
    }
    return 0;
}

// Helper to get field type for struct types - uses dynamic registry
auto LLVMIRGen::get_field_type(const std::string& struct_name, const std::string& field_name) -> std::string {
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
                // Load the actual 'this' pointer from the alloca
                std::string loaded_this = fresh_reg();
                emit_line("  " + loaded_this + " = load ptr, ptr " + struct_ptr);
                struct_ptr = loaded_this;
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
                emit_line("  " + nested_ptr + " = getelementptr " + outer_type + ", ptr " + outer_ptr + ", i32 0, i32 " + std::to_string(nested_idx));

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
    emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + struct_ptr + ", i32 0, i32 " + std::to_string(field_idx));

    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + field_type + ", ptr " + field_ptr);
    return result;
}

// Generate formatted print: "hello {} world {}" with args
// Supports {}, {:.N} for floats with N decimal places
auto LLVMIRGen::gen_format_print(const std::string& format,
                                   const std::vector<parser::ExprPtr>& args,
                                   size_t start_idx,
                                   bool with_newline) -> std::string {
    // Parse format string and print segments with arguments
    size_t arg_idx = start_idx;
    size_t pos = 0;
    std::string result = "0";

    while (pos < format.size()) {
        // Find next { placeholder
        size_t placeholder = format.find('{', pos);

        if (placeholder == std::string::npos) {
            // No more placeholders, print remaining text
            if (pos < format.size()) {
                std::string remaining = format.substr(pos);
                std::string str_const = add_string_literal(remaining);
                result = fresh_reg();
                emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + str_const + ")");
            }
            break;
        }

        // Print text before placeholder
        if (placeholder > pos) {
            std::string segment = format.substr(pos, placeholder - pos);
            std::string str_const = add_string_literal(segment);
            result = fresh_reg();
            emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + str_const + ")");
        }

        // Parse placeholder: {} or {:.N}
        size_t end_brace = format.find('}', placeholder);
        if (end_brace == std::string::npos) {
            pos = placeholder + 1;
            continue;
        }

        std::string placeholder_content = format.substr(placeholder + 1, end_brace - placeholder - 1);
        int precision = -1; // -1 means no precision specified

        // Parse {:.N} format
        if (placeholder_content.starts_with(":.")) {
            std::string prec_str = placeholder_content.substr(2);
            if (!prec_str.empty() && std::all_of(prec_str.begin(), prec_str.end(), ::isdigit)) {
                precision = std::stoi(prec_str);
            }
        }

        // Print argument
        if (arg_idx < args.size()) {
            const auto& arg_expr = *args[arg_idx];
            std::string arg_val = gen_expr(arg_expr);
            auto arg_type = infer_print_type(arg_expr);

            // For identifiers, check variable type
            if (arg_type == PrintArgType::Unknown && arg_expr.is<parser::IdentExpr>()) {
                const auto& ident = arg_expr.as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    if (it->second.type == "i1") arg_type = PrintArgType::Bool;
                    else if (it->second.type == "i32") arg_type = PrintArgType::Int;
                    else if (it->second.type == "i64") arg_type = PrintArgType::I64;
                    else if (it->second.type == "float" || it->second.type == "double") arg_type = PrintArgType::Float;
                    else if (it->second.type == "ptr") arg_type = PrintArgType::Str;
                }
            }

            // For string constants
            if (arg_val.starts_with("@.str.")) {
                arg_type = PrintArgType::Str;
            }

            result = fresh_reg();
            switch (arg_type) {
                case PrintArgType::Str:
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + arg_val + ")");
                    break;
                case PrintArgType::Bool: {
                    std::string label_true = fresh_label("fmt.true");
                    std::string label_false = fresh_label("fmt.false");
                    std::string label_end = fresh_label("fmt.end");

                    emit_line("  br i1 " + arg_val + ", label %" + label_true + ", label %" + label_false);

                    emit_line(label_true + ":");
                    std::string r1 = fresh_reg();
                    emit_line("  " + r1 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.true)");
                    emit_line("  br label %" + label_end);

                    emit_line(label_false + ":");
                    std::string r2 = fresh_reg();
                    emit_line("  " + r2 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.false)");
                    emit_line("  br label %" + label_end);

                    emit_line(label_end + ":");
                    block_terminated_ = false;
                    break;
                }
                case PrintArgType::I64:
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.i64.no_nl, i64 " + arg_val + ")");
                    break;
                case PrintArgType::Float: {
                    // Check if already double (from variable type or last_expr_type_)
                    bool is_double = (last_expr_type_ == "double");
                    if (!is_double && arg_expr.is<parser::IdentExpr>()) {
                        const auto& ident = arg_expr.as<parser::IdentExpr>();
                        auto it = locals_.find(ident.name);
                        if (it != locals_.end() && it->second.type == "double") {
                            is_double = true;
                        }
                    }

                    std::string double_val;
                    if (is_double) {
                        // Already a double, no conversion needed
                        double_val = arg_val;
                    } else {
                        // For printf, floats are promoted to double
                        double_val = fresh_reg();
                        emit_line("  " + double_val + " = fpext float " + arg_val + " to double");
                    }
                    if (precision >= 0) {
                        // Create custom format string for precision
                        std::string fmt_str = "%." + std::to_string(precision) + "f";
                        std::string fmt_const = add_string_literal(fmt_str);
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr " + fmt_const + ", double " + double_val + ")");
                    } else {
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.float.no_nl, double " + double_val + ")");
                    }
                    break;
                }
                case PrintArgType::Int:
                case PrintArgType::Unknown:
                default:
                    // If precision is specified for int, treat as float for fractional display
                    if (precision >= 0) {
                        // Convert i32 to double for fractional display (e.g., us to ms)
                        std::string double_val = fresh_reg();
                        emit_line("  " + double_val + " = sitofp i32 " + arg_val + " to double");
                        std::string fmt_str = "%." + std::to_string(precision) + "f";
                        std::string fmt_const = add_string_literal(fmt_str);
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr " + fmt_const + ", double " + double_val + ")");
                    } else {
                        emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.int.no_nl, i32 " + arg_val + ")");
                    }
                    break;
            }
            ++arg_idx;
        }

        pos = end_brace + 1; // Skip past }
    }

    // Print newline if println
    if (with_newline) {
        result = fresh_reg();
        emit_line("  " + result + " = call i32 @putchar(i32 10)");
    }

    return result;
}

auto LLVMIRGen::gen_array(const parser::ArrayExpr& arr) -> std::string {
    // Array literals create dynamic lists: [1, 2, 3] -> list_create + list_push for each element

    if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
        // [elem1, elem2, elem3, ...]
        const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
        size_t count = elements.size();

        // Create list with initial capacity
        size_t capacity = count > 0 ? count : 4;
        std::string list_ptr = fresh_reg();
        emit_line("  " + list_ptr + " = call ptr @list_create(i32 " + std::to_string(capacity) + ")");

        // Push each element
        for (const auto& elem : elements) {
            std::string val = gen_expr(*elem);
            std::string call_result = fresh_reg();
            emit_line("  " + call_result + " = call i32 @list_push(ptr " + list_ptr + ", i32 " + val + ")");
        }

        return list_ptr;
    } else {
        // [expr; count] - repeat expression count times
        const auto& pair = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);
        std::string init_val = gen_expr(*pair.first);
        std::string count_val = gen_expr(*pair.second);

        // Create list with capacity from count
        std::string list_ptr = fresh_reg();
        emit_line("  " + list_ptr + " = call ptr @list_create(i32 " + count_val + ")");

        // Loop to push init_val count times
        std::string label_cond = fresh_label("arr.cond");
        std::string label_body = fresh_label("arr.body");
        std::string label_end = fresh_label("arr.end");

        // Counter alloca
        std::string counter_ptr = fresh_reg();
        emit_line("  " + counter_ptr + " = alloca i32");
        emit_line("  store i32 0, ptr " + counter_ptr);

        emit_line("  br label %" + label_cond);
        emit_line(label_cond + ":");

        std::string counter_val = fresh_reg();
        emit_line("  " + counter_val + " = load i32, ptr " + counter_ptr);
        std::string cmp = fresh_reg();
        emit_line("  " + cmp + " = icmp slt i32 " + counter_val + ", " + count_val);
        emit_line("  br i1 " + cmp + ", label %" + label_body + ", label %" + label_end);

        emit_line(label_body + ":");
        std::string push_result = fresh_reg();
        emit_line("  " + push_result + " = call i32 @list_push(ptr " + list_ptr + ", i32 " + init_val + ")");

        std::string next_counter = fresh_reg();
        emit_line("  " + next_counter + " = add nsw i32 " + counter_val + ", 1");
        emit_line("  store i32 " + next_counter + ", ptr " + counter_ptr);
        emit_line("  br label %" + label_cond);

        emit_line(label_end + ":");
        block_terminated_ = false;

        return list_ptr;
    }
}

auto LLVMIRGen::gen_index(const parser::IndexExpr& idx) -> std::string {
    // arr[i] -> list_get(arr, i)
    std::string arr_ptr = gen_expr(*idx.object);
    std::string index_val = gen_expr(*idx.index);

    std::string result = fresh_reg();
    emit_line("  " + result + " = call i32 @list_get(ptr " + arr_ptr + ", i32 " + index_val + ")");

    return result;
}

auto LLVMIRGen::gen_path(const parser::PathExpr& path) -> std::string {
    // Path expressions like Color::Red resolve to enum variant values
    // Join path segments with ::
    std::string full_path;
    for (size_t i = 0; i < path.path.segments.size(); ++i) {
        if (i > 0) full_path += "::";
        full_path += path.path.segments[i];
    }

    // Look up in enum variants
    auto it = enum_variants_.find(full_path);
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
        emit_line("  " + tag_ptr + " = getelementptr " + struct_type + ", ptr " + alloca_reg + ", i32 0, i32 0");

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

auto LLVMIRGen::gen_method_call(const parser::MethodCallExpr& call) -> std::string {
    const std::string& method = call.method;

    // Check for static method calls on type names (e.g., List.new(16), HashMap.default())
    // These occur when receiver is an IdentExpr or PathExpr that names a type, not a variable

    // Extract type name from either IdentExpr or PathExpr (for generic types like List[I32])
    std::string type_name;
    bool has_type_name = false;

    if (call.receiver->is<parser::IdentExpr>()) {
        type_name = call.receiver->as<parser::IdentExpr>().name;
        has_type_name = true;
    } else if (call.receiver->is<parser::PathExpr>()) {
        const auto& path_expr = call.receiver->as<parser::PathExpr>();
        if (path_expr.path.segments.size() == 1) {
            type_name = path_expr.path.segments[0];
            has_type_name = true;
        }
    }

    if (has_type_name) {
        // Check if this is a known struct type (not a variable)
        bool is_type_name = struct_types_.count(type_name) > 0 ||
                           type_name == "List" || type_name == "HashMap" || type_name == "Buffer";

        // Also check it's not a local variable
        if (is_type_name && locals_.count(type_name) == 0) {
            // This is a static method call on a type

            // List static methods
            if (type_name == "List") {
                if (method == "new") {
                    std::string cap = call.args.empty() ? "8" : gen_expr(*call.args[0]);
                    // Convert i32 to i64 if needed
                    std::string cap_i64 = fresh_reg();
                    emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @list_create(i64 " + cap_i64 + ")");
                    last_expr_type_ = "ptr";
                    return result;
                }
                if (method == "default") {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @list_create(i64 8)");
                    last_expr_type_ = "ptr";
                    return result;
                }
            }

            // HashMap static methods
            if (type_name == "HashMap") {
                if (method == "new") {
                    std::string cap = call.args.empty() ? "16" : gen_expr(*call.args[0]);
                    std::string cap_i64 = fresh_reg();
                    emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @hashmap_create(i64 " + cap_i64 + ")");
                    last_expr_type_ = "ptr";
                    return result;
                }
                if (method == "default") {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @hashmap_create(i64 16)");
                    last_expr_type_ = "ptr";
                    return result;
                }
            }

            // Buffer static methods
            if (type_name == "Buffer") {
                if (method == "new") {
                    std::string cap = call.args.empty() ? "64" : gen_expr(*call.args[0]);
                    std::string cap_i64 = fresh_reg();
                    emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @buffer_create(i64 " + cap_i64 + ")");
                    last_expr_type_ = "ptr";
                    return result;
                }
                if (method == "default") {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @buffer_create(i64 64)");
                    last_expr_type_ = "ptr";
                    return result;
                }
            }

            // Unknown static method - report error
            report_error("Unknown static method: " + type_name + "." + method, call.span);
            return "0";
        }
    }

    // Generate receiver (the object the method is called on)
    std::string receiver = gen_expr(*call.receiver);

    // Handle Ptr[T] methods
    types::TypePtr receiver_type = infer_expr_type(*call.receiver);
    if (receiver_type && receiver_type->is<types::PtrType>()) {
        const auto& ptr_type = receiver_type->as<types::PtrType>();
        std::string inner_llvm_type = llvm_type_from_semantic(ptr_type.inner);

        // .read() -> T - dereference pointer
        if (method == "read") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + receiver);
            last_expr_type_ = inner_llvm_type;
            return result;
        }

        // .write(value: T) -> Unit - write value to pointer
        if (method == "write") {
            if (call.args.empty()) {
                report_error("Ptr.write() requires a value argument", call.span);
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  store " + inner_llvm_type + " " + val + ", ptr " + receiver);
            return "void";
        }

        // .offset(n: I64) -> Ptr[T] - pointer arithmetic
        if (method == "offset") {
            if (call.args.empty()) {
                report_error("Ptr.offset() requires an offset argument", call.span);
                return receiver;
            }
            std::string offset = gen_expr(*call.args[0]);
            // Convert offset to i64 (getelementptr requires i64 index)
            std::string offset_i64 = fresh_reg();
            emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + inner_llvm_type + ", ptr " + receiver + ", i64 " + offset_i64);
            last_expr_type_ = "ptr";
            return result;
        }

        // .is_null() -> Bool - check if pointer is null
        if (method == "is_null") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp eq ptr " + receiver + ", null");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // Determine receiver type name for type-aware method dispatch
    std::string receiver_type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        receiver_type_name = receiver_type->as<types::NamedType>().name;
    }

    // Type-aware len method - calls different runtime functions based on type
    if (method == "len" || method == "length") {
        if (receiver_type_name == "HashMap") {
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @hashmap_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        if (receiver_type_name == "Buffer") {
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @buffer_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        // Default: List
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_len(ptr " + receiver + ")");
        return result;
    }

    // List methods
    if (method == "push") {
        if (call.args.empty()) {
            report_error("push requires an argument", call.span);
            return "0";
        }
        std::string val = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_push(ptr " + receiver + ", i32 " + val + ")");
        return result;
    }
    if (method == "pop") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_pop(ptr " + receiver + ")");
        return result;
    }
    // Type-aware get method - different behavior for List vs HashMap
    if (method == "get") {
        if (call.args.empty()) {
            report_error("get requires an argument", call.span);
            return "0";
        }
        std::string arg = gen_expr(*call.args[0]);

        if (receiver_type_name == "HashMap") {
            // HashMap: get(key: I64) -> I64
            std::string key_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + arg + " to i64");
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @hashmap_get(ptr " + receiver + ", i64 " + key_i64 + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        // Default: List - get(index: I32) -> I32
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_get(ptr " + receiver + ", i32 " + arg + ")");
        return result;
    }
    // Type-aware set method
    if (method == "set") {
        if (call.args.size() < 2) {
            report_error("set requires two arguments", call.span);
            return "void";
        }
        std::string arg1 = gen_expr(*call.args[0]);
        std::string arg2 = gen_expr(*call.args[1]);

        if (receiver_type_name == "HashMap") {
            // HashMap: set(key: I64, value: I64) -> Unit
            std::string key_i64 = fresh_reg();
            std::string val_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + arg1 + " to i64");
            emit_line("  " + val_i64 + " = sext i32 " + arg2 + " to i64");
            emit_line("  call void @hashmap_set(ptr " + receiver + ", i64 " + key_i64 + ", i64 " + val_i64 + ")");
            return "void";
        }
        // Default: List - set(index: I32, value: I32) -> Unit
        emit_line("  call void @list_set(ptr " + receiver + ", i32 " + arg1 + ", i32 " + arg2 + ")");
        return "void";
    }
    if (method == "clear") {
        emit_line("  call void @list_clear(ptr " + receiver + ")");
        return "void";
    }
    if (method == "is_empty" || method == "isEmpty") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @list_is_empty(ptr " + receiver + ")");
        return result;
    }
    if (method == "capacity") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_capacity(ptr " + receiver + ")");
        return result;
    }

    // HashMap-specific methods (has, remove)
    // NOTE: get/set are handled above with type-aware dispatching
    if (method == "has" || method == "contains") {
        if (call.args.empty()) {
            report_error("has requires a key argument", call.span);
            return "0";
        }
        std::string key = gen_expr(*call.args[0]);
        std::string key_i64 = fresh_reg();
        emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @hashmap_has(ptr " + receiver + ", i64 " + key_i64 + ")");
        return result;
    }
    if (method == "remove") {
        if (call.args.empty()) {
            report_error("remove requires a key argument", call.span);
            return "0";
        }
        std::string key = gen_expr(*call.args[0]);
        std::string key_i64 = fresh_reg();
        emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @hashmap_remove(ptr " + receiver + ", i64 " + key_i64 + ")");
        return result;
    }

    // Buffer methods
    if (method == "write_byte") {
        if (call.args.empty()) {
            report_error("write_byte requires a value argument", call.span);
            return "0";
        }
        std::string val = gen_expr(*call.args[0]);
        emit_line("  call void @buffer_write_byte(ptr " + receiver + ", i32 " + val + ")");
        return "void";
    }
    if (method == "read_byte") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_read_byte(ptr " + receiver + ")");
        return result;
    }
    if (method == "remaining") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_remaining(ptr " + receiver + ")");
        return result;
    }
    if (method == "write_i32") {
        if (call.args.empty()) {
            report_error("write_i32 requires a value argument", call.span);
            return "void";
        }
        std::string val = gen_expr(*call.args[0]);
        emit_line("  call void @buffer_write_i32(ptr " + receiver + ", i32 " + val + ")");
        return "void";
    }
    if (method == "write_i64") {
        if (call.args.empty()) {
            report_error("write_i64 requires a value argument", call.span);
            return "void";
        }
        std::string val = gen_expr(*call.args[0]);
        // Convert i32 to i64 if needed
        std::string val_i64 = fresh_reg();
        emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
        emit_line("  call void @buffer_write_i64(ptr " + receiver + ", i64 " + val_i64 + ")");
        return "void";
    }
    if (method == "read_i32") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_read_i32(ptr " + receiver + ")");
        return result;
    }
    if (method == "read_i64") {
        std::string result_i64 = fresh_reg();
        emit_line("  " + result_i64 + " = call i64 @buffer_read_i64(ptr " + receiver + ")");
        // Truncate to i32 for now
        std::string result = fresh_reg();
        emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
        return result;
    }
    if (method == "reset_read") {
        emit_line("  call void @buffer_reset_read(ptr " + receiver + ")");
        return "void";
    }
    if (method == "destroy") {
        // Type-aware destroy - different for List, HashMap, Buffer
        if (receiver_type_name == "HashMap") {
            emit_line("  call void @hashmap_destroy(ptr " + receiver + ")");
        } else if (receiver_type_name == "Buffer") {
            emit_line("  call void @buffer_destroy(ptr " + receiver + ")");
        } else {
            // Default: List
            emit_line("  call void @list_destroy(ptr " + receiver + ")");
        }
        return "void";
    }

    // Try to find impl method using type inference
    // receiver_type already computed above for Ptr handling
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        std::string qualified_name = named.name + "::" + method;
        auto func_sig = env_.lookup_func(qualified_name);
        if (func_sig) {
            // Generate call to impl method: @tml_TypeName_MethodName(this_ptr, args...)
            std::string fn_name = "@tml_" + named.name + "_" + method;

            // Get receiver pointer (not the loaded value)
            // For identifiers, use the alloca directly
            std::string receiver_ptr;
            if (call.receiver->is<parser::IdentExpr>()) {
                const auto& ident = call.receiver->as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    // If the variable stores a pointer (like 'this' parameter),
                    // we need to load it to get the actual pointer value
                    if (it->second.type == "ptr") {
                        receiver_ptr = receiver;  // Use the loaded value
                    } else {
                        receiver_ptr = it->second.reg;  // Use alloca pointer directly
                    }
                } else {
                    receiver_ptr = receiver;  // Fall back to generated value
                }
            } else {
                // For other expressions, receiver is already a pointer
                receiver_ptr = receiver;
            }

            // Build argument list: self (receiver ptr) + args
            std::vector<std::pair<std::string, std::string>> typed_args;
            typed_args.push_back({"ptr", receiver_ptr});  // self reference

            for (const auto& arg : call.args) {
                std::string val = gen_expr(*arg);
                // Infer arg type from func signature
                std::string arg_type = "i32";  // default
                typed_args.push_back({arg_type, val});
            }

            // Get return type
            std::string ret_type = llvm_type_from_semantic(func_sig->return_type);

            // Build call
            std::string args_str;
            for (size_t i = 0; i < typed_args.size(); ++i) {
                if (i > 0) args_str += ", ";
                args_str += typed_args[i].first + " " + typed_args[i].second;
            }

            std::string result = fresh_reg();
            if (ret_type == "void") {
                emit_line("  call void " + fn_name + "(" + args_str + ")");
                return "void";
            } else {
                emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str + ")");
                return result;
            }
        }
    }

    // Handle dyn dispatch: call method through vtable
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& ident = call.receiver->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end() && it->second.type.starts_with("%dyn.")) {
            std::string dyn_type = it->second.type;
            std::string behavior_name = dyn_type.substr(5);  // Skip "%dyn."
            std::string dyn_ptr = it->second.reg;

            // Get method index in vtable
            auto behavior_methods_it = behavior_method_order_.find(behavior_name);
            if (behavior_methods_it != behavior_method_order_.end()) {
                const auto& methods = behavior_methods_it->second;
                int method_idx = -1;
                for (size_t i = 0; i < methods.size(); ++i) {
                    if (methods[i] == method) {
                        method_idx = static_cast<int>(i);
                        break;
                    }
                }

                if (method_idx >= 0) {
                    // Load data pointer from fat pointer (field 0)
                    std::string data_field = fresh_reg();
                    emit_line("  " + data_field + " = getelementptr " + dyn_type + ", ptr " + dyn_ptr + ", i32 0, i32 0");
                    std::string data_ptr = fresh_reg();
                    emit_line("  " + data_ptr + " = load ptr, ptr " + data_field);

                    // Load vtable pointer from fat pointer (field 1)
                    std::string vtable_field = fresh_reg();
                    emit_line("  " + vtable_field + " = getelementptr " + dyn_type + ", ptr " + dyn_ptr + ", i32 0, i32 1");
                    std::string vtable_ptr = fresh_reg();
                    emit_line("  " + vtable_ptr + " = load ptr, ptr " + vtable_field);

                    // Get function pointer from vtable
                    std::string fn_ptr_loc = fresh_reg();
                    emit_line("  " + fn_ptr_loc + " = getelementptr { ptr }, ptr " + vtable_ptr + ", i32 0, i32 " + std::to_string(method_idx));
                    std::string fn_ptr = fresh_reg();
                    emit_line("  " + fn_ptr + " = load ptr, ptr " + fn_ptr_loc);

                    // Call through function pointer (assumes method takes self and returns i32)
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call i32 " + fn_ptr + "(ptr " + data_ptr + ")");
                    return result;
                }
            }
        }
    }

    report_error("Unknown method: " + method, call.span);
    return "0";
}

} // namespace tml::codegen