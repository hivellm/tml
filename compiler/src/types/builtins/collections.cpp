//! # Builtin Collection Functions
//!
//! This file previously registered intrinsics for standard collections.
//! All collection types are now pure TML:
//!
//! - List[T]: see lib/std/src/collections/list.tml
//! - HashMap[K,V]: see lib/std/src/collections/hashmap.tml
//! - Buffer: see lib/std/src/collections/buffer.tml

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_collections() {
    // All collection builtins removed — now pure TML.
    // Method calls go through normal impl dispatch.
}

} // namespace tml::types
