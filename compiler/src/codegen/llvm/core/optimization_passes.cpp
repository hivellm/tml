TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Advanced Optimization Passes
//!
//! This file implements Phase 10.3 (Arena), Phase 11 (SOO), Phase 13 (Cache Layout),
//! and Phase 14 (Monomorphization) optimizations for OOP support.
//!
//! ## Arena Allocation (Phase 10.3)
//!
//! Detects arena allocation contexts and:
//! - Generates bump-pointer allocation
//! - Skips destructor generation for arena-allocated objects
//!
//! ## Small Object Optimization (Phase 11)
//!
//! Calculates type sizes at compile time and:
//! - Identifies types eligible for inline storage
//! - Optimizes Maybe/Outcome with small types
//!
//! ## Cache-Friendly Layout (Phase 13)
//!
//! Optimizes field layout for cache efficiency:
//! - Places hot fields at the start
//! - Minimizes padding through reordering
//! - Aligns frequently-accessed types to cache lines
//!
//! ## Class Monomorphization (Phase 14)
//!
//! Specializes generic functions with sealed class parameters:
//! - Detects devirtualization opportunities
//! - Generates specialized versions with direct calls

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace tml::codegen {

// ============================================================================
// Phase 10.3: Arena Allocation Integration
// ============================================================================

bool LLVMIRGen::is_arena_allocated(const std::string& value_reg) const {
    return arena_allocated_values_.count(value_reg) > 0;
}

auto LLVMIRGen::gen_arena_alloc(const std::string& arena_reg,
                                [[maybe_unused]] const std::string& type_name, size_t size,
                                size_t align) -> std::string {
    // Generate bump-pointer allocation:
    // 1. Load current position from arena
    // 2. Align position
    // 3. Check if enough space
    // 4. Increment position
    // 5. Return pointer

    std::string result = fresh_reg();
    std::string pos_ptr = fresh_reg();
    std::string current_pos = fresh_reg();
    std::string aligned_pos = fresh_reg();
    std::string end_pos = fresh_reg();
    std::string capacity_ptr = fresh_reg();
    std::string capacity = fresh_reg();
    std::string has_space = fresh_reg();

    // Arena layout: { chunks: List, current_chunk: I64, default_chunk_size: I64, stats: ArenaStats
    // } We use alloc_raw method which handles growth internally

    // Call arena.alloc_raw(size, align)
    emit_line("  " + result + " = call ptr @tml_" + get_suite_prefix() + "Arena_alloc_raw(ptr " +
              arena_reg + ", i64 " + std::to_string(size) + ", i64 " + std::to_string(align) + ")");

    // Track this allocation as arena-allocated
    arena_allocated_values_.insert(result);
    arena_alloc_stats_.arena_allocations++;
    arena_alloc_stats_.bump_ptr_ops++;

    return result;
}

// ============================================================================
// Phase 11: Small Object Optimization (SOO)
// ============================================================================

auto LLVMIRGen::calculate_type_size(const std::string& type_name) -> SooTypeInfo {
    // Check cache first
    auto it = type_size_cache_.find(type_name);
    if (it != type_size_cache_.end()) {
        return it->second;
    }

    SooTypeInfo info;
    info.type_name = type_name;
    soo_stats_.types_analyzed++;

    // Handle primitive types
    if (type_name == "I8" || type_name == "U8" || type_name == "Bool") {
        info.computed_size = 1;
        info.alignment = 1;
        info.is_small = true;
        info.has_trivial_dtor = true;
    } else if (type_name == "I16" || type_name == "U16") {
        info.computed_size = 2;
        info.alignment = 2;
        info.is_small = true;
        info.has_trivial_dtor = true;
    } else if (type_name == "I32" || type_name == "U32" || type_name == "F32") {
        info.computed_size = 4;
        info.alignment = 4;
        info.is_small = true;
        info.has_trivial_dtor = true;
    } else if (type_name == "I64" || type_name == "U64" || type_name == "F64") {
        info.computed_size = 8;
        info.alignment = 8;
        info.is_small = true;
        info.has_trivial_dtor = true;
    } else if (type_name == "I128" || type_name == "U128") {
        info.computed_size = 16;
        info.alignment = 16;
        info.is_small = true;
        info.has_trivial_dtor = true;
    }
    // Handle class types
    else if (auto class_def = env_.lookup_class(type_name)) {
        // Calculate size from fields
        size_t offset = 0;
        size_t max_align = 8; // Default alignment

        // If not a value class, add vtable pointer (8 bytes)
        if (!class_def->is_value) {
            offset = 8;
        }

        // Add base class size if any
        if (class_def->base_class) {
            auto base_info = calculate_type_size(*class_def->base_class);
            // Base class is embedded, so add its fields
            // Skip vtable pointer since we have our own
            size_t base_data_size = base_info.computed_size;
            if (!base_info.has_trivial_dtor) {
                info.has_trivial_dtor = false;
            }
            offset += base_data_size;
            if (base_info.alignment > max_align) {
                max_align = base_info.alignment;
            }
        }

        // Add own fields
        for (const auto& field : class_def->fields) {
            if (field.is_static)
                continue;

            // Get field size (simplified - using 8 bytes as default)
            size_t field_size = 8;
            size_t field_align = 8;

            // Align offset for this field
            offset = (offset + field_align - 1) & ~(field_align - 1);
            offset += field_size;

            if (field_align > max_align) {
                max_align = field_align;
            }
        }

        // Final alignment padding
        offset = (offset + max_align - 1) & ~(max_align - 1);

        info.computed_size = offset;
        info.alignment = max_align;
        info.is_small = (offset <= SOO_THRESHOLD);
        info.has_trivial_dtor = !env_.type_needs_drop(type_name);

        if (info.is_small) {
            soo_stats_.small_types++;
        }
    }
    // Handle struct types
    else if (struct_fields_.count(type_name) > 0) {
        const auto& fields = struct_fields_.at(type_name);
        size_t offset = 0;
        size_t max_align = 8;

        for (const auto& field : fields) {
            // Simplified - use 8 bytes for each field
            size_t field_size = 8;
            size_t field_align = 8;

            // Try to get actual size from LLVM type
            if (field.llvm_type == "i8") {
                field_size = 1;
                field_align = 1;
            } else if (field.llvm_type == "i16") {
                field_size = 2;
                field_align = 2;
            } else if (field.llvm_type == "i32" || field.llvm_type == "float") {
                field_size = 4;
                field_align = 4;
            } else if (field.llvm_type == "i64" || field.llvm_type == "double" ||
                       field.llvm_type == "ptr" || field.llvm_type.find("ptr") == 0) {
                field_size = 8;
                field_align = 8;
            } else if (field.llvm_type == "i128") {
                field_size = 16;
                field_align = 16;
            }

            offset = (offset + field_align - 1) & ~(field_align - 1);
            offset += field_size;
            if (field_align > max_align) {
                max_align = field_align;
            }
        }

        offset = (offset + max_align - 1) & ~(max_align - 1);

        info.computed_size = offset;
        info.alignment = max_align;
        info.is_small = (offset <= SOO_THRESHOLD);
        info.has_trivial_dtor = true; // Structs are typically trivial
    } else {
        // Unknown type - assume large and non-trivial
        info.computed_size = 128;
        info.alignment = 8;
        info.is_small = false;
        info.has_trivial_dtor = false;
    }

    type_size_cache_[type_name] = info;
    return info;
}

bool LLVMIRGen::is_soo_eligible(const std::string& type_name) {
    auto info = calculate_type_size(type_name);
    return info.is_small && info.has_trivial_dtor;
}

// ============================================================================
// Phase 13: Cache-Friendly Layout
// ============================================================================

auto LLVMIRGen::optimize_field_layout(const std::string& type_name,
                                      const std::vector<FieldLayoutInfo>& fields)
    -> OptimizedLayout {
    OptimizedLayout result;
    result.fields = fields;

    if (fields.empty()) {
        return result;
    }

    // Step 1: Separate hot and cold fields
    std::vector<FieldLayoutInfo> hot_fields;
    std::vector<FieldLayoutInfo> cold_fields;

    for (const auto& field : fields) {
        if (field.is_hot || field.heat_score > 50) {
            hot_fields.push_back(field);
        } else {
            cold_fields.push_back(field);
        }
    }

    // Step 2: Sort hot fields by alignment (descending) then by heat score
    std::sort(hot_fields.begin(), hot_fields.end(),
              [](const FieldLayoutInfo& a, const FieldLayoutInfo& b) {
                  if (a.alignment != b.alignment) {
                      return a.alignment > b.alignment;
                  }
                  return a.heat_score > b.heat_score;
              });

    // Step 3: Sort cold fields by size (descending) to minimize padding
    std::sort(cold_fields.begin(), cold_fields.end(),
              [](const FieldLayoutInfo& a, const FieldLayoutInfo& b) {
                  if (a.alignment != b.alignment) {
                      return a.alignment > b.alignment;
                  }
                  return a.size > b.size;
              });

    // Step 4: Merge - hot fields first, then cold fields
    result.fields.clear();
    result.fields.insert(result.fields.end(), hot_fields.begin(), hot_fields.end());
    result.fields.insert(result.fields.end(), cold_fields.begin(), cold_fields.end());

    // Step 5: Calculate total size and padding
    size_t offset = 0;
    size_t max_align = 1;
    size_t padding = 0;

    for (const auto& field : result.fields) {
        // Calculate padding needed for alignment
        size_t aligned_offset = (offset + field.alignment - 1) & ~(field.alignment - 1);
        padding += (aligned_offset - offset);
        offset = aligned_offset + field.size;
        if (field.alignment > max_align) {
            max_align = field.alignment;
        }
    }

    // Final alignment padding
    size_t final_offset = (offset + max_align - 1) & ~(max_align - 1);
    padding += (final_offset - offset);

    result.total_size = final_offset;
    result.total_padding = padding;
    result.is_cache_aligned = should_cache_align(type_name);

    // Update statistics
    if (!hot_fields.empty()) {
        cache_layout_stats_.types_optimized++;
        cache_layout_stats_.hot_fields_promoted += hot_fields.size();
    }

    return result;
}

bool LLVMIRGen::should_cache_align(const std::string& type_name) const {
    // Cache align types with @cache_aligned decorator or types used in hot loops
    // For now, check if it's a frequently-used container or has @hot decorator
    auto class_def = env_.lookup_class(type_name);
    if (class_def) {
        // Check for @cache_aligned or @hot decorator
        // These would be set during type checking
        // For now, cache-align large classes (> 256 bytes) that aren't value classes
        if (!class_def->is_value) {
            auto info = type_size_cache_.find(type_name);
            if (info != type_size_cache_.end() && info->second.computed_size > 256) {
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// Phase 14: Class Monomorphization
// ============================================================================

void LLVMIRGen::analyze_monomorphization_candidates(const parser::FuncDecl& func) {
    // Skip non-generic functions
    if (func.generics.empty()) {
        return;
    }

    // Skip if no where clause
    if (!func.where_clause.has_value()) {
        return;
    }

    const auto& where_clause = *func.where_clause;

    // Look for type parameters that are constrained to class types
    for (const auto& generic : func.generics) {
        // Check if this generic parameter has a class constraint
        // (e.g., T: SomeClass or T: SomeBehavior implemented by classes)
        for (const auto& [type_param, bounds] : where_clause.constraints) {
            // Check each bound type
            for (const auto& bound : bounds) {
                if (auto* named = std::get_if<parser::NamedType>(&bound->kind)) {
                    const std::string& bound_name = named->path.segments.back();

                    // Check if bound is a sealed class
                    auto class_def = env_.lookup_class(bound_name);
                    if (class_def && class_def->is_sealed) {
                        // This is a monomorphization candidate
                        MonomorphizationCandidate candidate;
                        candidate.func_name = func.name;
                        candidate.class_param = generic.name;
                        candidate.concrete_class = bound_name;
                        candidate.benefits_from_devirt = true;

                        pending_monomorphizations_.push_back(candidate);
                        monomorph_stats_.candidates_found++;
                        monomorph_stats_.devirt_opportunities++;
                    }
                }
            }
        }
    }
}

void LLVMIRGen::gen_specialized_function(const MonomorphizationCandidate& candidate) {
    // Generate mangled name for specialized function
    std::string specialized_name = candidate.func_name + "__" + candidate.concrete_class;

    // Check if already generated
    if (specialized_functions_.count(specialized_name) > 0) {
        return;
    }

    // Find the original generic function
    auto it = pending_generic_funcs_.find(candidate.func_name);
    if (it == pending_generic_funcs_.end()) {
        return;
    }

    const parser::FuncDecl* func = it->second;

    // Create type substitution map
    std::unordered_map<std::string, types::TypePtr> type_subs;

    // Create a named type for the concrete class
    auto concrete_type = std::make_shared<types::Type>(
        types::Type{types::NamedType{candidate.concrete_class, "", {}}});
    type_subs[candidate.class_param] = concrete_type;

    // Generate instantiation with the concrete type
    std::vector<types::TypePtr> type_args = {concrete_type};
    gen_func_instantiation(*func, type_args);

    specialized_functions_.insert(specialized_name);
    monomorph_stats_.specializations_generated++;
}

// ============================================================================
// Phase 6.3.4: Sparse Interface Layout Optimization
// ============================================================================

auto LLVMIRGen::analyze_interface_layout(
    const std::string& iface_name, const std::vector<std::pair<std::string, std::string>>& impls)
    -> InterfaceLayoutInfo {

    InterfaceLayoutInfo layout;
    layout.interface_name = iface_name;
    layout.original_size = impls.size();

    interface_layout_stats_.interfaces_analyzed++;

    // Track which methods have implementations
    size_t compacted_index = 0;
    for (size_t i = 0; i < impls.size(); ++i) {
        layout.method_names.push_back(impls[i].first);

        bool has_impl = (impls[i].second != "null" && !impls[i].second.empty());
        layout.has_implementation.push_back(has_impl);

        if (has_impl) {
            layout.compacted_indices.push_back(compacted_index++);
        } else {
            // Null implementation - will be removed in compacted layout
            layout.compacted_indices.push_back(SIZE_MAX); // Invalid index
        }
    }

    layout.compacted_size = compacted_index;

    // Track statistics
    size_t gaps_removed = layout.original_size - layout.compacted_size;
    if (gaps_removed > 0) {
        interface_layout_stats_.interfaces_compacted++;
        interface_layout_stats_.slots_removed += gaps_removed;
        interface_layout_stats_.bytes_saved += gaps_removed * 8; // 8 bytes per pointer
    }

    // Store layout info for later lookup
    interface_layouts_[iface_name] = layout;

    return layout;
}

void LLVMIRGen::gen_compacted_interface_vtable(
    const std::string& class_name, const std::string& iface_name, const InterfaceLayoutInfo& layout,
    const std::vector<std::pair<std::string, std::string>>& impls) {

    // If no gaps were removed, generate standard vtable
    if (layout.original_size == layout.compacted_size) {
        // No optimization needed - fallback to standard generation
        return;
    }

    // Generate compacted vtable type (only for non-null implementations)
    std::string vtable_type_name = "%vtable." + iface_name + ".compact";

    // Check if we need to emit the compacted type
    if (emitted_interface_vtable_types_.find(iface_name + ".compact") ==
        emitted_interface_vtable_types_.end()) {
        std::string vtable_type = vtable_type_name + " = type { ";
        for (size_t i = 0; i < layout.compacted_size; ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        if (layout.compacted_size == 0) {
            vtable_type += "ptr"; // At least one slot
        }
        vtable_type += " }";
        emit_line(vtable_type);
        emitted_interface_vtable_types_.insert(iface_name + ".compact");
    }

    // Generate compacted vtable global
    std::string vtable_name = "@vtable." + class_name + "." + iface_name + ".compact";
    std::string vtable_value = "{ ";

    bool first = true;
    for (size_t i = 0; i < impls.size(); ++i) {
        if (!layout.has_implementation[i]) {
            continue; // Skip null implementations
        }

        if (!first)
            vtable_value += ", ";
        first = false;

        if (impls[i].second == "null" || impls[i].second.empty()) {
            vtable_value += "ptr null";
        } else {
            vtable_value += "ptr " + impls[i].second;
        }
    }

    if (layout.compacted_size == 0) {
        vtable_value += "ptr null"; // Placeholder
    }
    vtable_value += " }";

    emit_line(vtable_name + " = internal constant " + vtable_type_name + " " + vtable_value);

    // Update statistics
    interface_vtable_stats_.compacted_slots += (layout.original_size - layout.compacted_size);
}

auto LLVMIRGen::get_compacted_interface_index(const std::string& iface_name,
                                              const std::string& method_name) const -> size_t {
    auto it = interface_layouts_.find(iface_name);
    if (it == interface_layouts_.end()) {
        return SIZE_MAX; // Not found
    }

    const auto& layout = it->second;
    for (size_t i = 0; i < layout.method_names.size(); ++i) {
        if (layout.method_names[i] == method_name) {
            return layout.compacted_indices[i];
        }
    }

    return SIZE_MAX; // Method not found
}

} // namespace tml::codegen
