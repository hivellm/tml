/**
 * Encoding Benchmarks (C++)
 *
 * Tests encoding/decoding performance: base64, hex, base32.
 * Pure C++ implementations comparable with TML encoding benchmarks.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <cstring>
#include <string>

// Prevent optimization
volatile int64_t sink = 0;

// ============================================================
// Base64 implementation (standard, no external deps)
// ============================================================

static const char b64_encode_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const uint8_t b64_decode_table[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 62,  255, 255, 255, 63,  52,  53,  54,  55,  56,  57,  58,  59,  60,
    61,  255, 255, 255, 0,   255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,
    11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  255, 255, 255, 255,
    255, 255, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  51,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255,
};

std::string base64_encode(const std::string& input) {
    size_t len = input.size();
    size_t out_len = 4 * ((len + 2) / 3);
    std::string result(out_len, '=');
    const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());

    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t a = data[i];
        uint32_t b = (i + 1 < len) ? data[i + 1] : 0;
        uint32_t c = (i + 2 < len) ? data[i + 2] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;

        result[j++] = b64_encode_table[(triple >> 18) & 0x3F];
        result[j++] = b64_encode_table[(triple >> 12) & 0x3F];
        result[j++] = (i + 1 < len) ? b64_encode_table[(triple >> 6) & 0x3F] : '=';
        result[j++] = (i + 2 < len) ? b64_encode_table[triple & 0x3F] : '=';
    }
    return result;
}

std::string base64_decode(const std::string& input) {
    size_t len = input.size();
    if (len % 4 != 0)
        return "";

    size_t padding = 0;
    if (len > 0 && input[len - 1] == '=')
        padding++;
    if (len > 1 && input[len - 2] == '=')
        padding++;

    size_t out_len = (len / 4) * 3 - padding;
    std::string result(out_len, '\0');
    const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());

    size_t j = 0;
    for (size_t i = 0; i < len; i += 4) {
        uint32_t a = b64_decode_table[data[i]];
        uint32_t b = b64_decode_table[data[i + 1]];
        uint32_t c = b64_decode_table[data[i + 2]];
        uint32_t d = b64_decode_table[data[i + 3]];
        uint32_t triple = (a << 18) | (b << 12) | (c << 6) | d;

        if (j < out_len)
            result[j++] = static_cast<char>((triple >> 16) & 0xFF);
        if (j < out_len)
            result[j++] = static_cast<char>((triple >> 8) & 0xFF);
        if (j < out_len)
            result[j++] = static_cast<char>(triple & 0xFF);
    }
    return result;
}

// ============================================================
// Hex implementation
// ============================================================

static const char hex_chars[] = "0123456789abcdef";

std::string hex_encode(const std::string& input) {
    std::string result(input.size() * 2, '\0');
    const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
    for (size_t i = 0; i < input.size(); ++i) {
        result[i * 2] = hex_chars[data[i] >> 4];
        result[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    return result;
}

static uint8_t hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

std::string hex_decode(const std::string& input) {
    if (input.size() % 2 != 0)
        return "";
    std::string result(input.size() / 2, '\0');
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<char>((hex_val(input[i * 2]) << 4) | hex_val(input[i * 2 + 1]));
    }
    return result;
}

// ============================================================
// Base32 implementation
// ============================================================

static const char b32_encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

std::string base32_encode(const std::string& input) {
    size_t len = input.size();
    size_t out_len = ((len + 4) / 5) * 8;
    std::string result(out_len, '=');
    const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());

    size_t j = 0;
    for (size_t i = 0; i < len; i += 5) {
        size_t remaining = len - i;
        uint64_t buf = 0;
        for (size_t k = 0; k < 5 && i + k < len; ++k) {
            buf = (buf << 8) | data[i + k];
        }
        // Pad with zeros for remaining bytes
        buf <<= (5 - (remaining < 5 ? remaining : 5)) * 8;

        int chars_to_write = 0;
        switch (remaining < 5 ? remaining : 5) {
        case 5:
            chars_to_write = 8;
            break;
        case 4:
            chars_to_write = 7;
            break;
        case 3:
            chars_to_write = 5;
            break;
        case 2:
            chars_to_write = 4;
            break;
        case 1:
            chars_to_write = 2;
            break;
        }
        for (int k = 7; k >= 0; --k) {
            if (7 - k < chars_to_write) {
                result[j + (7 - k)] = b32_encode_table[(buf >> (k * 5)) & 0x1F];
            }
        }
        j += 8;
    }
    return result;
}

// ============================================================
// Benchmark functions
// ============================================================

// Base64 encode (short string - 13 bytes)
void bench_b64_encode_short(int64_t iterations) {
    std::string input = "Hello, World!";
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        std::string encoded = base64_encode(input);
        total += encoded.size();
        bench::do_not_optimize(encoded.data());
    }
    sink = total;
}

// Base64 encode (medium string - 95 bytes)
void bench_b64_encode_medium(int64_t iterations) {
    std::string input = "The quick brown fox jumps over the lazy dog. The quick brown fox jumps "
                        "over the lazy dog again!";
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        std::string encoded = base64_encode(input);
        total += encoded.size();
        bench::do_not_optimize(encoded.data());
    }
    sink = total;
}

// Base64 decode
void bench_b64_decode(int64_t iterations) {
    std::string encoded = "SGVsbG8sIFdvcmxkIQ==";
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        std::string decoded = base64_decode(encoded);
        total += decoded.size();
        bench::do_not_optimize(decoded.data());
    }
    sink = total;
}

// Hex encode (13 bytes)
void bench_hex_encode(int64_t iterations) {
    std::string input = "Hello, World!";
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        std::string encoded = hex_encode(input);
        total += encoded.size();
        bench::do_not_optimize(encoded.data());
    }
    sink = total;
}

// Hex decode (26 chars)
void bench_hex_decode(int64_t iterations) {
    std::string encoded = "48656c6c6f2c20576f726c6421";
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        std::string decoded = hex_decode(encoded);
        total += decoded.size();
        bench::do_not_optimize(decoded.data());
    }
    sink = total;
}

// Base32 encode (13 bytes)
void bench_b32_encode(int64_t iterations) {
    std::string input = "Hello, World!";
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        std::string encoded = base32_encode(input);
        total += encoded.size();
        bench::do_not_optimize(encoded.data());
    }
    sink = total;
}

int main() {
    bench::Benchmark b("Encoding");

    const int64_t ITERATIONS = 100000;

    b.run_with_iter("Base64 Encode (13 bytes)", ITERATIONS, bench_b64_encode_short, 100);
    b.run_with_iter("Base64 Encode (95 bytes)", ITERATIONS, bench_b64_encode_medium, 100);
    b.run_with_iter("Base64 Decode (20 chars)", ITERATIONS, bench_b64_decode, 100);
    b.run_with_iter("Hex Encode (13 bytes)", ITERATIONS, bench_hex_encode, 100);
    b.run_with_iter("Hex Decode (26 chars)", ITERATIONS, bench_hex_decode, 100);
    b.run_with_iter("Base32 Encode (13 bytes)", ITERATIONS, bench_b32_encode, 100);

    b.print_results();
    b.save_json("../results/encoding_cpp.json");

    return 0;
}
