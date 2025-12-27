// LLVM IR generator - Generic instantiation
// Handles: generate_pending_instantiations, require_enum_instantiation, require_func_instantiation

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// ============ Generate Pending Generic Instantiations ============
// Iteratively generate all pending struct/enum/func instantiations
// Loops until no new instantiations are added (handles recursive types)

void LLVMIRGen::generate_pending_instantiations() {
    const int MAX_ITERATIONS = 100; // Prevent infinite loops
    int iterations = 0;

    // First pass: generate ALL type definitions (structs and enums)
    // This must complete before any functions that use these types
    bool types_changed = true;
    while (types_changed && iterations < MAX_ITERATIONS) {
        types_changed = false;
        ++iterations;

        // Generate pending struct instantiations
        for (auto& [key, inst] : struct_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                // Find the generic struct declaration
                auto it = pending_generic_structs_.find(inst.base_name);
                if (it != pending_generic_structs_.end()) {
                    gen_struct_instantiation(*it->second, inst.type_args);
                    types_changed = true;
                }
            }
        }

        // Generate pending enum instantiations
        for (auto& [key, inst] : enum_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                auto it = pending_generic_enums_.find(inst.base_name);
                if (it != pending_generic_enums_.end()) {
                    gen_enum_instantiation(*it->second, inst.type_args);
                    types_changed = true;
                }
            }
        }
    }

    // Second pass: generate functions (may discover new types, so we loop)
    iterations = 0;
    bool changed = true;
    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        ++iterations;

        // Generate pending function instantiations
        for (auto& [key, inst] : func_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                auto it = pending_generic_funcs_.find(inst.base_name);
                if (it != pending_generic_funcs_.end()) {
                    gen_func_instantiation(*it->second, inst.type_args);
                    changed = true;
                }
            }
        }

        // If new types were discovered during function generation, emit them now
        // before continuing with more functions
        bool new_types = false;
        for (auto& [key, inst] : struct_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;
                auto it = pending_generic_structs_.find(inst.base_name);
                if (it != pending_generic_structs_.end()) {
                    gen_struct_instantiation(*it->second, inst.type_args);
                    new_types = true;
                }
            }
        }
        for (auto& [key, inst] : enum_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;
                auto it = pending_generic_enums_.find(inst.base_name);
                if (it != pending_generic_enums_.end()) {
                    gen_enum_instantiation(*it->second, inst.type_args);
                    new_types = true;
                }
            }
        }
        if (new_types)
            changed = true;
    }
}

// Request enum instantiation - returns mangled name
// Immediately generates the type definition to type_defs_buffer_ if not already generated
auto LLVMIRGen::require_enum_instantiation(const std::string& base_name,
                                           const std::vector<types::TypePtr>& type_args)
    -> std::string {
    std::string mangled = mangle_struct_name(base_name, type_args);

    auto it = enum_instantiations_.find(mangled);
    if (it != enum_instantiations_.end()) {
        return mangled;
    }

    enum_instantiations_[mangled] = GenericInstantiation{
        base_name, type_args, mangled,
        true // Mark as generated since we'll generate it immediately
    };

    // Register enum variants and generate type definition immediately
    auto decl_it = pending_generic_enums_.find(base_name);
    if (decl_it != pending_generic_enums_.end()) {
        const parser::EnumDecl* decl = decl_it->second;

        // Register variant tags with mangled enum name
        int tag = 0;
        for (const auto& variant : decl->variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }

        // Generate type definition immediately to type_defs_buffer_
        gen_enum_instantiation(*decl, type_args);
    }

    return mangled;
}

// Placeholder for function instantiation (will implement when adding generic functions)
auto LLVMIRGen::require_func_instantiation(const std::string& base_name,
                                           const std::vector<types::TypePtr>& type_args)
    -> std::string {
    std::string mangled = mangle_func_name(base_name, type_args);

    // Register the instantiation if not already registered
    if (func_instantiations_.find(mangled) == func_instantiations_.end()) {
        func_instantiations_[mangled] = GenericInstantiation{
            base_name, type_args, mangled,
            false // not generated yet
        };
    }

    return mangled;
}

} // namespace tml::codegen
