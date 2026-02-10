/**
 * @file crypto_dh.c
 * @brief TML Runtime - Diffie-Hellman Key Exchange
 *
 * Implements DH parameter creation, key generation, and shared secret
 * derivation using OpenSSL 3.0+ EVP API.
 *
 * Supports:
 * - Custom DH parameters (prime + generator)
 * - Random parameter generation (by prime length)
 * - Named groups (modp14, modp15, ffdhe2048, etc.)
 * - Key pair generation, public/private key get/set
 * - Shared secret computation via EVP_PKEY_derive
 * - Parameter validation via EVP_PKEY_param_check
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL

#include <openssl/core_names.h>
#include <openssl/dh.h>
#include <openssl/param_build.h>

// ============================================================================
// Internal DH state
// ============================================================================

typedef struct {
    EVP_PKEY* pkey;   // DH key (NULL until generate_keys)
    EVP_PKEY* params; // DH parameters
} TmlDh;

static TmlDh* tml_dh_alloc(void) {
    TmlDh* dh = (TmlDh*)calloc(1, sizeof(TmlDh));
    return dh;
}

// Helper: extract a BIGNUM parameter from an EVP_PKEY as a TmlBuffer (big-endian)
static TmlBuffer* tml_pkey_get_bn_param_buf(EVP_PKEY* pkey, const char* param_name) {
    if (!pkey)
        return NULL;
    BIGNUM* bn = NULL;
    if (EVP_PKEY_get_bn_param(pkey, param_name, &bn) != 1 || !bn)
        return NULL;
    int len = BN_num_bytes(bn);
    if (len <= 0) {
        BN_free(bn);
        return NULL;
    }
    TmlBuffer* buf = tml_create_buffer((int64_t)len);
    if (!buf) {
        BN_free(bn);
        return NULL;
    }
    BN_bn2bin(bn, buf->data);
    buf->length = (int64_t)len;
    BN_free(bn);
    return buf;
}

// Helper: resolve prime length for modp group names
// Returns 0 if unrecognised (caller should try as ffdhe group)
static int tml_dh_modp_prime_len(const char* name) {
    if (!name)
        return 0;
    if (strcmp(name, "modp1") == 0)
        return 768;
    if (strcmp(name, "modp2") == 0)
        return 1024;
    if (strcmp(name, "modp5") == 0)
        return 1536;
    if (strcmp(name, "modp14") == 0)
        return 2048;
    if (strcmp(name, "modp15") == 0)
        return 3072;
    if (strcmp(name, "modp16") == 0)
        return 4096;
    if (strcmp(name, "modp17") == 0)
        return 6144;
    if (strcmp(name, "modp18") == 0)
        return 8192;
    return 0;
}

// Helper: map TML group name to OpenSSL group name for ffdhe groups
static const char* tml_dh_ffdhe_ossl_name(const char* name) {
    if (!name)
        return NULL;
    if (strcmp(name, "ffdhe2048") == 0)
        return "ffdhe2048";
    if (strcmp(name, "ffdhe3072") == 0)
        return "ffdhe3072";
    if (strcmp(name, "ffdhe4096") == 0)
        return "ffdhe4096";
    if (strcmp(name, "ffdhe6144") == 0)
        return "ffdhe6144";
    if (strcmp(name, "ffdhe8192") == 0)
        return "ffdhe8192";
    // Also accept modp names mapped to OpenSSL names for named-group path
    if (strcmp(name, "modp_2048") == 0)
        return "modp_2048";
    if (strcmp(name, "modp_3072") == 0)
        return "modp_3072";
    if (strcmp(name, "modp_4096") == 0)
        return "modp_4096";
    if (strcmp(name, "modp_6144") == 0)
        return "modp_6144";
    if (strcmp(name, "modp_8192") == 0)
        return "modp_8192";
    return NULL;
}

// Helper: get the effective EVP_PKEY (prefer pkey if it has keys, else params)
static EVP_PKEY* tml_dh_effective_pkey(TmlDh* dh) {
    if (!dh)
        return NULL;
    return dh->pkey ? dh->pkey : dh->params;
}

// ============================================================================
// 1. crypto_dh_create - from explicit prime/generator
// ============================================================================

TML_EXPORT void* crypto_dh_create(void* prime_handle, void* generator_handle) {
    TmlBuffer* prime_buf = (TmlBuffer*)prime_handle;
    TmlBuffer* gen_buf = (TmlBuffer*)generator_handle;
    if (!prime_buf || prime_buf->length <= 0 || !gen_buf || gen_buf->length <= 0)
        return NULL;

    TmlDh* dh = tml_dh_alloc();
    if (!dh)
        return NULL;

    // Convert big-endian bytes to BIGNUMs
    BIGNUM* p = BN_bin2bn(prime_buf->data, (int)prime_buf->length, NULL);
    BIGNUM* g = BN_bin2bn(gen_buf->data, (int)gen_buf->length, NULL);
    if (!p || !g) {
        BN_free(p);
        BN_free(g);
        free(dh);
        return NULL;
    }

    // Build OSSL_PARAM array for DH parameters
    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    if (!bld) {
        BN_free(p);
        BN_free(g);
        free(dh);
        return NULL;
    }

    if (OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_P, p) != 1 ||
        OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_G, g) != 1) {
        OSSL_PARAM_BLD_free(bld);
        BN_free(p);
        BN_free(g);
        free(dh);
        return NULL;
    }

    OSSL_PARAM* ossl_params = OSSL_PARAM_BLD_to_param(bld);
    OSSL_PARAM_BLD_free(bld);
    BN_free(p);
    BN_free(g);
    if (!ossl_params) {
        free(dh);
        return NULL;
    }

    // Create EVP_PKEY from params using EVP_PKEY_fromdata
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
    if (!ctx) {
        OSSL_PARAM_free(ossl_params);
        free(dh);
        return NULL;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(ossl_params);
        free(dh);
        return NULL;
    }

    EVP_PKEY* params_pkey = NULL;
    if (EVP_PKEY_fromdata(ctx, &params_pkey, EVP_PKEY_KEY_PARAMETERS, ossl_params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(ossl_params);
        free(dh);
        return NULL;
    }

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(ossl_params);

    dh->params = params_pkey;
    return (void*)dh;
}

// ============================================================================
// 2. crypto_dh_generate - random DH params with given prime length
// ============================================================================

TML_EXPORT void* crypto_dh_generate(int64_t prime_length) {
    if (prime_length <= 0)
        return NULL;

    TmlDh* dh = tml_dh_alloc();
    if (!dh)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, NULL);
    if (!ctx) {
        free(dh);
        return NULL;
    }

    if (EVP_PKEY_paramgen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        free(dh);
        return NULL;
    }

    if (EVP_PKEY_CTX_set_dh_paramgen_prime_len(ctx, (int)prime_length) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        free(dh);
        return NULL;
    }

    EVP_PKEY* params = NULL;
    if (EVP_PKEY_paramgen(ctx, &params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        free(dh);
        return NULL;
    }

    EVP_PKEY_CTX_free(ctx);
    dh->params = params;
    return (void*)dh;
}

// ============================================================================
// 3. crypto_dh_create_group - named DH group
// ============================================================================

TML_EXPORT void* crypto_dh_create_group(const char* group_name) {
    if (!group_name)
        return NULL;

    TmlDh* dh = tml_dh_alloc();
    if (!dh)
        return NULL;

    // First try modp groups (generate params with appropriate prime length)
    int modp_bits = tml_dh_modp_prime_len(group_name);
    if (modp_bits > 0) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, NULL);
        if (!ctx) {
            free(dh);
            return NULL;
        }

        if (EVP_PKEY_paramgen_init(ctx) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            free(dh);
            return NULL;
        }

        if (EVP_PKEY_CTX_set_dh_paramgen_prime_len(ctx, modp_bits) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            free(dh);
            return NULL;
        }

        EVP_PKEY* params = NULL;
        if (EVP_PKEY_paramgen(ctx, &params) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            free(dh);
            return NULL;
        }

        EVP_PKEY_CTX_free(ctx);
        dh->params = params;
        return (void*)dh;
    }

    // Try ffdhe named groups via EVP_PKEY_CTX_set_group_name (OpenSSL 3.0+)
    const char* ossl_name = tml_dh_ffdhe_ossl_name(group_name);
    if (ossl_name) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "DHX", NULL);
        if (!ctx) {
            // Fallback: try "DH" algorithm name
            ctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
        }
        if (!ctx) {
            free(dh);
            return NULL;
        }

        if (EVP_PKEY_paramgen_init(ctx) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            free(dh);
            return NULL;
        }

        if (EVP_PKEY_CTX_set_params(
                ctx, (OSSL_PARAM[]){OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                                     (char*)ossl_name, 0),
                                    OSSL_PARAM_construct_end()}) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            free(dh);
            return NULL;
        }

        EVP_PKEY* params = NULL;
        if (EVP_PKEY_paramgen(ctx, &params) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            free(dh);
            return NULL;
        }

        EVP_PKEY_CTX_free(ctx);
        dh->params = params;
        return (void*)dh;
    }

    // Unknown group
    free(dh);
    return NULL;
}

// ============================================================================
// 4. crypto_dh_generate_keys - generate key pair from params
// ============================================================================

TML_EXPORT void crypto_dh_generate_keys(void* handle) {
    TmlDh* dh = (TmlDh*)handle;
    if (!dh || !dh->params)
        return;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(dh->params, NULL);
    if (!ctx)
        return;

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return;
    }

    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return;
    }

    EVP_PKEY_CTX_free(ctx);

    // Replace any existing key
    if (dh->pkey)
        EVP_PKEY_free(dh->pkey);
    dh->pkey = pkey;
}

// ============================================================================
// 5. crypto_dh_get_public_key / 6. crypto_dh_get_private_key
// ============================================================================

TML_EXPORT void* crypto_dh_get_public_key(void* handle) {
    TmlDh* dh = (TmlDh*)handle;
    EVP_PKEY* pkey = tml_dh_effective_pkey(dh);
    return (void*)tml_pkey_get_bn_param_buf(pkey, OSSL_PKEY_PARAM_PUB_KEY);
}

TML_EXPORT void* crypto_dh_get_private_key(void* handle) {
    TmlDh* dh = (TmlDh*)handle;
    EVP_PKEY* pkey = tml_dh_effective_pkey(dh);
    return (void*)tml_pkey_get_bn_param_buf(pkey, OSSL_PKEY_PARAM_PRIV_KEY);
}

// ============================================================================
// 7. crypto_dh_set_public_key / 8. crypto_dh_set_private_key
// ============================================================================

TML_EXPORT void crypto_dh_set_public_key(void* handle, void* key_handle) {
    TmlDh* dh = (TmlDh*)handle;
    TmlBuffer* key_buf = (TmlBuffer*)key_handle;
    if (!dh || !dh->params || !key_buf || key_buf->length <= 0)
        return;

    // Get existing p and g from params
    BIGNUM* p = NULL;
    BIGNUM* g = NULL;
    EVP_PKEY_get_bn_param(dh->params, OSSL_PKEY_PARAM_FFC_P, &p);
    EVP_PKEY_get_bn_param(dh->params, OSSL_PKEY_PARAM_FFC_G, &g);
    if (!p || !g) {
        BN_free(p);
        BN_free(g);
        return;
    }

    BIGNUM* pub = BN_bin2bn(key_buf->data, (int)key_buf->length, NULL);
    if (!pub) {
        BN_free(p);
        BN_free(g);
        return;
    }

    // Build params with p, g, and public key
    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    if (!bld) {
        BN_free(p);
        BN_free(g);
        BN_free(pub);
        return;
    }

    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_P, p);
    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_G, g);
    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, pub);

    // Also carry over existing private key if present
    if (dh->pkey) {
        BIGNUM* priv = NULL;
        if (EVP_PKEY_get_bn_param(dh->pkey, OSSL_PKEY_PARAM_PRIV_KEY, &priv) == 1 && priv) {
            OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY, priv);
            BN_free(priv);
        }
    }

    OSSL_PARAM* ossl_params = OSSL_PARAM_BLD_to_param(bld);
    OSSL_PARAM_BLD_free(bld);
    BN_free(p);
    BN_free(g);
    BN_free(pub);
    if (!ossl_params)
        return;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
    if (!ctx) {
        OSSL_PARAM_free(ossl_params);
        return;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(ossl_params);
        return;
    }

    EVP_PKEY* new_pkey = NULL;
    int selection = EVP_PKEY_PUBLIC_KEY;
    // If we included a private key, use keypair selection
    if (dh->pkey) {
        BIGNUM* priv_check = NULL;
        if (EVP_PKEY_get_bn_param(dh->pkey, OSSL_PKEY_PARAM_PRIV_KEY, &priv_check) == 1 &&
            priv_check) {
            selection = EVP_PKEY_KEYPAIR;
            BN_free(priv_check);
        }
    }

    if (EVP_PKEY_fromdata(ctx, &new_pkey, selection, ossl_params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(ossl_params);
        return;
    }

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(ossl_params);

    if (dh->pkey)
        EVP_PKEY_free(dh->pkey);
    dh->pkey = new_pkey;
}

TML_EXPORT void crypto_dh_set_private_key(void* handle, void* key_handle) {
    TmlDh* dh = (TmlDh*)handle;
    TmlBuffer* key_buf = (TmlBuffer*)key_handle;
    if (!dh || !dh->params || !key_buf || key_buf->length <= 0)
        return;

    // Get existing p and g from params
    BIGNUM* p = NULL;
    BIGNUM* g = NULL;
    EVP_PKEY_get_bn_param(dh->params, OSSL_PKEY_PARAM_FFC_P, &p);
    EVP_PKEY_get_bn_param(dh->params, OSSL_PKEY_PARAM_FFC_G, &g);
    if (!p || !g) {
        BN_free(p);
        BN_free(g);
        return;
    }

    BIGNUM* priv = BN_bin2bn(key_buf->data, (int)key_buf->length, NULL);
    if (!priv) {
        BN_free(p);
        BN_free(g);
        return;
    }

    // Build params with p, g, and private key
    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    if (!bld) {
        BN_free(p);
        BN_free(g);
        BN_free(priv);
        return;
    }

    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_P, p);
    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_G, g);
    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY, priv);

    // Also carry over existing public key if present
    if (dh->pkey) {
        BIGNUM* pub = NULL;
        if (EVP_PKEY_get_bn_param(dh->pkey, OSSL_PKEY_PARAM_PUB_KEY, &pub) == 1 && pub) {
            OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, pub);
            BN_free(pub);
        }
    }

    OSSL_PARAM* ossl_params = OSSL_PARAM_BLD_to_param(bld);
    OSSL_PARAM_BLD_free(bld);
    BN_free(p);
    BN_free(g);
    BN_free(priv);
    if (!ossl_params)
        return;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
    if (!ctx) {
        OSSL_PARAM_free(ossl_params);
        return;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(ossl_params);
        return;
    }

    EVP_PKEY* new_pkey = NULL;
    int selection = EVP_PKEY_KEYPAIR;

    if (EVP_PKEY_fromdata(ctx, &new_pkey, selection, ossl_params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(ossl_params);
        return;
    }

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(ossl_params);

    if (dh->pkey)
        EVP_PKEY_free(dh->pkey);
    dh->pkey = new_pkey;
}

// ============================================================================
// 9. crypto_dh_get_prime / 10. crypto_dh_get_generator
// ============================================================================

TML_EXPORT void* crypto_dh_get_prime(void* handle) {
    TmlDh* dh = (TmlDh*)handle;
    EVP_PKEY* pkey = tml_dh_effective_pkey(dh);
    return (void*)tml_pkey_get_bn_param_buf(pkey, OSSL_PKEY_PARAM_FFC_P);
}

TML_EXPORT void* crypto_dh_get_generator(void* handle) {
    TmlDh* dh = (TmlDh*)handle;
    EVP_PKEY* pkey = tml_dh_effective_pkey(dh);
    return (void*)tml_pkey_get_bn_param_buf(pkey, OSSL_PKEY_PARAM_FFC_G);
}

// ============================================================================
// 11. crypto_dh_compute_secret - shared secret derivation
// ============================================================================

TML_EXPORT void* crypto_dh_compute_secret(void* handle, void* other_pub_handle) {
    TmlDh* dh = (TmlDh*)handle;
    TmlBuffer* other_pub_buf = (TmlBuffer*)other_pub_handle;
    if (!dh || !dh->pkey || !other_pub_buf || other_pub_buf->length <= 0)
        return NULL;

    // Build a peer EVP_PKEY from the other party's public key bytes + our params
    BIGNUM* p = NULL;
    BIGNUM* g = NULL;
    EVP_PKEY* source = dh->params ? dh->params : dh->pkey;
    EVP_PKEY_get_bn_param(source, OSSL_PKEY_PARAM_FFC_P, &p);
    EVP_PKEY_get_bn_param(source, OSSL_PKEY_PARAM_FFC_G, &g);
    if (!p || !g) {
        BN_free(p);
        BN_free(g);
        return NULL;
    }

    BIGNUM* peer_pub = BN_bin2bn(other_pub_buf->data, (int)other_pub_buf->length, NULL);
    if (!peer_pub) {
        BN_free(p);
        BN_free(g);
        return NULL;
    }

    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    if (!bld) {
        BN_free(p);
        BN_free(g);
        BN_free(peer_pub);
        return NULL;
    }

    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_P, p);
    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_G, g);
    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, peer_pub);

    OSSL_PARAM* peer_params = OSSL_PARAM_BLD_to_param(bld);
    OSSL_PARAM_BLD_free(bld);
    BN_free(p);
    BN_free(g);
    BN_free(peer_pub);
    if (!peer_params)
        return NULL;

    EVP_PKEY_CTX* from_ctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
    if (!from_ctx) {
        OSSL_PARAM_free(peer_params);
        return NULL;
    }

    if (EVP_PKEY_fromdata_init(from_ctx) <= 0) {
        EVP_PKEY_CTX_free(from_ctx);
        OSSL_PARAM_free(peer_params);
        return NULL;
    }

    EVP_PKEY* peer_pkey = NULL;
    if (EVP_PKEY_fromdata(from_ctx, &peer_pkey, EVP_PKEY_PUBLIC_KEY, peer_params) <= 0) {
        EVP_PKEY_CTX_free(from_ctx);
        OSSL_PARAM_free(peer_params);
        return NULL;
    }

    EVP_PKEY_CTX_free(from_ctx);
    OSSL_PARAM_free(peer_params);

    // Derive shared secret
    EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(dh->pkey, NULL);
    if (!derive_ctx) {
        EVP_PKEY_free(peer_pkey);
        return NULL;
    }

    if (EVP_PKEY_derive_init(derive_ctx) <= 0) {
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        return NULL;
    }

    if (EVP_PKEY_derive_set_peer(derive_ctx, peer_pkey) <= 0) {
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        return NULL;
    }

    // Determine secret length
    size_t secret_len = 0;
    if (EVP_PKEY_derive(derive_ctx, NULL, &secret_len) <= 0 || secret_len == 0) {
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        return NULL;
    }

    uint8_t* secret = (uint8_t*)malloc(secret_len);
    if (!secret) {
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        return NULL;
    }

    if (EVP_PKEY_derive(derive_ctx, secret, &secret_len) <= 0) {
        free(secret);
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        return NULL;
    }

    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(peer_pkey);

    TmlBuffer* result = tml_create_buffer_with_data(secret, (int64_t)secret_len);
    free(secret);
    return (void*)result;
}

// ============================================================================
// 12. crypto_dh_check - validate DH parameters
// ============================================================================

TML_EXPORT int64_t crypto_dh_check(void* handle) {
    TmlDh* dh = (TmlDh*)handle;
    EVP_PKEY* pkey = tml_dh_effective_pkey(dh);
    if (!pkey)
        return -1;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return -1;

    int result = EVP_PKEY_param_check(ctx);
    EVP_PKEY_CTX_free(ctx);

    // result == 1 means OK, anything else is failure
    return (result == 1) ? 0 : 1;
}

// ============================================================================
// 13. crypto_dh_destroy - free TmlDh
// ============================================================================

TML_EXPORT void crypto_dh_destroy(void* handle) {
    TmlDh* dh = (TmlDh*)handle;
    if (!dh)
        return;
    if (dh->pkey)
        EVP_PKEY_free(dh->pkey);
    if (dh->params)
        EVP_PKEY_free(dh->params);
    free(dh);
}

// ============================================================================
// 14. crypto_dh_group_get_prime / 15. crypto_dh_group_get_generator
// ============================================================================

TML_EXPORT void* crypto_dh_group_get_prime(const char* group_name) {
    if (!group_name)
        return NULL;

    // Create a temporary DH group and extract its prime
    void* dh_handle = crypto_dh_create_group(group_name);
    if (!dh_handle)
        return NULL;

    void* prime = crypto_dh_get_prime(dh_handle);
    crypto_dh_destroy(dh_handle);
    return prime;
}

TML_EXPORT void* crypto_dh_group_get_generator(const char* group_name) {
    if (!group_name)
        return NULL;

    // Create a temporary DH group and extract its generator
    void* dh_handle = crypto_dh_create_group(group_name);
    if (!dh_handle)
        return NULL;

    void* gen = crypto_dh_get_generator(dh_handle);
    crypto_dh_destroy(dh_handle);
    return gen;
}

#else /* !TML_HAS_OPENSSL */

