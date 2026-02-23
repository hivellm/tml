TML_MODULE("compiler")

//! # Builtin Time Functions (REMOVED)
//!
//! Phase 25: time_ns/sleep_ms migrated to @extern("c") FFI in lib/std/src/time.tml.
//! Phase 39: All remaining type registrations removed (dead code — time_ms, time_us,
//!           time_ns, sleep_ms, sleep_us, elapsed_ms, elapsed_us, elapsed_ns were
//!           only in the type environment but had no codegen handlers since Phase 25).

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_time() {
    // All time builtins removed. time_ns and sleep_ms are now @extern("c")
    // in lib/std/src/time.tml and don't need type registrations here.
}

} // namespace tml::types
