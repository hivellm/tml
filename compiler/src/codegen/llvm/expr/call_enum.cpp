TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Enum Constructor Code Generation
//!
//! This file implements enum constructor dispatch for the LLVM IR backend.
//! It handles two call syntaxes:
//!
//! - **PathExpr** style: `Option::Some(42)`, `Outcome::Ok(v)` — type and variant separated by `::`
//! - **IdentExpr** style: `Just(42)`, `Nothing` — bare variant name (must be unambiguous)
//!
//! Both paths handle generic enums (resolved via `pending_generic_enums_`) and
//! non-generic enums (resolved via the type environment). The nullable-pointer
//! optimisation for `Maybe[ptr]` is applied in both paths.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>

namespace tml::codegen {

/// Generate LLVM IR for an enum constructor call.
///
/// Handles both `PathExpr` calls (`Enum::Variant(args)`) and bare `IdentExpr`
/// calls (`Variant(args)`). Returns the SSA register holding the constructed
/// enum value, or `std::nullopt` if the call is not an enum constructor.
auto LLVMIRGen::gen_call_enum_constructor(const parser::CallExpr& call, const std::string& fn_name)
    -> std::optional<std::string> {
    // ============ PATH-BASED ENUM CONSTRUCTORS ============
    // e.g. Option::Some(42), Outcome::Ok(v)

    // Check if this is an enum constructor via PathExpr (e.g., Option::Some(42))
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path_expr = call.callee->as<parser::PathExpr>();
        const auto& segments = path_expr.path.segments;
        if (segments.size() == 2) {
            const std::string& enum_name = segments[0];
            const std::string& variant_name = segments[1];

            // First check pending generic enums
            auto gen_enum_it = pending_generic_enums_.find(enum_name);
            if (gen_enum_it != pending_generic_enums_.end()) {
                const auto& gen_enum_decl = *gen_enum_it->second;
                for (size_t variant_idx = 0; variant_idx < gen_enum_decl.variants.size();
                     ++variant_idx) {
                    const auto& variant = gen_enum_decl.variants[variant_idx];
                    if (variant.name == variant_name) {
                        // Found generic enum constructor via PathExpr
                        std::string enum_type;

                        // Check if variant has payload
                        bool has_payload =
                            variant.tuple_fields.has_value() && !variant.tuple_fields->empty();

                        // If we have expected type from context, use it
                        if (!expected_enum_type_.empty()) {
                            enum_type = expected_enum_type_;
                        } else if (!current_ret_type_.empty() && current_ret_type_ == "ptr" &&
                                   enum_name == "Maybe") {
                            // Nullable Maybe: ret type is bare ptr
                            enum_type = "ptr";
                        } else if (!current_ret_type_.empty() &&
                                   current_ret_type_.find("%struct." + enum_name + "__") == 0) {
                            enum_type = current_ret_type_;
                        } else if (!closure_return_type_.empty() &&
                                   closure_return_type_.find("%struct." + enum_name + "__") == 0) {
                            // Inside inline closure evaluation: use the closure's return type
                            enum_type = closure_return_type_;
                        } else {
                            // Infer type args by matching variant field types
                            // against inferred argument types. Uses the full
                            // infer_expr_type result from the argument expression.
                            std::vector<types::TypePtr> inferred_type_args;
                            // First, get inferred types for all args
                            std::vector<types::TypePtr> arg_types;
                            for (size_t ai = 0; ai < call.args.size(); ++ai) {
                                arg_types.push_back(infer_expr_type(*call.args[ai]));
                            }
                            for (size_t g = 0; g < gen_enum_decl.generics.size(); ++g) {
                                const std::string& gname = gen_enum_decl.generics[g].name;
                                types::TypePtr inferred = nullptr;
                                if (has_payload && variant.tuple_fields.has_value()) {
                                    for (size_t fi = 0;
                                         fi < variant.tuple_fields->size() && fi < call.args.size();
                                         ++fi) {
                                        const auto& ftype = (*variant.tuple_fields)[fi];
                                        // Try direct extraction from inferred type
                                        auto extracted =
                                            extract_generic_from_type(ftype, gname, arg_types[fi]);
                                        if (extracted) {
                                            inferred = extracted;
                                            break;
                                        }
                                        // Fallback: unwrap constructor calls.
                                        // For Heap::new(X), look at X's type to
                                        // match field's inner generic args.
                                        if (ftype && ftype->is<parser::NamedType>() &&
                                            call.args[fi]->is<parser::CallExpr>()) {
                                            const auto& fn = ftype->as<parser::NamedType>();
                                            if (fn.generics.has_value()) {
                                                const auto& inner_call =
                                                    call.args[fi]->as<parser::CallExpr>();
                                                if (inner_call.callee &&
                                                    inner_call.callee->is<parser::PathExpr>()) {
                                                    const auto& ip =
                                                        inner_call.callee->as<parser::PathExpr>();
                                                    if (ip.path.segments.size() == 2 &&
                                                        !fn.path.segments.empty() &&
                                                        fn.path.segments.back() ==
                                                            ip.path.segments[0]) {
                                                        // Match: field is Foo[...] and
                                                        // arg is Foo::method(...)
                                                        const auto& gas = fn.generics->args;
                                                        for (size_t gi = 0;
                                                             gi < gas.size() &&
                                                             gi < inner_call.args.size();
                                                             ++gi) {
                                                            if (gas[gi].is_type()) {
                                                                const auto& inf =
                                                                    std::get<parser::TypePtr>(
                                                                        gas[gi].value);
                                                                auto inner_t = infer_expr_type(
                                                                    *inner_call.args[gi]);
                                                                extracted =
                                                                    extract_generic_from_type(
                                                                        inf, gname, inner_t);
                                                                if (extracted)
                                                                    break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            if (extracted) {
                                                inferred = extracted;
                                                break;
                                            }
                                        }
                                    }
                                }
                                if (!inferred) {
                                    inferred = types::make_i32();
                                }
                                inferred_type_args.push_back(inferred);
                            }
                            std::string mangled_name =
                                require_enum_instantiation(enum_name, inferred_type_args);
                            if (nullable_maybe_types_.count(mangled_name))
                                enum_type = "ptr";
                            else
                                enum_type = "%struct." + mangled_name;
                        }

                        // Nullable pointer optimization: Maybe[ptr] → bare ptr
                        // ONLY apply for Maybe, not for Outcome or other enums
                        if (enum_type == "ptr" && enum_name == "Maybe") {
                            if (has_payload && !call.args.empty()) {
                                std::string payload = gen_expr(*call.args[0]);
                                last_expr_type_ = "ptr";
                                return payload;
                            } else {
                                last_expr_type_ = "ptr";
                                return "null";
                            }
                        }

                        std::string result = fresh_reg();
                        std::string enum_val = fresh_reg();

                        // Create enum value on stack
                        emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                        // Set tag (field 0)
                        std::string tag_ptr = fresh_reg();
                        emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type +
                                  ", ptr " + enum_val + ", i32 0, i32 0");
                        emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " +
                                  tag_ptr);

                        // Set payload if present
                        if (has_payload && !call.args.empty()) {
                            // For nested generics like Poll[Outcome[I64, MyError]], propagate
                            // the inner type as expected_enum_type_ before generating the
                            // inner expression, so that Outcome::Ok(42) gets the correct
                            // type args (I64, MyError) instead of defaulting to (I32, I32).
                            std::string saved_expected = expected_enum_type_;
                            if (!enum_type.empty() && enum_type.starts_with("%struct.")) {
                                std::string mangled = enum_type.substr(8); // Skip "%struct."
                                auto sep = mangled.find("__");
                                if (sep != std::string::npos) {
                                    std::string type_arg_str = mangled.substr(sep + 2);
                                    size_t num_type_params = gen_enum_decl.generics.size();
                                    if (num_type_params == 1 &&
                                        type_arg_str.find("__") != std::string::npos) {
                                        // The type arg itself is a generic type - set expected
                                        // type for inner expression
                                        expected_enum_type_ = "%struct." + type_arg_str;
                                    }
                                }
                            }

                            if (call.args.size() == 1) {
                                // Single-arg variant
                                std::string payload = gen_expr(*call.args[0]);
                                expected_enum_type_ = saved_expected;

                                // Mark variable as consumed (moved into enum variant)
                                // to prevent double-free at scope exit
                                if (call.args[0]->is<parser::IdentExpr>()) {
                                    const auto& arg_ident = call.args[0]->as<parser::IdentExpr>();
                                    if (locals_.find(arg_ident.name) != locals_.end()) {
                                        mark_var_consumed(arg_ident.name);
                                    }
                                }

                                // Skip store for Unit payload - "{}" is zero-sized
                                if (last_expr_type_ != "{}") {
                                    std::string payload_ptr = fresh_reg();
                                    emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                              enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                                    // Widen integer type if declared field type is wider than
                                    // inferred (e.g., literal 42 → i32, but field is I64 → i64).
                                    // Only sext when widening; never truncate or cast structs.
                                    auto int_bits = [](const std::string& t) -> int {
                                        if (t.size() > 1 && t[0] == 'i' &&
                                            std::isdigit(static_cast<unsigned char>(t[1])))
                                            return std::stoi(t.substr(1));
                                        return -1;
                                    };
                                    std::string store_type = last_expr_type_;
                                    std::string store_val = payload;
                                    if (has_payload && variant.tuple_fields.has_value() &&
                                        !variant.tuple_fields->empty()) {
                                        std::string decl_type =
                                            llvm_type(*variant.tuple_fields->at(0));
                                        int src_bits = int_bits(store_type);
                                        int dst_bits = int_bits(decl_type);
                                        if (!decl_type.empty() && decl_type != store_type &&
                                            src_bits > 0 && dst_bits > src_bits) {
                                            std::string coerced = fresh_reg();
                                            emit_line("  " + coerced + " = sext " + store_type +
                                                      " " + store_val + " to " + decl_type);
                                            store_val = coerced;
                                            store_type = decl_type;
                                        }
                                    }
                                    std::string payload_typed_ptr = fresh_reg();
                                    emit_line("  " + payload_typed_ptr + " = bitcast ptr " +
                                              payload_ptr + " to ptr");
                                    emit_line("  store " + store_type + " " + store_val + ", ptr " +
                                              payload_typed_ptr);
                                }
                            } else {
                                // Multi-arg variant: store each field into a tuple in the payload
                                std::vector<std::string> arg_vals;
                                std::vector<std::string> arg_types;
                                for (size_t ai = 0; ai < call.args.size(); ++ai) {
                                    arg_vals.push_back(gen_expr(*call.args[ai]));
                                    arg_types.push_back(last_expr_type_);

                                    // Mark variable as consumed (moved into enum variant)
                                    if (call.args[ai]->is<parser::IdentExpr>()) {
                                        const auto& arg_ident =
                                            call.args[ai]->as<parser::IdentExpr>();
                                        if (locals_.find(arg_ident.name) != locals_.end()) {
                                            mark_var_consumed(arg_ident.name);
                                        }
                                    }
                                }
                                expected_enum_type_ = saved_expected;
                                // Build tuple type string: { type0, type1, ... }
                                std::string tuple_type = "{ ";
                                for (size_t ai = 0; ai < arg_types.size(); ++ai) {
                                    if (ai > 0)
                                        tuple_type += ", ";
                                    tuple_type += arg_types[ai];
                                }
                                tuple_type += " }";
                                // GEP to payload area, store each field
                                std::string payload_ptr = fresh_reg();
                                emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                          enum_type + ", ptr " + enum_val + ", i32 0, i32 1");
                                for (size_t ai = 0; ai < arg_vals.size(); ++ai) {
                                    std::string field_ptr = fresh_reg();
                                    emit_line("  " + field_ptr + " = getelementptr inbounds " +
                                              tuple_type + ", ptr " + payload_ptr +
                                              ", i32 0, i32 " + std::to_string(ai));
                                    emit_line("  store " + arg_types[ai] + " " + arg_vals[ai] +
                                              ", ptr " + field_ptr);
                                }
                            }
                        }

                        // Load the complete enum value
                        emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                        last_expr_type_ = enum_type;
                        return result;
                    }
                }
            }

            // Then check non-generic enums (including from imported modules)
            // Helper lambda to generate path-based enum constructor
            auto gen_path_enum_constructor =
                [&](const std::string& enum_name,
                    const types::EnumDef& enum_def) -> std::optional<std::string> {
                for (size_t variant_idx = 0; variant_idx < enum_def.variants.size();
                     ++variant_idx) {
                    const auto& [vname, payload_types] = enum_def.variants[variant_idx];
                    if (vname == variant_name) {
                        std::string enum_type = "%struct." + enum_name;
                        std::string result = fresh_reg();
                        std::string enum_val = fresh_reg();

                        emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                        std::string tag_ptr = fresh_reg();
                        emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type +
                                  ", ptr " + enum_val + ", i32 0, i32 0");
                        emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " +
                                  tag_ptr);

                        if (!payload_types.empty() && !call.args.empty()) {
                            if (call.args.size() == 1) {
                                // Single-arg variant
                                std::string payload = gen_expr(*call.args[0]);

                                // Mark variable as consumed (moved into enum variant)
                                // to prevent double-free at scope exit
                                if (call.args[0]->is<parser::IdentExpr>()) {
                                    const auto& arg_ident = call.args[0]->as<parser::IdentExpr>();
                                    if (locals_.find(arg_ident.name) != locals_.end()) {
                                        mark_var_consumed(arg_ident.name);
                                    }
                                }

                                // Skip store for Unit payload - "{}" is zero-sized
                                if (last_expr_type_ != "{}") {
                                    std::string payload_ptr = fresh_reg();
                                    emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                              enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                                    // Widen integer type if declared field type is wider than
                                    // inferred (e.g., literal 42 → i32, but field is I64 → i64).
                                    // Only sext when widening; never truncate or cast structs.
                                    auto int_bits = [](const std::string& t) -> int {
                                        if (t.size() > 1 && t[0] == 'i' &&
                                            std::isdigit(static_cast<unsigned char>(t[1])))
                                            return std::stoi(t.substr(1));
                                        return -1;
                                    };
                                    std::string store_type = last_expr_type_;
                                    std::string store_val = payload;
                                    if (!payload_types.empty()) {
                                        std::string decl_type =
                                            llvm_type_from_semantic(payload_types[0]);
                                        int src_bits = int_bits(store_type);
                                        int dst_bits = int_bits(decl_type);
                                        if (!decl_type.empty() && decl_type != store_type &&
                                            src_bits > 0 && dst_bits > src_bits) {
                                            std::string coerced = fresh_reg();
                                            emit_line("  " + coerced + " = sext " + store_type +
                                                      " " + store_val + " to " + decl_type);
                                            store_val = coerced;
                                            store_type = decl_type;
                                        }
                                    }
                                    std::string payload_typed_ptr = fresh_reg();
                                    emit_line("  " + payload_typed_ptr + " = bitcast ptr " +
                                              payload_ptr + " to ptr");
                                    emit_line("  store " + store_type + " " + store_val + ", ptr " +
                                              payload_typed_ptr);
                                }
                            } else {
                                // Multi-arg variant: store each field
                                std::vector<std::string> arg_vals;
                                std::vector<std::string> arg_types;
                                for (size_t ai = 0; ai < call.args.size(); ++ai) {
                                    arg_vals.push_back(gen_expr(*call.args[ai]));
                                    arg_types.push_back(last_expr_type_);

                                    // Mark variable as consumed (moved into enum variant)
                                    if (call.args[ai]->is<parser::IdentExpr>()) {
                                        const auto& arg_ident =
                                            call.args[ai]->as<parser::IdentExpr>();
                                        if (locals_.find(arg_ident.name) != locals_.end()) {
                                            mark_var_consumed(arg_ident.name);
                                        }
                                    }
                                }
                                std::string tuple_type = "{ ";
                                for (size_t ai = 0; ai < arg_types.size(); ++ai) {
                                    if (ai > 0)
                                        tuple_type += ", ";
                                    tuple_type += arg_types[ai];
                                }
                                tuple_type += " }";
                                std::string payload_ptr = fresh_reg();
                                emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                          enum_type + ", ptr " + enum_val + ", i32 0, i32 1");
                                for (size_t ai = 0; ai < arg_vals.size(); ++ai) {
                                    std::string field_ptr = fresh_reg();
                                    emit_line("  " + field_ptr + " = getelementptr inbounds " +
                                              tuple_type + ", ptr " + payload_ptr +
                                              ", i32 0, i32 " + std::to_string(ai));
                                    emit_line("  store " + arg_types[ai] + " " + arg_vals[ai] +
                                              ", ptr " + field_ptr);
                                }
                            }
                        }

                        emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                        last_expr_type_ = enum_type;
                        return result;
                    }
                }
                return std::nullopt;
            };

            // First try lookup_enum (handles local and imported enums)
            auto enum_opt = env_.lookup_enum(enum_name);
            if (enum_opt) {
                if (auto result = gen_path_enum_constructor(enum_name, *enum_opt)) {
                    return *result;
                }
            }

            // If not found via lookup_enum, search all modules
            // This handles cases where we're generating code for a module's functions
            // but the enum is defined in that module (not imported to main file).
            // Also check internal_enums for private enums like BorrowState.
            for (const auto& [mod_path, mod] : env_.get_all_modules()) {
                auto enum_it = mod.enums.find(enum_name);
                if (enum_it != mod.enums.end()) {
                    if (auto result = gen_path_enum_constructor(enum_name, enum_it->second)) {
                        return *result;
                    }
                }
                auto internal_it = mod.internal_enums.find(enum_name);
                if (internal_it != mod.internal_enums.end()) {
                    if (auto result = gen_path_enum_constructor(enum_name, internal_it->second)) {
                        return *result;
                    }
                }
            }
        }
    }

    // ============ BARE IDENTIFIER ENUM CONSTRUCTORS ============
    // e.g. Just(42), Nothing, Ok(v)

    // Check if this is an enum constructor via bare IdentExpr (e.g., Some(42))
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& ident = call.callee->as<parser::IdentExpr>();

        // First check pending generic enums
        for (const auto& [gen_enum_name, gen_enum_decl] : pending_generic_enums_) {
            for (size_t variant_idx = 0; variant_idx < gen_enum_decl->variants.size();
                 ++variant_idx) {
                const auto& variant = gen_enum_decl->variants[variant_idx];
                if (variant.name == ident.name) {
                    // Found generic enum constructor
                    std::string enum_type;

                    // Check if variant has payload (tuple_fields for tuple variants like Just(T))
                    bool has_payload =
                        variant.tuple_fields.has_value() && !variant.tuple_fields->empty();

                    // If we have expected type from context, use it (for multi-param generics)
                    if (!expected_enum_type_.empty()) {
                        enum_type = expected_enum_type_;
                    } else if (!current_ret_type_.empty() && current_ret_type_ == "ptr" &&
                               gen_enum_name == "Maybe") {
                        // Nullable pointer optimization: return type is ptr (nullable Maybe)
                        enum_type = "ptr";
                    } else if (!current_ret_type_.empty() &&
                               current_ret_type_.find("%struct." + gen_enum_name + "__") == 0) {
                        // Function returns this generic enum type - use the return type directly
                        // This handles multi-param generics like Outcome[T, E] where we can only
                        // infer T from Ok(value) but need E from context
                        enum_type = current_ret_type_;
                    } else if (!closure_return_type_.empty() && closure_return_type_ == "ptr" &&
                               gen_enum_name == "Maybe") {
                        // Nullable pointer optimization: closure return type is ptr (nullable
                        // Maybe)
                        enum_type = "ptr";
                    } else if (!closure_return_type_.empty() &&
                               closure_return_type_.find("%struct." + gen_enum_name + "__") == 0) {
                        // Inside inline closure evaluation (e.g., Outcome::and_then closure):
                        // use the closure's return type to resolve the full generic enum type
                        enum_type = closure_return_type_;
                    } else {
                        // Infer type args by matching variant field types
                        std::vector<types::TypePtr> inferred_type_args;
                        std::vector<types::TypePtr> arg_types;
                        for (size_t ai = 0; ai < call.args.size(); ++ai) {
                            arg_types.push_back(infer_expr_type(*call.args[ai]));
                        }
                        for (size_t g = 0; g < gen_enum_decl->generics.size(); ++g) {
                            const std::string& gname = gen_enum_decl->generics[g].name;
                            types::TypePtr inferred = nullptr;
                            if (has_payload && variant.tuple_fields.has_value()) {
                                for (size_t fi = 0;
                                     fi < variant.tuple_fields->size() && fi < call.args.size();
                                     ++fi) {
                                    const auto& ftype = (*variant.tuple_fields)[fi];
                                    auto extracted =
                                        extract_generic_from_type(ftype, gname, arg_types[fi]);
                                    if (extracted) {
                                        inferred = extracted;
                                        break;
                                    }
                                    // Fallback: unwrap constructor calls
                                    if (ftype && ftype->is<parser::NamedType>() &&
                                        call.args[fi]->is<parser::CallExpr>()) {
                                        const auto& fn = ftype->as<parser::NamedType>();
                                        if (fn.generics.has_value()) {
                                            const auto& inner_call =
                                                call.args[fi]->as<parser::CallExpr>();
                                            if (inner_call.callee &&
                                                inner_call.callee->is<parser::PathExpr>()) {
                                                const auto& ip =
                                                    inner_call.callee->as<parser::PathExpr>();
                                                if (ip.path.segments.size() == 2 &&
                                                    !fn.path.segments.empty() &&
                                                    fn.path.segments.back() ==
                                                        ip.path.segments[0]) {
                                                    const auto& gas = fn.generics->args;
                                                    for (size_t gi = 0; gi < gas.size() &&
                                                                        gi < inner_call.args.size();
                                                         ++gi) {
                                                        if (gas[gi].is_type()) {
                                                            const auto& inf =
                                                                std::get<parser::TypePtr>(
                                                                    gas[gi].value);
                                                            auto inner_t = infer_expr_type(
                                                                *inner_call.args[gi]);
                                                            extracted = extract_generic_from_type(
                                                                inf, gname, inner_t);
                                                            if (extracted)
                                                                break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        if (extracted) {
                                            inferred = extracted;
                                            break;
                                        }
                                    }
                                }
                            }
                            if (!inferred) {
                                inferred = types::make_i32();
                            }
                            inferred_type_args.push_back(inferred);
                        }
                        std::string mangled_name =
                            require_enum_instantiation(gen_enum_name, inferred_type_args);
                        if (nullable_maybe_types_.count(mangled_name))
                            enum_type = "ptr";
                        else
                            enum_type = "%struct." + mangled_name;
                    }

                    // Nullable pointer optimization: Maybe[ptr] → bare ptr
                    // ONLY apply for Maybe, not for Outcome or other enums
                    if (enum_type == "ptr" && gen_enum_name == "Maybe") {
                        if (has_payload && !call.args.empty()) {
                            std::string payload = gen_expr(*call.args[0]);
                            last_expr_type_ = "ptr";
                            return payload;
                        } else {
                            last_expr_type_ = "ptr";
                            return "null";
                        }
                    }

                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack
                    emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                    // Set tag (field 0)
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                              enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // Set payload if present (stored in field 1, the [N x i8] array)
                    if (has_payload && !call.args.empty()) {
                        // For nested generics like Maybe[Maybe[I32]], we need to compute
                        // the inner type for the payload before generating the inner expression.
                        // expected_enum_type_ might be %struct.Maybe__Maybe__I32, but the
                        // inner Just(42) needs %struct.Maybe__I32 as its expected type.
                        std::string saved_expected = expected_enum_type_;
                        if (!enum_type.empty() && enum_type.starts_with("%struct.")) {
                            // Extract type args from the mangled name
                            std::string mangled = enum_type.substr(8); // Skip "%struct."
                            auto sep = mangled.find("__");
                            if (sep != std::string::npos) {
                                std::string base = mangled.substr(0, sep);
                                std::string type_arg_str = mangled.substr(sep + 2);
                                // For single-type-param generics, the payload is the type arg
                                // Check if this is a single-type-param enum
                                size_t num_type_params = gen_enum_decl->generics.size();
                                if (num_type_params == 1 &&
                                    type_arg_str.find("__") != std::string::npos) {
                                    // The type arg itself is a generic - set expected type for
                                    // inner
                                    expected_enum_type_ = "%struct." + type_arg_str;
                                }
                            }
                        }

                        if (call.args.size() == 1) {
                            // Single-arg variant
                            std::string payload = gen_expr(*call.args[0]);
                            expected_enum_type_ = saved_expected;

                            // Mark variable as consumed (moved into enum variant)
                            // to prevent double-free at scope exit
                            if (call.args[0]->is<parser::IdentExpr>()) {
                                const auto& arg_ident = call.args[0]->as<parser::IdentExpr>();
                                if (locals_.find(arg_ident.name) != locals_.end()) {
                                    mark_var_consumed(arg_ident.name);
                                }
                            }

                            // Skip store for Unit payload - "{}" is zero-sized
                            if (last_expr_type_ != "{}") {
                                std::string payload_ptr = fresh_reg();
                                emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                          enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                                // Widen integer type if declared field type is wider than
                                // inferred (e.g., literal 42 → i32, but field is I64 → i64).
                                // Only sext when widening; never truncate or cast structs.
                                auto int_bits = [](const std::string& t) -> int {
                                    if (t.size() > 1 && t[0] == 'i' &&
                                        std::isdigit(static_cast<unsigned char>(t[1])))
                                        return std::stoi(t.substr(1));
                                    return -1;
                                };
                                std::string store_type = last_expr_type_;
                                std::string store_val = payload;
                                if (has_payload && variant.tuple_fields.has_value() &&
                                    !variant.tuple_fields->empty()) {
                                    std::string decl_type = llvm_type(*variant.tuple_fields->at(0));
                                    int src_bits = int_bits(store_type);
                                    int dst_bits = int_bits(decl_type);
                                    if (!decl_type.empty() && decl_type != store_type &&
                                        src_bits > 0 && dst_bits > src_bits) {
                                        std::string coerced = fresh_reg();
                                        emit_line("  " + coerced + " = sext " + store_type + " " +
                                                  store_val + " to " + decl_type);
                                        store_val = coerced;
                                        store_type = decl_type;
                                    }
                                }
                                std::string payload_typed_ptr = fresh_reg();
                                emit_line("  " + payload_typed_ptr + " = bitcast ptr " +
                                          payload_ptr + " to ptr");
                                emit_line("  store " + store_type + " " + store_val + ", ptr " +
                                          payload_typed_ptr);
                            }
                        } else {
                            // Multi-arg variant: store each field
                            std::vector<std::string> arg_vals;
                            std::vector<std::string> arg_types;
                            for (size_t ai = 0; ai < call.args.size(); ++ai) {
                                arg_vals.push_back(gen_expr(*call.args[ai]));
                                arg_types.push_back(last_expr_type_);

                                // Mark variable as consumed (moved into enum variant)
                                if (call.args[ai]->is<parser::IdentExpr>()) {
                                    const auto& arg_ident = call.args[ai]->as<parser::IdentExpr>();
                                    if (locals_.find(arg_ident.name) != locals_.end()) {
                                        mark_var_consumed(arg_ident.name);
                                    }
                                }
                            }
                            expected_enum_type_ = saved_expected;

                            std::string tuple_type = "{ ";
                            for (size_t ai = 0; ai < arg_types.size(); ++ai) {
                                if (ai > 0)
                                    tuple_type += ", ";
                                tuple_type += arg_types[ai];
                            }
                            tuple_type += " }";
                            std::string payload_ptr = fresh_reg();
                            emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                      enum_type + ", ptr " + enum_val + ", i32 0, i32 1");
                            for (size_t ai = 0; ai < arg_vals.size(); ++ai) {
                                std::string field_ptr = fresh_reg();
                                emit_line("  " + field_ptr + " = getelementptr inbounds " +
                                          tuple_type + ", ptr " + payload_ptr + ", i32 0, i32 " +
                                          std::to_string(ai));
                                emit_line("  store " + arg_types[ai] + " " + arg_vals[ai] +
                                          ", ptr " + field_ptr);
                            }
                        }
                    }

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
        }

        // Then check non-generic enums (including from imported modules)
        // Helper lambda to generate enum constructor
        auto gen_enum_constructor =
            [&](const std::string& enum_name,
                const types::EnumDef& enum_def) -> std::optional<std::string> {
            for (size_t variant_idx = 0; variant_idx < enum_def.variants.size(); ++variant_idx) {
                const auto& [variant_name, payload_types] = enum_def.variants[variant_idx];

                if (variant_name == ident.name) {
                    // Found enum constructor
                    std::string enum_type = "%struct." + enum_name;
                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack
                    emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                    // Set tag (field 0)
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                              enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // Set payload if present (stored in field 1, the [N x i8] array)
                    if (!payload_types.empty() && !call.args.empty()) {
                        if (call.args.size() == 1) {
                            // Single-arg variant
                            std::string payload = gen_expr(*call.args[0]);

                            // Mark variable as consumed (moved into enum variant)
                            // to prevent double-free at scope exit
                            if (call.args[0]->is<parser::IdentExpr>()) {
                                const auto& arg_ident = call.args[0]->as<parser::IdentExpr>();
                                if (locals_.find(arg_ident.name) != locals_.end()) {
                                    mark_var_consumed(arg_ident.name);
                                }
                            }

                            // Skip store for Unit payload - "{}" is zero-sized
                            if (last_expr_type_ != "{}") {
                                std::string payload_ptr = fresh_reg();
                                emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                          enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                                // Widen integer type if declared field type is wider than
                                // inferred (e.g., literal 42 → i32, but field is I64 → i64).
                                // Only sext when widening; never truncate or cast structs.
                                auto int_bits = [](const std::string& t) -> int {
                                    if (t.size() > 1 && t[0] == 'i' &&
                                        std::isdigit(static_cast<unsigned char>(t[1])))
                                        return std::stoi(t.substr(1));
                                    return -1;
                                };
                                std::string store_type = last_expr_type_;
                                std::string store_val = payload;
                                if (!payload_types.empty()) {
                                    std::string decl_type =
                                        llvm_type_from_semantic(payload_types[0]);
                                    int src_bits = int_bits(store_type);
                                    int dst_bits = int_bits(decl_type);
                                    if (!decl_type.empty() && decl_type != store_type &&
                                        src_bits > 0 && dst_bits > src_bits) {
                                        std::string coerced = fresh_reg();
                                        emit_line("  " + coerced + " = sext " + store_type + " " +
                                                  store_val + " to " + decl_type);
                                        store_val = coerced;
                                        store_type = decl_type;
                                    }
                                }
                                std::string payload_typed_ptr = fresh_reg();
                                emit_line("  " + payload_typed_ptr + " = bitcast ptr " +
                                          payload_ptr + " to ptr");
                                emit_line("  store " + store_type + " " + store_val + ", ptr " +
                                          payload_typed_ptr);
                            }
                        } else {
                            // Multi-arg variant: store each field
                            std::vector<std::string> arg_vals;
                            std::vector<std::string> arg_types;
                            for (size_t ai = 0; ai < call.args.size(); ++ai) {
                                arg_vals.push_back(gen_expr(*call.args[ai]));
                                arg_types.push_back(last_expr_type_);

                                // Mark variable as consumed (moved into enum variant)
                                if (call.args[ai]->is<parser::IdentExpr>()) {
                                    const auto& arg_ident = call.args[ai]->as<parser::IdentExpr>();
                                    if (locals_.find(arg_ident.name) != locals_.end()) {
                                        mark_var_consumed(arg_ident.name);
                                    }
                                }
                            }
                            std::string tuple_type = "{ ";
                            for (size_t ai = 0; ai < arg_types.size(); ++ai) {
                                if (ai > 0)
                                    tuple_type += ", ";
                                tuple_type += arg_types[ai];
                            }
                            tuple_type += " }";
                            std::string payload_ptr = fresh_reg();
                            emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                      enum_type + ", ptr " + enum_val + ", i32 0, i32 1");
                            for (size_t ai = 0; ai < arg_vals.size(); ++ai) {
                                std::string field_ptr = fresh_reg();
                                emit_line("  " + field_ptr + " = getelementptr inbounds " +
                                          tuple_type + ", ptr " + payload_ptr + ", i32 0, i32 " +
                                          std::to_string(ai));
                                emit_line("  store " + arg_types[ai] + " " + arg_vals[ai] +
                                          ", ptr " + field_ptr);
                            }
                        }
                    }

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
            return std::nullopt;
        };

        // Check local enums first
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            if (auto result = gen_enum_constructor(enum_name, enum_def)) {
                return *result;
            }
        }

        // Check enums from imported modules
        for (const auto& [mod_path, mod] : env_.get_all_modules()) {
            for (const auto& [enum_name, enum_def] : mod.enums) {
                if (auto result = gen_enum_constructor(enum_name, enum_def)) {
                    return *result;
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
