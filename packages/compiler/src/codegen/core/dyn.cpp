// LLVM IR generator - Dynamic dispatch and vtables
// Handles: register_impl, emit_dyn_type, get_vtable, emit_vtables

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// ============ Vtable Support ============

void LLVMIRGen::register_impl(const parser::ImplDecl* impl) {
    pending_impls_.push_back(impl);

    // Eagerly populate behavior_method_order_ and vtables_ for dyn dispatch
    if (impl->trait_path && !impl->trait_path->segments.empty()) {
        std::string behavior_name = impl->trait_path->segments.back();

        // Populate behavior_method_order_ only once per behavior
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            auto behavior_def = env_.lookup_behavior(behavior_name);
            if (behavior_def) {
                std::vector<std::string> methods;
                for (const auto& m : behavior_def->methods) {
                    methods.push_back(m.name);
                }
                behavior_method_order_[behavior_name] = methods;
            }
        }

        // Register vtable for EVERY impl (not just the first per behavior)
        std::string type_name;
        if (impl->self_type->kind.index() == 0) {
            const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
            if (!named.path.segments.empty()) {
                type_name = named.path.segments.back();
            }
        }
        if (!type_name.empty()) {
            std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
            std::string key = type_name + "::" + behavior_name;
            vtables_[key] = vtable_name;
        }
    }
}

void LLVMIRGen::emit_dyn_type(const std::string& behavior_name) {
    if (emitted_dyn_types_.count(behavior_name))
        return;
    emitted_dyn_types_.insert(behavior_name);

    // Emit the dyn type as a fat pointer struct: { data_ptr, vtable_ptr }
    emit_line("%dyn." + behavior_name + " = type { ptr, ptr }");
}

auto LLVMIRGen::get_vtable(const std::string& type_name, const std::string& behavior_name)
    -> std::string {
    std::string key = type_name + "::" + behavior_name;
    auto it = vtables_.find(key);
    if (it != vtables_.end()) {
        return it->second;
    }
    return ""; // No vtable found
}

void LLVMIRGen::emit_vtables() {
    // For each registered impl block, generate a vtable
    for (const auto* impl : pending_impls_) {
        if (!impl->trait_path)
            continue; // Skip inherent impls

        // Get the type name and behavior name
        std::string type_name;
        if (impl->self_type->kind.index() == 0) { // NamedType
            const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
            if (!named.path.segments.empty()) {
                type_name = named.path.segments.back();
            }
        }

        std::string behavior_name;
        if (!impl->trait_path->segments.empty()) {
            behavior_name = impl->trait_path->segments.back();
        }

        if (type_name.empty() || behavior_name.empty())
            continue;

        // Emit the dyn type for this behavior
        emit_dyn_type(behavior_name);

        // Get behavior method order
        auto behavior_def = env_.lookup_behavior(behavior_name);
        if (!behavior_def)
            continue;

        // Build vtable type: array of function pointers
        std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
        std::string vtable_type = "{ ";
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";

        // Build vtable value with function pointers
        std::string vtable_value = "{ ";
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";
            const auto& method = behavior_def->methods[i];
            vtable_value += "ptr @tml_" + type_name + "_" + method.name;
        }
        vtable_value += " }";

        // Emit vtable global constant
        emit_line(vtable_name + " = internal constant " + vtable_type + " " + vtable_value);

        // Register vtable
        std::string key = type_name + "::" + behavior_name;
        vtables_[key] = vtable_name;

        // Store method order for this behavior
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            std::vector<std::string> methods;
            for (const auto& m : behavior_def->methods) {
                methods.push_back(m.name);
            }
            behavior_method_order_[behavior_name] = methods;
        }
    }
}

} // namespace tml::codegen
