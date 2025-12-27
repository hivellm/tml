#include "ir/ir.hpp"

#include <algorithm>

namespace tml::ir {

auto IRBuilder::build_module(const parser::Module& module,
                             const std::string& module_name) -> IRModule {
    current_module_ = module_name;

    IRModule ir_module;
    ir_module.id = generate_id(module_name, "module");
    ir_module.name = module_name;

    // Build all declarations
    std::vector<IRConst> consts;
    std::vector<IRType> types;
    std::vector<IRBehavior> behaviors;
    std::vector<IRImpl> impls;
    std::vector<IRFunc> funcs;

    for (const auto& decl : module.decls) {
        std::visit(
            [&](const auto& d) {
                using T = std::decay_t<decltype(d)>;
                if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                    funcs.push_back(build_func(d));
                } else if constexpr (std::is_same_v<T, parser::StructDecl>) {
                    types.push_back(build_struct(d));
                } else if constexpr (std::is_same_v<T, parser::EnumDecl>) {
                    types.push_back(build_enum(d));
                } else if constexpr (std::is_same_v<T, parser::TraitDecl>) {
                    behaviors.push_back(build_trait(d));
                } else if constexpr (std::is_same_v<T, parser::ImplDecl>) {
                    impls.push_back(build_impl(d));
                } else if constexpr (std::is_same_v<T, parser::ConstDecl>) {
                    consts.push_back(build_const(d));
                }
                // Other declarations handled as needed
            },
            decl->kind);
    }

    // Sort items by kind, then alphabetically by name
    auto sort_by_name = [](auto& vec) {
        std::sort(vec.begin(), vec.end(),
                  [](const auto& a, const auto& b) { return a.name < b.name; });
    };

    sort_by_name(consts);
    sort_by_name(types);
    sort_by_name(behaviors);
    sort_by_name(funcs);
    // impls don't have names, sort by target type
    std::sort(impls.begin(), impls.end(),
              [](const auto& a, const auto& b) { return a.target_type < b.target_type; });

    // Add items in canonical order: const, type, behavior, impl, func
    for (auto& c : consts)
        ir_module.items.push_back(std::move(c));
    for (auto& t : types)
        ir_module.items.push_back(std::move(t));
    for (auto& b : behaviors)
        ir_module.items.push_back(std::move(b));
    for (auto& i : impls)
        ir_module.items.push_back(std::move(i));
    for (auto& f : funcs)
        ir_module.items.push_back(std::move(f));

    return ir_module;
}

} // namespace tml::ir
