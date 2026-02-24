TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Enum Declarations
//!
//! This file implements enum declaration and instantiation code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <functional>

namespace tml::codegen {

void LLVMIRGen::gen_enum_decl(const parser::EnumDecl& e) {
    // If enum has generic parameters, defer generation until instantiated
    if (!e.generics.empty()) {
        pending_generic_enums_[e.name] = &e;
        return;
    }

    // Skip builtin enums that are already declared in the runtime
    if (e.name == "Ordering") {
        // Register variant values for builtin enums but don't emit type definition
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
        struct_types_[e.name] = "%struct." + e.name;
        return;
    }

    // Check if already emitted (can happen with re-exports across modules)
    if (struct_types_.find(e.name) != struct_types_.end()) {
        // Type already emitted, but still need to register variants if not done
        // This handles re-exports across modules
        std::string first_variant_key = e.name + "::" + e.variants[0].name;
        if (enum_variants_.find(first_variant_key) == enum_variants_.end()) {
            int tag = 0;
            for (const auto& variant : e.variants) {
                std::string key = e.name + "::" + variant.name;
                enum_variants_[key] = tag++;
            }
        }
        return;
    }

    // Check if this is a @flags enum
    bool is_flags = false;
    std::string flags_underlying_llvm = "i32";
    for (const auto& deco : e.decorators) {
        if (deco.name == "flags") {
            is_flags = true;
            if (!deco.args.empty() && deco.args[0]->is<parser::IdentExpr>()) {
                const auto& type_arg = deco.args[0]->as<parser::IdentExpr>().name;
                if (type_arg == "U8")
                    flags_underlying_llvm = "i8";
                else if (type_arg == "U16")
                    flags_underlying_llvm = "i16";
                else if (type_arg == "U64")
                    flags_underlying_llvm = "i64";
            }
            break;
        }
    }

    if (is_flags) {
        // @flags enums are raw unsigned integers, NOT struct { i32 }
        std::string type_name = "%struct." + e.name;
        type_defs_buffer_ << "; @flags enum " << e.name << "\n";
        type_defs_buffer_ << type_name << " = type { " << flags_underlying_llvm << " }\n";
        struct_types_[e.name] = type_name;

        // Register variant values with power-of-2 assignment
        FlagsEnumInfo info;
        info.underlying_llvm_type = flags_underlying_llvm;
        info.all_bits_mask = 0;
        uint64_t next_power = 1;

        for (const auto& variant : e.variants) {
            uint64_t value;
            if (variant.discriminant) {
                // Evaluate integer literal discriminant
                if ((*variant.discriminant)->is<parser::LiteralExpr>()) {
                    const auto& lit = (*variant.discriminant)->as<parser::LiteralExpr>();
                    value = std::stoull(std::string(lit.token.lexeme));
                } else {
                    value = next_power;
                    next_power <<= 1;
                }
            } else {
                value = next_power;
                next_power <<= 1;
            }

            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = static_cast<int>(value);
            info.variant_values.emplace_back(variant.name, value);
            info.all_bits_mask |= value;
        }

        flags_enums_[e.name] = info;

        // Generate built-in methods for @flags
        gen_flags_enum_methods(e, info);

        // Generate @derive support (Debug, Display, PartialEq are auto-derived)
        gen_derive_debug_enum(e);
        gen_derive_display_enum(e);
        return;
    }

    // TML enums are represented as tagged unions
    // For now, simple enums are just integers (tag only)
    // Complex enums with data would need { i32, union_data }

    bool has_data = false;
    for (const auto& variant : e.variants) {
        if (variant.tuple_fields.has_value() || variant.struct_fields.has_value()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        // Simple enum - represented as struct with single i32 tag field
        // Emit to type_defs_buffer_ (ensures types before functions)
        std::string type_name = "%struct." + e.name;
        type_defs_buffer_ << type_name << " = type { i32 }\n";
        struct_types_[e.name] = type_name;

        // Register variant values
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    } else {
        // Complex enum with data - create tagged union
        // Find the largest variant

        // Helper lambda to calculate size of an LLVM type (same as gen_enum_instantiation)
        std::function<size_t(const std::string&)> calc_type_size;
        calc_type_size = [this, &calc_type_size](const std::string& ty) -> size_t {
            if (ty == "{}" || ty == "void")
                return 0; // Unit type has zero size
            if (ty == "i8")
                return 1;
            if (ty == "i16")
                return 2;
            if (ty == "i32" || ty == "float" || ty == "i1")
                return 4;
            if (ty == "i64" || ty == "double" || ty == "ptr")
                return 8;
            if (ty == "i128")
                return 16;
            // Check if it's an array type like "[16 x %struct.Ipv4Addr]" or "[4 x i32]"
            if (ty.size() > 2 && ty.front() == '[') {
                // Parse "[N x ElementType]"
                auto x_pos = ty.find(" x ");
                if (x_pos != std::string::npos) {
                    size_t count = std::stoull(ty.substr(1, x_pos - 1));
                    std::string elem_type = ty.substr(x_pos + 3, ty.size() - x_pos - 4);
                    return count * calc_type_size(elem_type);
                }
            }
            // Check if it's an anonymous struct/tuple type like "{ %struct.Layout, i64 }"
            if (ty.starts_with("{ ") && ty.ends_with(" }")) {
                std::string inner = ty.substr(2, ty.size() - 4); // Remove "{ " and " }"
                size_t tuple_size = 0;
                size_t pos = 0;
                while (pos < inner.size()) {
                    // Find the next element (separated by ", ")
                    size_t next_comma = inner.find(", ", pos);
                    std::string elem = (next_comma == std::string::npos)
                                           ? inner.substr(pos)
                                           : inner.substr(pos, next_comma - pos);
                    tuple_size += calc_type_size(elem);
                    if (next_comma == std::string::npos)
                        break;
                    pos = next_comma + 2;
                }
                return tuple_size > 0 ? tuple_size : 8;
            }
            // Check if it's a struct type
            if (ty.starts_with("%struct.")) {
                std::string struct_name = ty.substr(8); // Remove "%struct."
                auto it = struct_fields_.find(struct_name);
                if (it != struct_fields_.end()) {
                    size_t struct_size = 0;
                    for (const auto& field : it->second) {
                        struct_size += calc_type_size(field.llvm_type);
                    }
                    return struct_size > 0 ? struct_size : 8;
                }
            }
            return 8; // Default size
        };

        size_t max_size = 0;
        for (const auto& variant : e.variants) {
            size_t size = 0;
            if (variant.tuple_fields.has_value()) {
                for (const auto& field_type : *variant.tuple_fields) {
                    std::string ty = llvm_type_ptr(field_type);
                    size += calc_type_size(ty);
                }
            }
            if (variant.struct_fields.has_value()) {
                for (const auto& field : *variant.struct_fields) {
                    std::string ty = llvm_type_ptr(field.type);
                    size += calc_type_size(ty);
                }
            }
            max_size = std::max(max_size, size);
        }

        // Emit the enum type to type_defs_buffer_ (ensures types before functions)
        std::string type_name = "%struct." + e.name;

        // Compact layout optimization: use smallest payload type that fits
        if (max_size == 0) {
            // No payload variants — tag only (e.g. Ordering)
            type_defs_buffer_ << type_name << " = type { i32 }\n";
            enum_payload_type_[type_name] = "";
        } else if (max_size <= 4) {
            // Fits in i32 (e.g., Maybe[I32], Maybe[Bool])
            type_defs_buffer_ << type_name << " = type { i32, i32 }\n";
            enum_payload_type_[type_name] = "i32";
        } else if (max_size <= 8) {
            // Fits in i64 (e.g., Maybe[I64], Maybe[F64], enums with ptr payload)
            type_defs_buffer_ << type_name << " = type { i32, i64 }\n";
            enum_payload_type_[type_name] = "i64";
        } else {
            // Large payloads — keep [N x i64] union
            size_t num_i64 = (max_size + 7) / 8;
            type_defs_buffer_ << type_name << " = type { i32, [" << std::to_string(num_i64)
                              << " x i64] }\n";
            enum_payload_type_[type_name] = "";
        }
        struct_types_[e.name] = type_name;

        // Register variant values
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }

    // Generate @derive support if decorated
    gen_derive_reflect_enum(e);
    gen_derive_partial_eq_enum(e);
    gen_derive_duplicate_enum(e);
    gen_derive_hash_enum(e);
    gen_derive_default_enum(e);
    gen_derive_partial_ord_enum(e);
    gen_derive_ord_enum(e);
    gen_derive_debug_enum(e);
    gen_derive_display_enum(e);
    gen_derive_serialize_enum(e);
    gen_derive_deserialize_enum(e);
    gen_derive_fromstr_enum(e);
}

// Generate a specialized version of a generic enum
void LLVMIRGen::gen_enum_instantiation(const parser::EnumDecl& decl,
                                       const std::vector<types::TypePtr>& type_args) {
    // 1. Create substitution map: T -> I32, K -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < decl.generics.size() && i < type_args.size(); ++i) {
        subs[decl.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled name: Maybe[I32] -> Maybe__I32
    std::string mangled = mangle_struct_name(decl.name, type_args);
    std::string type_name = "%struct." + mangled;

    // Check if this type has already been emitted
    if (struct_types_.find(mangled) != struct_types_.end()) {
        // Type already emitted, skip
        return;
    }

    // Check if any variant has data
    bool has_data = false;
    for (const auto& variant : decl.variants) {
        if (variant.tuple_fields.has_value() || variant.struct_fields.has_value()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        // Simple enum - just a tag
        // Emit to type_defs_buffer_ instead of output_ to ensure type is defined before use
        type_defs_buffer_ << type_name << " = type { i32 }\n";
        struct_types_[mangled] = type_name;

        int tag = 0;
        for (const auto& variant : decl.variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    } else {
        // Complex enum with data
        // Calculate max size with substituted types
        // Use for_data=true since these are data fields (Unit -> "{}" not "void")

        // Helper lambda to calculate size of an LLVM type
        std::function<size_t(const std::string&)> calc_type_size;
        calc_type_size = [this, &calc_type_size, &decl](const std::string& ty) -> size_t {
            if (ty == "{}" || ty == "void")
                return 0; // Unit type has zero size
            if (ty == "i8")
                return 1;
            if (ty == "i16")
                return 2;
            if (ty == "i32" || ty == "float" || ty == "i1")
                return 4;
            if (ty == "i64" || ty == "double" || ty == "ptr")
                return 8;
            if (ty == "i128")
                return 16;
            // Check if it's an array type like "[16 x %struct.Ipv4Addr]" or "[4 x i32]"
            if (ty.size() > 2 && ty.front() == '[') {
                // Parse "[N x ElementType]"
                auto x_pos = ty.find(" x ");
                if (x_pos != std::string::npos) {
                    size_t count = std::stoull(ty.substr(1, x_pos - 1));
                    std::string elem_type = ty.substr(x_pos + 3, ty.size() - x_pos - 4);
                    return count * calc_type_size(elem_type);
                }
            }
            // Check if it's an anonymous struct/tuple type like "{ %struct.Layout, i64 }"
            if (ty.starts_with("{ ") && ty.ends_with(" }")) {
                std::string inner = ty.substr(2, ty.size() - 4); // Remove "{ " and " }"
                size_t tuple_size = 0;
                size_t pos = 0;
                while (pos < inner.size()) {
                    // Find the next element (separated by ", ")
                    size_t next_comma = inner.find(", ", pos);
                    std::string elem = (next_comma == std::string::npos)
                                           ? inner.substr(pos)
                                           : inner.substr(pos, next_comma - pos);
                    tuple_size += calc_type_size(elem);
                    if (next_comma == std::string::npos)
                        break;
                    pos = next_comma + 2;
                }
                return tuple_size > 0 ? tuple_size : 8;
            }
            // Check if it's a struct type
            if (ty.starts_with("%struct.")) {
                std::string struct_name = ty.substr(8); // Remove "%struct."
                auto it = struct_fields_.find(struct_name);
                if (it != struct_fields_.end()) {
                    size_t struct_size = 0;
                    for (const auto& field : it->second) {
                        struct_size += calc_type_size(field.llvm_type);
                    }
                    return struct_size > 0 ? struct_size : 8;
                }

                // For enum types (not in struct_fields_), check if it's a known enum instantiation
                // Enums have layout { i32, [N x i64] } = 4 + padding + N*8 bytes
                auto enum_it = enum_instantiations_.find(struct_name);
                if (enum_it != enum_instantiations_.end()) {
                    const auto& inst = enum_it->second;
                    auto decl_it = pending_generic_enums_.find(inst.base_name);
                    if (decl_it != pending_generic_enums_.end()) {
                        // Recursively calculate the inner enum's payload size
                        const parser::EnumDecl* inner_decl = decl_it->second;
                        std::unordered_map<std::string, types::TypePtr> inner_subs;
                        for (size_t i = 0;
                             i < inner_decl->generics.size() && i < inst.type_args.size(); ++i) {
                            inner_subs[inner_decl->generics[i].name] = inst.type_args[i];
                        }

                        size_t inner_max_size = 0;
                        for (const auto& variant : inner_decl->variants) {
                            size_t vsize = 0;
                            if (variant.tuple_fields.has_value()) {
                                for (const auto& field_type : *variant.tuple_fields) {
                                    types::TypePtr resolved =
                                        resolve_parser_type_with_subs(*field_type, inner_subs);
                                    std::string vty = llvm_type_from_semantic(resolved, true);
                                    vsize += calc_type_size(vty);
                                }
                            }
                            inner_max_size = std::max(inner_max_size, vsize);
                        }
                        if (inner_max_size == 0)
                            inner_max_size = 8;
                        size_t inner_num_i64 = (inner_max_size + 7) / 8;
                        // Enum layout: { i32, [N x i64] } with 8-byte alignment
                        // i32 tag (4 bytes) + padding (4 bytes) + N * 8 bytes
                        return 8 + inner_num_i64 * 8;
                    }
                }
            }
            return 8; // Default size
        };

        size_t max_size = 0;
        for (const auto& variant : decl.variants) {
            size_t size = 0;
            if (variant.tuple_fields.has_value()) {
                for (const auto& field_type : *variant.tuple_fields) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*field_type, subs);
                    std::string ty = llvm_type_from_semantic(resolved, true);
                    size += calc_type_size(ty);
                }
            }
            if (variant.struct_fields.has_value()) {
                for (const auto& field : *variant.struct_fields) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*field.type, subs);
                    std::string ty = llvm_type_from_semantic(resolved, true);
                    size += calc_type_size(ty);
                }
            }
            max_size = std::max(max_size, size);
        }

        // Compact layout optimization: use smallest payload type that fits
        // For small payloads (≤8 bytes), use i32 or i64 directly instead of [N x i64].
        // This halves the size of Maybe[I32] from 16 to 8 bytes, matching Rust's layout.
        if (max_size == 0) {
            // No payload variants — tag only
            type_defs_buffer_ << type_name << " = type { i32 }\n";
            enum_payload_type_[type_name] = "";
        } else if (max_size <= 4) {
            // Fits in i32 (e.g., Maybe[I32], Maybe[Bool], Maybe[U8])
            type_defs_buffer_ << type_name << " = type { i32, i32 }\n";
            enum_payload_type_[type_name] = "i32";
        } else if (max_size <= 8) {
            // Fits in i64 (e.g., Maybe[I64], Maybe[F64], Maybe[Str/ptr])
            type_defs_buffer_ << type_name << " = type { i32, i64 }\n";
            enum_payload_type_[type_name] = "i64";
        } else {
            // Large payloads — keep [N x i64] union for proper 8-byte alignment.
            // Use [N x i64] instead of [N x i8] to ensure alignment of the payload data.
            size_t num_i64 = (max_size + 7) / 8;
            type_defs_buffer_ << type_name << " = type { i32, [" << std::to_string(num_i64)
                              << " x i64] }\n";
            enum_payload_type_[type_name] = "";
        }
        struct_types_[mangled] = type_name;

        int tag = 0;
        for (const auto& variant : decl.variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }
}

void LLVMIRGen::gen_flags_enum_methods(const parser::EnumDecl& e, const FlagsEnumInfo& info) {
    std::string type_name = e.name;
    std::string struct_type = "%struct." + type_name;
    const std::string& iN = info.underlying_llvm_type;

    // Suite prefix for test-local types
    std::string suite_prefix;
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    std::string prefix = "tml_" + suite_prefix + type_name + "_";

    // Helper to emit only if not already generated
    auto should_emit = [this](const std::string& name) -> bool {
        if (generated_functions_.count(name) > 0)
            return false;
        generated_functions_.insert(name);
        return true;
    };

    int tc = 0;
    auto t = [&tc]() { return "%t" + std::to_string(tc++); };

    // ── has(self, flag) -> Bool ──
    // Returns (self & flag) != 0
    {
        std::string fn = "@" + prefix + "has";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "; @flags method " << type_name << "::has\n";
            type_defs_buffer_ << "define internal i1 " << fn << "(ptr %self, ptr %flag) {\n";
            type_defs_buffer_ << "entry:\n";
            auto sp = t(), sv = t(), fp = t(), fv = t(), a = t(), r = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";
            type_defs_buffer_ << "  " << fp << " = getelementptr " << struct_type
                              << ", ptr %flag, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << fv << " = load " << iN << ", ptr " << fp << "\n";
            type_defs_buffer_ << "  " << a << " = and " << iN << " " << sv << ", " << fv << "\n";
            type_defs_buffer_ << "  " << r << " = icmp eq " << iN << " " << a << ", " << fv << "\n";
            type_defs_buffer_ << "  ret i1 " << r << "\n";
            type_defs_buffer_ << "}\n\n";
            // Register in functions_ for call resolution
            // Method call dispatch happens in method.cpp, no need to register in functions_
        }
    }

    // ── is_empty(self) -> Bool ──
    {
        std::string fn = "@" + prefix + "is_empty";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal i1 " << fn << "(ptr %self) {\n";
            type_defs_buffer_ << "entry:\n";
            auto sp = t(), sv = t(), r = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";
            type_defs_buffer_ << "  " << r << " = icmp eq " << iN << " " << sv << ", 0\n";
            type_defs_buffer_ << "  ret i1 " << r << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── bits(self) -> underlying integer type ──
    {
        std::string fn = "@" + prefix + "bits";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal " << iN << " " << fn << "(ptr %self) {\n";
            type_defs_buffer_ << "entry:\n";
            auto sp = t(), sv = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";
            type_defs_buffer_ << "  ret " << iN << " " << sv << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── add(self, flag) -> Self ──
    // Returns self | flag
    {
        std::string fn = "@" + prefix + "add";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal " << struct_type << " " << fn
                              << "(ptr %self, ptr %flag) {\n";
            type_defs_buffer_ << "entry:\n";
            auto sp = t(), sv = t(), fp = t(), fv = t(), r = t(), a = t(), sv2 = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";
            type_defs_buffer_ << "  " << fp << " = getelementptr " << struct_type
                              << ", ptr %flag, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << fv << " = load " << iN << ", ptr " << fp << "\n";
            type_defs_buffer_ << "  " << r << " = or " << iN << " " << sv << ", " << fv << "\n";
            // Build result struct
            type_defs_buffer_ << "  " << a << " = alloca " << struct_type << "\n";
            type_defs_buffer_ << "  " << sv2 << " = getelementptr " << struct_type << ", ptr " << a
                              << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store " << iN << " " << r << ", ptr " << sv2 << "\n";
            auto res = t();
            type_defs_buffer_ << "  " << res << " = load " << struct_type << ", ptr " << a << "\n";
            type_defs_buffer_ << "  ret " << struct_type << " " << res << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── remove(self, flag) -> Self ──
    // Returns self & ~flag
    {
        std::string fn = "@" + prefix + "remove";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal " << struct_type << " " << fn
                              << "(ptr %self, ptr %flag) {\n";
            type_defs_buffer_ << "entry:\n";
            auto sp = t(), sv = t(), fp = t(), fv = t(), nf = t(), r = t(), a = t(), p = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";
            type_defs_buffer_ << "  " << fp << " = getelementptr " << struct_type
                              << ", ptr %flag, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << fv << " = load " << iN << ", ptr " << fp << "\n";
            type_defs_buffer_ << "  " << nf << " = xor " << iN << " " << fv << ", -1\n";
            type_defs_buffer_ << "  " << r << " = and " << iN << " " << sv << ", " << nf << "\n";
            type_defs_buffer_ << "  " << a << " = alloca " << struct_type << "\n";
            type_defs_buffer_ << "  " << p << " = getelementptr " << struct_type << ", ptr " << a
                              << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store " << iN << " " << r << ", ptr " << p << "\n";
            auto res = t();
            type_defs_buffer_ << "  " << res << " = load " << struct_type << ", ptr " << a << "\n";
            type_defs_buffer_ << "  ret " << struct_type << " " << res << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── toggle(self, flag) -> Self ──
    // Returns self ^ flag
    {
        std::string fn = "@" + prefix + "toggle";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal " << struct_type << " " << fn
                              << "(ptr %self, ptr %flag) {\n";
            type_defs_buffer_ << "entry:\n";
            auto sp = t(), sv = t(), fp = t(), fv = t(), r = t(), a = t(), p = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";
            type_defs_buffer_ << "  " << fp << " = getelementptr " << struct_type
                              << ", ptr %flag, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << fv << " = load " << iN << ", ptr " << fp << "\n";
            type_defs_buffer_ << "  " << r << " = xor " << iN << " " << sv << ", " << fv << "\n";
            type_defs_buffer_ << "  " << a << " = alloca " << struct_type << "\n";
            type_defs_buffer_ << "  " << p << " = getelementptr " << struct_type << ", ptr " << a
                              << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store " << iN << " " << r << ", ptr " << p << "\n";
            auto res = t();
            type_defs_buffer_ << "  " << res << " = load " << struct_type << ", ptr " << a << "\n";
            type_defs_buffer_ << "  ret " << struct_type << " " << res << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── eq(self, other) -> Bool ──
    // Tag comparison (both are just { iN })
    {
        std::string fn = "@" + prefix + "eq";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "; @flags PartialEq for " << type_name << "\n";
            type_defs_buffer_ << "define internal i1 " << fn << "(ptr %this, ptr %other) {\n";
            type_defs_buffer_ << "entry:\n";
            auto tp = t(), tv = t(), op = t(), ov = t(), r = t();
            type_defs_buffer_ << "  " << tp << " = getelementptr " << struct_type
                              << ", ptr %this, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << tv << " = load " << iN << ", ptr " << tp << "\n";
            type_defs_buffer_ << "  " << op << " = getelementptr " << struct_type
                              << ", ptr %other, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << ov << " = load " << iN << ", ptr " << op << "\n";
            type_defs_buffer_ << "  " << r << " = icmp eq " << iN << " " << tv << ", " << ov
                              << "\n";
            type_defs_buffer_ << "  ret i1 " << r << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── Static methods ──

    // ── none() -> Self ──
    // Returns 0 (empty set)
    {
        std::string fn = "@" + prefix + "none";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal " << struct_type << " " << fn << "() {\n";
            type_defs_buffer_ << "entry:\n";
            auto a = t(), p = t(), res = t();
            type_defs_buffer_ << "  " << a << " = alloca " << struct_type << "\n";
            type_defs_buffer_ << "  " << p << " = getelementptr " << struct_type << ", ptr " << a
                              << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store " << iN << " 0, ptr " << p << "\n";
            type_defs_buffer_ << "  " << res << " = load " << struct_type << ", ptr " << a << "\n";
            type_defs_buffer_ << "  ret " << struct_type << " " << res << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── all() -> Self ──
    // Returns all_bits_mask
    {
        std::string fn = "@" + prefix + "all";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal " << struct_type << " " << fn << "() {\n";
            type_defs_buffer_ << "entry:\n";
            auto a = t(), p = t(), res = t();
            type_defs_buffer_ << "  " << a << " = alloca " << struct_type << "\n";
            type_defs_buffer_ << "  " << p << " = getelementptr " << struct_type << ", ptr " << a
                              << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store " << iN << " " << std::to_string(info.all_bits_mask)
                              << ", ptr " << p << "\n";
            type_defs_buffer_ << "  " << res << " = load " << struct_type << ", ptr " << a << "\n";
            type_defs_buffer_ << "  ret " << struct_type << " " << res << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── from_bits(value: underlying) -> Self ──
    {
        std::string fn = "@" + prefix + "from_bits";
        if (should_emit(fn)) {
            tc = 0;
            type_defs_buffer_ << "define internal " << struct_type << " " << fn << "(" << iN
                              << " %val) {\n";
            type_defs_buffer_ << "entry:\n";
            auto a = t(), p = t(), res = t();
            type_defs_buffer_ << "  " << a << " = alloca " << struct_type << "\n";
            type_defs_buffer_ << "  " << p << " = getelementptr " << struct_type << ", ptr " << a
                              << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store " << iN << " %val, ptr " << p << "\n";
            type_defs_buffer_ << "  " << res << " = load " << struct_type << ", ptr " << a << "\n";
            type_defs_buffer_ << "  ret " << struct_type << " " << res << "\n";
            type_defs_buffer_ << "}\n\n";
        }
    }

    // ── to_string(self) -> Str  (Display) ──
    // Returns pipe-separated variant names, e.g. "Read | Write"
    // Returns "(empty)" if no bits are set
    // Uses alloca to avoid complex phi nodes
    {
        std::string fn = "@" + prefix + "to_string";
        if (should_emit(fn)) {
            // Emit string constants for variant names and separators
            std::string const_prefix = "@.flags_" + suite_prefix + type_name;
            type_defs_buffer_ << "; @flags Display string constants for " << type_name << "\n";
            for (const auto& [vname, vval] : info.variant_values) {
                type_defs_buffer_ << const_prefix << "_v_" << vname << " = private constant ["
                                  << (vname.size() + 1) << " x i8] c\"" << vname << "\\00\"\n";
            }
            type_defs_buffer_ << const_prefix << "_sep = private constant [4 x i8] c\" | \\00\"\n";
            type_defs_buffer_ << const_prefix
                              << "_empty = private constant [8 x i8] c\"(empty)\\00\"\n";
            type_defs_buffer_ << "\n";

            tc = 0;
            type_defs_buffer_ << "; @flags Display for " << type_name << "\n";
            type_defs_buffer_ << "define internal ptr " << fn << "(ptr %self) {\n";
            type_defs_buffer_ << "entry:\n";

            // Load the raw value
            auto sp = t(), sv = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";

            // Check if empty
            auto is_empty_r = t();
            type_defs_buffer_ << "  " << is_empty_r << " = icmp eq " << iN << " " << sv << ", 0\n";
            type_defs_buffer_ << "  br i1 " << is_empty_r << ", label %empty, label %build\n\n";

            // Empty case
            type_defs_buffer_ << "empty:\n";
            type_defs_buffer_ << "  ret ptr " << const_prefix << "_empty\n\n";

            // Build string using alloca to accumulate result
            type_defs_buffer_ << "build:\n";
            auto acc = t();
            type_defs_buffer_ << "  " << acc << " = alloca ptr\n";
            type_defs_buffer_ << "  store ptr null, ptr " << acc << "\n";

            // For each variant, check if its bit is set and concat
            for (size_t i = 0; i < info.variant_values.size(); ++i) {
                const auto& [vname, vval] = info.variant_values[i];
                std::string check_label = "check_v" + std::to_string(i);
                std::string set_label = "set_v" + std::to_string(i);
                std::string next_label = (i + 1 < info.variant_values.size())
                                             ? "check_v" + std::to_string(i + 1)
                                             : "done";

                type_defs_buffer_ << "  br label %" << check_label << "\n\n";
                type_defs_buffer_ << check_label << ":\n";

                // Check if this variant's bit is set
                auto masked = t(), has_bit = t();
                type_defs_buffer_ << "  " << masked << " = and " << iN << " " << sv << ", " << vval
                                  << "\n";
                type_defs_buffer_ << "  " << has_bit << " = icmp ne " << iN << " " << masked
                                  << ", 0\n";
                type_defs_buffer_ << "  br i1 " << has_bit << ", label %" << set_label
                                  << ", label %" << next_label << "\n\n";

                type_defs_buffer_ << set_label << ":\n";
                // Load current accumulated string
                auto cur = t();
                type_defs_buffer_ << "  " << cur << " = load ptr, ptr " << acc << "\n";
                // If current string is null, just use variant name; otherwise concat " | " + name
                auto is_first = t();
                type_defs_buffer_ << "  " << is_first << " = icmp eq ptr " << cur << ", null\n";
                auto with_sep = t();
                type_defs_buffer_ << "  " << with_sep << " = call ptr @str_concat_opt(ptr " << cur
                                  << ", ptr " << const_prefix << "_sep)\n";
                auto base = t();
                type_defs_buffer_ << "  " << base << " = select i1 " << is_first
                                  << ", ptr null, ptr " << with_sep << "\n";
                auto result = t();
                type_defs_buffer_ << "  " << result << " = call ptr @str_concat_opt(ptr " << base
                                  << ", ptr " << const_prefix << "_v_" << vname << ")\n";
                // Store updated string
                type_defs_buffer_ << "  store ptr " << result << ", ptr " << acc << "\n";
                type_defs_buffer_ << "  br label %" << next_label << "\n\n";
            }

            // Done - return accumulated string
            type_defs_buffer_ << "done:\n";
            auto final_val = t();
            type_defs_buffer_ << "  " << final_val << " = load ptr, ptr " << acc << "\n";
            type_defs_buffer_ << "  ret ptr " << final_val << "\n";
            type_defs_buffer_ << "}\n\n";

            allocating_functions_.insert("to_string");
        }
    }

    // ── debug_string(self) -> Str  (Debug) ──
    // Returns "TypeName(Read | Write)" format
    {
        std::string fn = "@" + prefix + "debug_string";
        if (should_emit(fn)) {
            // Emit type name constant
            std::string const_prefix = "@.flags_" + suite_prefix + type_name;
            std::string debug_prefix_str = type_name + "(";
            std::string debug_suffix_str = ")";
            type_defs_buffer_ << const_prefix << "_dbg_prefix = private constant ["
                              << (debug_prefix_str.size() + 1) << " x i8] c\"" << debug_prefix_str
                              << "\\00\"\n";
            type_defs_buffer_ << const_prefix << "_dbg_suffix = private constant ["
                              << (debug_suffix_str.size() + 1) << " x i8] c\"" << debug_suffix_str
                              << "\\00\"\n\n";

            tc = 0;
            type_defs_buffer_ << "; @flags Debug for " << type_name << "\n";
            type_defs_buffer_ << "define internal ptr " << fn << "(ptr %self) {\n";
            type_defs_buffer_ << "entry:\n";

            // Call to_string first to get the display representation
            auto display = t();
            type_defs_buffer_ << "  " << display << " = call ptr @" << prefix
                              << "to_string(ptr %self)\n";

            // Wrap with "TypeName(" ... ")"
            auto with_prefix = t();
            type_defs_buffer_ << "  " << with_prefix << " = call ptr @str_concat_opt(ptr "
                              << const_prefix << "_dbg_prefix, ptr " << display << ")\n";
            auto result = t();
            type_defs_buffer_ << "  " << result << " = call ptr @str_concat_opt(ptr " << with_prefix
                              << ", ptr " << const_prefix << "_dbg_suffix)\n";
            type_defs_buffer_ << "  ret ptr " << result << "\n";
            type_defs_buffer_ << "}\n\n";

            allocating_functions_.insert("debug_string");
        }
    }

    // ── to_json(self) -> Str  (Serialize) ──
    // Returns JSON array of set flag names: ["Read", "Write"]
    // Returns "[]" if no bits are set
    {
        std::string fn = "@" + prefix + "to_json";
        if (should_emit(fn)) {
            std::string const_prefix = "@.flags_" + suite_prefix + type_name;

            // Emit JSON string constants for variant names (quoted)
            type_defs_buffer_ << "; @flags Serialize string constants for " << type_name << "\n";
            for (const auto& [vname, vval] : info.variant_values) {
                // \"Name\" format (with escaped quotes for JSON)
                std::string quoted = "\\22" + vname + "\\22";
                size_t len = vname.size() + 2 + 1; // 2 quotes + null
                type_defs_buffer_ << const_prefix << "_jv_" << vname << " = private constant ["
                                  << len << " x i8] c\"" << quoted << "\\00\"\n";
            }
            type_defs_buffer_ << const_prefix
                              << "_json_open = private constant [2 x i8] c\"[\\00\"\n";
            type_defs_buffer_ << const_prefix
                              << "_json_close = private constant [2 x i8] c\"]\\00\"\n";
            type_defs_buffer_ << const_prefix
                              << "_json_comma = private constant [3 x i8] c\", \\00\"\n";
            type_defs_buffer_ << "\n";

            tc = 0;
            type_defs_buffer_ << "; @flags Serialize for " << type_name << "\n";
            type_defs_buffer_ << "define internal ptr " << fn << "(ptr %self) {\n";
            type_defs_buffer_ << "entry:\n";

            // Load the raw value
            auto sp = t(), sv = t();
            type_defs_buffer_ << "  " << sp << " = getelementptr " << struct_type
                              << ", ptr %self, i32 0, i32 0\n";
            type_defs_buffer_ << "  " << sv << " = load " << iN << ", ptr " << sp << "\n";

            // Start with "["
            auto acc = t();
            type_defs_buffer_ << "  " << acc << " = alloca ptr\n";
            auto open = t();
            type_defs_buffer_ << "  " << open << " = call ptr @str_concat_opt(ptr null, ptr "
                              << const_prefix << "_json_open)\n";
            type_defs_buffer_ << "  store ptr " << open << ", ptr " << acc << "\n";

            // Track if we need comma
            auto need_comma = t();
            type_defs_buffer_ << "  " << need_comma << " = alloca i1\n";
            type_defs_buffer_ << "  store i1 0, ptr " << need_comma << "\n";

            // For each variant, check if its bit is set
            for (size_t i = 0; i < info.variant_values.size(); ++i) {
                const auto& [vname, vval] = info.variant_values[i];
                std::string check_label = "jcheck_v" + std::to_string(i);
                std::string set_label = "jset_v" + std::to_string(i);
                std::string next_label = (i + 1 < info.variant_values.size())
                                             ? "jcheck_v" + std::to_string(i + 1)
                                             : "jdone";

                type_defs_buffer_ << "  br label %" << check_label << "\n\n";
                type_defs_buffer_ << check_label << ":\n";

                auto masked = t(), has_bit = t();
                type_defs_buffer_ << "  " << masked << " = and " << iN << " " << sv << ", " << vval
                                  << "\n";
                type_defs_buffer_ << "  " << has_bit << " = icmp ne " << iN << " " << masked
                                  << ", 0\n";
                type_defs_buffer_ << "  br i1 " << has_bit << ", label %" << set_label
                                  << ", label %" << next_label << "\n\n";

                type_defs_buffer_ << set_label << ":\n";
                // Load current string and comma flag
                auto cur = t();
                type_defs_buffer_ << "  " << cur << " = load ptr, ptr " << acc << "\n";
                auto nc = t();
                type_defs_buffer_ << "  " << nc << " = load i1, ptr " << need_comma << "\n";
                // Add comma if needed
                auto with_comma = t();
                type_defs_buffer_ << "  " << with_comma << " = call ptr @str_concat_opt(ptr " << cur
                                  << ", ptr " << const_prefix << "_json_comma)\n";
                auto base = t();
                type_defs_buffer_ << "  " << base << " = select i1 " << nc << ", ptr " << with_comma
                                  << ", ptr " << cur << "\n";
                // Add quoted variant name
                auto result = t();
                type_defs_buffer_ << "  " << result << " = call ptr @str_concat_opt(ptr " << base
                                  << ", ptr " << const_prefix << "_jv_" << vname << ")\n";
                type_defs_buffer_ << "  store ptr " << result << ", ptr " << acc << "\n";
                type_defs_buffer_ << "  store i1 1, ptr " << need_comma << "\n";
                type_defs_buffer_ << "  br label %" << next_label << "\n\n";
            }

            // Close with "]"
            type_defs_buffer_ << "jdone:\n";
            auto final_val = t();
            type_defs_buffer_ << "  " << final_val << " = load ptr, ptr " << acc << "\n";
            auto closed = t();
            type_defs_buffer_ << "  " << closed << " = call ptr @str_concat_opt(ptr " << final_val
                              << ", ptr " << const_prefix << "_json_close)\n";
            type_defs_buffer_ << "  ret ptr " << closed << "\n";
            type_defs_buffer_ << "}\n\n";

            allocating_functions_.insert("to_json");
        }
    }
}

} // namespace tml::codegen
