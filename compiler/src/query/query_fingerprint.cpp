#include "query/query_fingerprint.hpp"

#include "common/crc32c.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace tml::query {

std::string Fingerprint::to_hex() const {
    static constexpr char HEX[] = "0123456789abcdef";
    char buf[33];
    uint64_t vals[2] = {high, low};
    for (int v = 0; v < 2; ++v) {
        uint64_t val = vals[v];
        for (int i = 15; i >= 0; --i) {
            buf[v * 16 + i] = HEX[val & 0xF];
            val >>= 4;
        }
    }
    buf[32] = '\0';
    return std::string(buf);
}

Fingerprint fingerprint_bytes(const void* data, size_t len) {
    if (!data || len == 0) {
        return {};
    }

    const auto* bytes = static_cast<const uint8_t*>(data);
    size_t half = len / 2;

    // High: CRC32C of first half combined with length
    uint32_t crc_high = tml::crc32c(bytes, half > 0 ? half : len);
    uint64_t hi = (static_cast<uint64_t>(crc_high) << 32) | static_cast<uint64_t>(len);

    // Low: CRC32C of second half combined with a salt
    static constexpr uint32_t SALT = 0x9E3779B9; // golden ratio
    uint32_t crc_low = tml::crc32c(bytes + half, len - half);
    uint64_t lo = (static_cast<uint64_t>(crc_low) << 32) | static_cast<uint64_t>(SALT ^ (len >> 1));

    return {hi, lo};
}

Fingerprint fingerprint_string(const std::string& str) {
    return fingerprint_bytes(str.data(), str.size());
}

Fingerprint fingerprint_combine(Fingerprint a, Fingerprint b) {
    // Mix the fingerprints together
    uint64_t hi = a.high ^ (b.high * 0x517CC1B727220A95ULL + 1);
    uint64_t lo = a.low ^ (b.low * 0x6C62272E07BB0142ULL + 1);
    return {hi, lo};
}

Fingerprint fingerprint_source(const std::string& file_path) {
    try {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) {
            return {};
        }
        auto size = file.tellg();
        if (size <= 0) {
            return {};
        }
        file.seekg(0);
        std::string content(static_cast<size_t>(size), '\0');
        file.read(content.data(), size);
        return fingerprint_string(content);
    } catch (...) {
        return {};
    }
}

} // namespace tml::query
