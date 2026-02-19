//! # Builtin String Functions — REMOVED (Phase 29)
//!
//! All 29 FuncSig registrations (12 string + 14 char + 3 char_to_string)
//! were dead code. No compiler code path queried the functions_ map for
//! these names:
//!
//! - String ops: codegen uses try_gen_builtin_string() inline checks in
//!   compiler/src/codegen/llvm/builtins/string.cpp, not functions_ lookup
//! - Char ops: migrated to pure TML in lib/core/src/char/methods.tml
//!   (Phase 18.2), codegen dispatch already removed
//!
//! File kept as documentation stub. The init_builtin_string() method is
//! retained as a no-op to avoid header changes, but is no longer called
//! from init_builtins() in register.cpp.

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_string() {
    // No-op — all 29 FuncSig registrations removed in Phase 29.
    // String/char operations are handled by:
    //   - try_gen_builtin_string() for inline codegen (str_len, str_eq, etc.)
    //   - Pure TML implementations in lib/core/src/char/methods.tml
    //   - Module-qualified calls (core::str::*, core::char::*) that bypass
    //     builtin dispatch entirely
}

} // namespace tml::types
