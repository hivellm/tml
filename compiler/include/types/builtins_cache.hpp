//! # Builtins Snapshot Cache
//!
//! Thread-safe cached snapshot of TypeEnv with builtins initialized.
//! Avoids re-running init_builtins() for every compilation unit.
//!
//! ## Usage
//!
//! ```cpp
//! // Instead of creating a fresh TypeEnv (which calls init_builtins()):
//! TypeEnv env;
//!
//! // Use the cached snapshot:
//! TypeEnv env = BuiltinsSnapshot::instance().create_env();
//! ```

#ifndef TML_TYPES_BUILTINS_CACHE_HPP
#define TML_TYPES_BUILTINS_CACHE_HPP

#include "types/env.hpp"

#include <memory>
#include <mutex>

namespace tml::types {

/// Thread-safe cached snapshot of TypeEnv with builtins initialized.
/// First call creates the base TypeEnv (runs init_builtins() once).
/// Subsequent calls return snapshots with type tables pre-populated.
class BuiltinsSnapshot {
public:
    /// Get the singleton instance.
    static BuiltinsSnapshot& instance();

    /// Returns a fresh TypeEnv with builtins pre-populated.
    /// First call creates the base TypeEnv; subsequent calls return snapshots.
    TypeEnv create_env();

    /// Clears the cached snapshot (e.g., for --no-cache flag).
    void clear();

private:
    BuiltinsSnapshot() = default;
    ~BuiltinsSnapshot() = default;

    // Non-copyable
    BuiltinsSnapshot(const BuiltinsSnapshot&) = delete;
    BuiltinsSnapshot& operator=(const BuiltinsSnapshot&) = delete;

    std::mutex mutex_;
    std::unique_ptr<TypeEnv> base_env_;
};

} // namespace tml::types

#endif // TML_TYPES_BUILTINS_CACHE_HPP
