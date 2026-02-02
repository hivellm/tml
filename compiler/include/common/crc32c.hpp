//! # CRC32C Hash Utility
//!
//! This module provides CRC32C hashing using the Castagnoli polynomial.
//! CRC32C is faster than cryptographic hashes and has good collision resistance
//! for content verification purposes.
//!
//! ## Usage
//!
//! ```cpp
//! #include "common/crc32c.hpp"
//!
//! std::string data = "Hello, world!";
//! uint32_t hash = tml::crc32c(data.data(), data.size());
//!
//! // Or hash a file
//! std::string file_hash = tml::crc32c_file("path/to/file.txt");
//! ```

#ifndef TML_COMMON_CRC32C_HPP
#define TML_COMMON_CRC32C_HPP

#include <cstdint>
#include <string>

namespace tml {

// ============================================================================
// CRC32C Lookup Table (Castagnoli polynomial 0x1EDC6F41)
// ============================================================================

// Pre-computed lookup table for CRC32C using the Castagnoli polynomial.
// This polynomial is optimized for error detection and is used in iSCSI,
// SCTP, and other protocols. It also has hardware support on modern x86 CPUs.
inline constexpr uint32_t CRC32C_TABLE[256] = {
    0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4, 0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
    0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B, 0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
    0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B, 0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
    0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54, 0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
    0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A, 0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
    0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5, 0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
    0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45, 0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
    0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A, 0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
    0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48, 0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
    0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687, 0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
    0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927, 0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
    0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8, 0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
    0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096, 0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
    0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859, 0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
    0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9, 0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
    0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36, 0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
    0x82F63B78, 0x70BDBF7B, 0x63ED4C8F, 0x9186CF8C, 0x454CAB67, 0xB7272864, 0xA477DB90, 0x561C5893,
    0x080F64B7, 0xFA64E7B4, 0xE9341440, 0x1B5F9743, 0xCF95F3A8, 0x3DFE70AB, 0x2EAE835F, 0xDCC5005C,
    0x9288FB17, 0x60E37814, 0x73B38BE0, 0x81D808E3, 0x55126C08, 0xA779EF0B, 0xB4291CFF, 0x46429FFC,
    0x1851A3D8, 0xEA3A20DB, 0xF96AD32F, 0x0B01502C, 0xDFCB34C7, 0x2DA0B7C4, 0x3EF04430, 0xCC9BC733,
    0xA26B92A6, 0x500011A5, 0x4350E251, 0xB13B6152, 0x65F105B9, 0x979A86BA, 0x84CA754E, 0x76A1F64D,
    0x28B2CA69, 0xDAD9496A, 0xC989BA9E, 0x3BE2399D, 0xEF285D76, 0x1D43DE75, 0x0E132D81, 0xFC78AE82,
    0xB23555C9, 0x405ED6CA, 0x530E253E, 0xA165A63D, 0x75AFC2D6, 0x87C441D5, 0x9494B221, 0x66FF3122,
    0x38EC0D06, 0xCA879E05, 0xD9D76DF1, 0x2BBCEEF2, 0xFF768A19, 0x0D1D091A, 0x1E4DFAEE, 0xEC2679ED,
    0xC3ADDD70, 0x31C65E73, 0x2296AD87, 0xD0FD2E84, 0x04374A6F, 0xF65CC96C, 0xE50C3A98, 0x1767B99B,
    0x497485BF, 0xBB1F06BC, 0xA84FF548, 0x5A24764B, 0x8EEE12A0, 0x7C8591A3, 0x6FD56257, 0x9DBEE154,
    0xD3F31A1F, 0x2198991C, 0x32C86AE8, 0xC0A3E9EB, 0x14698D00, 0xE6020E03, 0xF552FDF7, 0x07397EF4,
    0x592A42D0, 0xAB41C1D3, 0xB8113227, 0x4A7AB124, 0x9EB0D5CF, 0x6CDB56CC, 0x7F8BA538, 0x8DE0263B,
    0xE310539E, 0x117BD09D, 0x022B2369, 0xF040A06A, 0x248AC481, 0xD6E14782, 0xC5B1B476, 0x37DA3775,
    0x69C90B51, 0x9BA28852, 0x88F27BA6, 0x7A99F8A5, 0xAE539C4E, 0x5C381F4D, 0x4F68ECB9, 0xBD036FBA,
    0xF34E94F1, 0x012517F2, 0x1275E406, 0xE01E6705, 0x34D403EE, 0xC6BF80ED, 0xD5EF7319, 0x2784F01A,
    0x7997CC3E, 0x8BFC4F3D, 0x98ACBCC9, 0x6AC73FCA, 0xBE0D5B21, 0x4C66D822, 0x5F362BD6, 0xAD5DA8D5
};

// ============================================================================
// CRC32C Functions
// ============================================================================

/// Computes CRC32C hash of a byte array.
///
/// @param data Pointer to the data to hash
/// @param len Length of the data in bytes
/// @return 32-bit CRC32C hash value
[[nodiscard]] inline uint32_t crc32c(const void* data, size_t len) noexcept {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = CRC32C_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/// Computes CRC32C hash of a string.
///
/// @param str String to hash
/// @return 32-bit CRC32C hash value
[[nodiscard]] inline uint32_t crc32c(const std::string& str) noexcept {
    return crc32c(str.data(), str.size());
}

/// Computes CRC32C hash of a string_view.
///
/// @param str String view to hash
/// @return 32-bit CRC32C hash value
[[nodiscard]] inline uint32_t crc32c(std::string_view str) noexcept {
    return crc32c(str.data(), str.size());
}

/// Computes a combined hash from CRC32C and file size for better collision resistance.
/// Returns a 16-character hex string (8 bytes: 4 for CRC32C + 4 for size).
///
/// @param data Pointer to the data to hash
/// @param len Length of the data in bytes
/// @return 16-character hex string combining hash and size
[[nodiscard]] inline std::string crc32c_hex(const void* data, size_t len) {
    uint32_t hash = crc32c(data, len);
    // Combine hash with size for reduced collision probability
    uint64_t combined = (static_cast<uint64_t>(hash) << 32) | static_cast<uint64_t>(len & 0xFFFFFFFF);

    char hex[17];
    static constexpr char HEX_CHARS[] = "0123456789abcdef";
    for (int i = 15; i >= 0; --i) {
        hex[i] = HEX_CHARS[combined & 0xF];
        combined >>= 4;
    }
    hex[16] = '\0';
    return std::string(hex);
}

/// Computes CRC32C hash of a file and returns as hex string with size.
///
/// @param file_path Path to the file to hash
/// @return 16-character hex string, or empty string on error
[[nodiscard]] std::string crc32c_file(const std::string& file_path);

} // namespace tml

#endif // TML_COMMON_CRC32C_HPP
