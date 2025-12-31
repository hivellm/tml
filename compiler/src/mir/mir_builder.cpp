// MIR Builder Implementation
//
// This is the main entry point for the MIR builder. The actual implementation
// is split across multiple files in the builder/ subdirectory:
//   - builder/types.cpp    - Type conversion functions
//   - builder/expr.cpp     - Expression building functions
//   - builder/stmt.cpp     - Statement and declaration building
//   - builder/pattern.cpp  - Pattern matching and destructuring
//   - builder/control.cpp  - Control flow (if, loops, when)
//   - builder/helpers.cpp  - Helper methods (emit, constants, etc.)

#include "mir/mir_builder.hpp"

namespace tml::mir {

MirBuilder::MirBuilder(const types::TypeEnv& env) : env_(env) {}

auto MirBuilder::build(const parser::Module& ast_module) -> Module {
    module_.name = ast_module.name;

    // Process declarations
    for (const auto& decl : ast_module.decls) {
        build_decl(*decl);
    }

    return std::move(module_);
}

} // namespace tml::mir
