TML_MODULE("compiler")

//! # Builtins Snapshot Cache - Implementation
//!
//! Caches a TypeEnv with builtins initialized to avoid redundant
//! init_builtins() calls across compilation units.

#include "types/builtins_cache.hpp"

namespace tml::types {

BuiltinsSnapshot& BuiltinsSnapshot::instance() {
    static BuiltinsSnapshot instance;
    return instance;
}

TypeEnv BuiltinsSnapshot::create_env() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!base_env_) {
        // First call: create the base TypeEnv (calls init_builtins())
        base_env_ = std::make_unique<TypeEnv>();
    }
    // Return a snapshot (copies type tables, resets per-file state)
    return base_env_->snapshot();
}

void BuiltinsSnapshot::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    base_env_.reset();
}

} // namespace tml::types
