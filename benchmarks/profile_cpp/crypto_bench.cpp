/**
 * Crypto Benchmarks (C++)
 *
 * Tests cryptographic hashing: SHA256, SHA512, MD5.
 * Uses OpenSSL EVP API - same library TML uses via FFI.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <cstring>
#include <string>

// OpenSSL headers
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

// Prevent optimization
volatile int64_t sink = 0;
volatile const char* str_sink = nullptr;

// Helper: convert digest to hex string
std::string to_hex(const unsigned char* digest, size_t len) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result(len * 2, '\0');
    for (size_t i = 0; i < len; ++i) {
        result[i * 2] = hex_chars[digest[i] >> 4];
        result[i * 2 + 1] = hex_chars[digest[i] & 0x0F];
    }
    return result;
}

// SHA256 one-shot (short string - 13 bytes)
void bench_sha256_short(int64_t iterations) {
    const char* input = "Hello, World!";
    size_t input_len = strlen(input);
    unsigned char digest[SHA256_DIGEST_LENGTH];
    int64_t total = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, input, input_len);
        unsigned int len;
        EVP_DigestFinal_ex(ctx, digest, &len);
        EVP_MD_CTX_free(ctx);
        total++;
    }
    sink = total;
}

// SHA256 one-shot (medium string - 95 bytes)
void bench_sha256_medium(int64_t iterations) {
    const char* input = "The quick brown fox jumps over the lazy dog. The quick brown fox jumps "
                        "over the lazy dog again!";
    size_t input_len = strlen(input);
    unsigned char digest[SHA256_DIGEST_LENGTH];
    int64_t total = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, input, input_len);
        unsigned int len;
        EVP_DigestFinal_ex(ctx, digest, &len);
        EVP_MD_CTX_free(ctx);
        total++;
    }
    sink = total;
}

// SHA256 streaming (multiple updates)
void bench_sha256_streaming(int64_t iterations) {
    int64_t total = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, "Hello", 5);
        EVP_DigestUpdate(ctx, ", ", 2);
        EVP_DigestUpdate(ctx, "World!", 6);
        unsigned char digest[SHA256_DIGEST_LENGTH];
        unsigned int len;
        EVP_DigestFinal_ex(ctx, digest, &len);
        EVP_MD_CTX_free(ctx);
        total++;
    }
    sink = total;
}

// SHA512 one-shot (short string)
void bench_sha512_short(int64_t iterations) {
    const char* input = "Hello, World!";
    size_t input_len = strlen(input);
    unsigned char digest[SHA512_DIGEST_LENGTH];
    int64_t total = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr);
        EVP_DigestUpdate(ctx, input, input_len);
        unsigned int len;
        EVP_DigestFinal_ex(ctx, digest, &len);
        EVP_MD_CTX_free(ctx);
        total++;
    }
    sink = total;
}

// MD5 one-shot (short string)
void bench_md5_short(int64_t iterations) {
    const char* input = "Hello, World!";
    size_t input_len = strlen(input);
    unsigned char digest[MD5_DIGEST_LENGTH];
    int64_t total = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
        EVP_DigestUpdate(ctx, input, input_len);
        unsigned int len;
        EVP_DigestFinal_ex(ctx, digest, &len);
        EVP_MD_CTX_free(ctx);
        total++;
    }
    sink = total;
}

// SHA256 + to_hex
void bench_sha256_to_hex(int64_t iterations) {
    const char* input = "Hello, World!";
    size_t input_len = strlen(input);
    int64_t total = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        unsigned char digest[SHA256_DIGEST_LENGTH];
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, input, input_len);
        unsigned int len;
        EVP_DigestFinal_ex(ctx, digest, &len);
        EVP_MD_CTX_free(ctx);

        std::string hex = to_hex(digest, SHA256_DIGEST_LENGTH);
        total += hex.size();
        bench::do_not_optimize(hex.data());
    }
    sink = total;
}

int main() {
    bench::Benchmark b("Crypto");

    const int64_t ITERATIONS = 100000;

    b.run_with_iter("SHA256 (13 bytes)", ITERATIONS, bench_sha256_short, 100);
    b.run_with_iter("SHA256 (95 bytes)", ITERATIONS, bench_sha256_medium, 100);
    b.run_with_iter("SHA256 Streaming (3 updates)", ITERATIONS, bench_sha256_streaming, 100);
    b.run_with_iter("SHA512 (13 bytes)", ITERATIONS, bench_sha512_short, 100);
    b.run_with_iter("MD5 (13 bytes)", ITERATIONS, bench_md5_short, 100);
    b.run_with_iter("SHA256 + to_hex (13 bytes)", ITERATIONS, bench_sha256_to_hex, 100);

    b.print_results();
    b.save_json("../results/crypto_cpp.json");

    return 0;
}
