//! # CRC32C Implementation
//!
//! Implementation of file hashing utilities.

#include "common/crc32c.hpp"

#include <fstream>
#include <sstream>

namespace tml {

std::string crc32c_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return "";
    }

    // Read entire file
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    return crc32c_hex(content.data(), content.size());
}

} // namespace tml
