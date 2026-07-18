TML_MODULE("compiler")

#include "query/query_fingerprint.hpp"

#include "common/crc32c.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace tml::query {

// ============================================================================
// F-026: session-level source-fingerprint memo
// ============================================================================
//
// A 1,339-file test run previously re-read and re-hashed the same unchanged
// stdlib/.meta sources ~1,339× (once per QueryContext). This process-global,
// (mtime,size)-gated memo hashes each file's content once per run: subsequent
// calls with an unchanged file return the cached fingerprint. Correctness rests
// on last_write_time being high-resolution — an edit changes mtime, forcing a
// re-hash (same freshness signal the phase42b .ast.bin sidecar relies on).
namespace {
struct FpMemoEntry {
    uint64_t mtime = 0;
    uint64_t size = 0;
    Fingerprint fp;
};
std::mutex g_fp_memo_mtx;
std::unordered_map<std::string, FpMemoEntry> g_fp_memo;
} // namespace

void reset_source_fingerprint_memo() {
    std::lock_guard<std::mutex> lk(g_fp_memo_mtx);
    g_fp_memo.clear();
}

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
        // F-026: (mtime,size)-gated memo. Stat first; on a hit return the cached
        // fingerprint without reading the file. Stat is far cheaper than a full
        // read+CRC of a stdlib source, and the same unchanged file is hashed
        // once per run instead of once per QueryContext.
        std::error_code ec;
        auto fsize = std::filesystem::file_size(file_path, ec);
        uint64_t mtime = 0;
        bool have_stat = !ec;
        if (have_stat) {
            std::error_code ec2;
            auto ft = std::filesystem::last_write_time(file_path, ec2);
            if (!ec2) {
                mtime = static_cast<uint64_t>(ft.time_since_epoch().count());
            } else {
                have_stat = false;
            }
        }
        if (have_stat) {
            std::lock_guard<std::mutex> lk(g_fp_memo_mtx);
            auto it = g_fp_memo.find(file_path);
            if (it != g_fp_memo.end() && it->second.mtime == mtime &&
                it->second.size == static_cast<uint64_t>(fsize)) {
                return it->second.fp;
            }
        }

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
        auto fp = fingerprint_string(content);

        if (have_stat) {
            std::lock_guard<std::mutex> lk(g_fp_memo_mtx);
            g_fp_memo[file_path] = FpMemoEntry{mtime, static_cast<uint64_t>(fsize), fp};
        }
        return fp;
    } catch (...) {
        return {};
    }
}

} // namespace tml::query