// ============================================================================
// Stubs when OpenSSL is not available
// ============================================================================

TML_EXPORT void* crypto_dh_create(void* prime_handle, void* generator_handle) {
    (void)prime_handle;
    (void)generator_handle;
    return NULL;
}
TML_EXPORT void* crypto_dh_generate(int64_t prime_length) {
    (void)prime_length;
    return NULL;
}
TML_EXPORT void* crypto_dh_create_group(const char* group_name) {
    (void)group_name;
    return NULL;
}
TML_EXPORT void crypto_dh_generate_keys(void* handle) {
    (void)handle;
}
TML_EXPORT void* crypto_dh_get_public_key(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void* crypto_dh_get_private_key(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void crypto_dh_set_public_key(void* handle, void* key_handle) {
    (void)handle;
    (void)key_handle;
}
TML_EXPORT void crypto_dh_set_private_key(void* handle, void* key_handle) {
    (void)handle;
    (void)key_handle;
}
TML_EXPORT void* crypto_dh_get_prime(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void* crypto_dh_get_generator(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void* crypto_dh_compute_secret(void* handle, void* other_pub_handle) {
    (void)handle;
    (void)other_pub_handle;
    return NULL;
}
TML_EXPORT int64_t crypto_dh_check(void* handle) {
    (void)handle;
    return -1;
}
TML_EXPORT void crypto_dh_destroy(void* handle) {
    (void)handle;
}
TML_EXPORT void* crypto_dh_group_get_prime(const char* group_name) {
    (void)group_name;
    return NULL;
}
TML_EXPORT void* crypto_dh_group_get_generator(const char* group_name) {
    (void)group_name;
    return NULL;
}

#endif /* TML_HAS_OPENSSL */
