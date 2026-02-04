//! # LLVM IR Generator - Enum Declarations
//!
//! This file implements enum declaration and instantiation code generation.

#include "codegen/llvm_ir_gen.hpp"

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

        // Ensure at least 8 bytes for data
        if (max_size == 0)
            max_size = 8;

        // Emit the enum type to type_defs_buffer_ (ensures types before functions)
        std::string type_name = "%struct." + e.name;
        // Use [N x i64] for proper 8-byte alignment (same as gen_enum_instantiation)
        size_t num_i64 = (max_size + 7) / 8;
        type_defs_buffer_ << type_name << " = type { i32, [" << std::to_string(num_i64)
                          << " x i64] }\n";
        struct_types_[e.name] = type_name;

        // Register variant values
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }

    // Generate @derive(Reflect) support if decorated
    gen_derive_reflect_enum(e);
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

        // Ensure at least 8 bytes for data
        if (max_size == 0)
            max_size = 8;

        // Calculate alignment needed for payload (max alignment of any variant)
        // Use [N x i64] instead of [N x i8] to ensure 8-byte alignment of the payload data.
        // This is necessary because payloads may contain i64/double/structs that require
        // 8-byte alignment. Using i8 array would place data at offset 4 after the i32 tag,
        // causing alignment issues.
        //
        // We round up max_size to the nearest multiple of 8, then divide by 8 to get
        // the number of i64 elements needed.
        size_t num_i64 = (max_size + 7) / 8;
        type_defs_buffer_ << type_name << " = type { i32, [" << std::to_string(num_i64)
                          << " x i64] }\n";
        struct_types_[mangled] = type_name;

        int tag = 0;
        for (const auto& variant : decl.variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }
}

} // namespace tml::codegen
