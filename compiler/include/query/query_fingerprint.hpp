//! # Query Fingerprinting
//!
//! 128-bit fingerprints for query inputs and outputs.
//! Used by the incremental compilation system to detect when cached
//! results can be reused vs when recomputation is needed.
//!
//! Uses CRC32C (Castagnoli) from `common/crc32c.hpp` for fast hashing.

#pragma once

#include <cstdint>
#include <string>

namespace tml::query {

/// 128-bit fingerprint for query inputs and outputs.
struct Fingerprint {
    uint64_t high = 0;
    uint64_t low = 0;

    bool operator==(const Fingerprint& other) const = default;
    bool operator!=(const Fingerprint& other) const = default;

    /// Returns true if this fingerprint has not been computed yet.
    [[nodiscard]] bool is_zero() const {
        return high == 0 && low == 0;
    }

    /// Returns a 32-character hex string representation.
    [[nodiscard]] std::string to_hex() const;
};

/// Compute a fingerprint from raw bytes.
[[nodiscard]] Fingerprint fingerprint_bytes(const void* data, size_t len);

/// Compute a fingerprint from a string.
[[nodiscard]] Fingerprint fingerprint_string(const std::string& str);

/// Combine two fingerprints into one.
[[nodiscard]] Fingerprint fingerprint_combine(Fingerprint a, Fingerprint b);

/// Compute a fingerprint for a source file (reads file, hashes content).
/// Returns zero fingerprint on error.
[[nodiscard]] Fingerprint fingerprint_source(const std::string& file_path);

} // namespace tml::query
