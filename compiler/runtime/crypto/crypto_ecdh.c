/**
 * @file crypto_ecdh.c
 * @brief TML Runtime - Elliptic Curve Diffie-Hellman Key Exchange
 *
 * Implements ECDH key exchange for:
 * - X25519 (Curve25519)
 * - X448 (Curve448)
 * - NIST curves (prime256v1/P-256, secp384r1/P-384, secp521r1/P-521, secp256k1)
 *
 * Provides both high-level ECDH object API and low-level X25519/X448 functions
 * for raw key byte operations.
 *
 * Uses OpenSSL 3.0+ EVP API.
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL

#include <openssl/obj_mac.h>

// ============================================================================
// Internal ECDH state
// ============================================================================

typedef struct {
    EVP_PKEY* pkey;      // our key pair
    char curve_name[64]; // curve name
    int is_x25519;       // 1 if X25519
    int is_x448;         // 1 if X448
} TmlEcdh;

// ============================================================================
// Helper: map curve name to NID for NIST curves
// ============================================================================

static int ecdh_curve_to_nid(const char* curve_name) {
    if (!curve_name)
        return NID_undef;

    if (strcmp(curve_name, "prime256v1") == 0 || strcmp(curve_name, "P-256") == 0)
        return NID_X9_62_prime256v1;
    if (strcmp(curve_name, "secp384r1") == 0 || strcmp(curve_name, "P-384") == 0)
        return NID_secp384r1;
    if (strcmp(curve_name, "secp521r1") == 0 || strcmp(curve_name, "P-521") == 0)
        return NID_secp521r1;
    if (strcmp(curve_name, "secp256k1") == 0)
        return NID_secp256k1;

    // Try OpenSSL's own name resolution as fallback
    return OBJ_txt2nid(curve_name);
}

// ============================================================================
// Helper: generate EC key pair for NIST curves
// ============================================================================

static EVP_PKEY* ecdh_generate_ec_key(int nid) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, nid) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

// ============================================================================
// Helper: generate X25519 or X448 key pair
// ============================================================================

static EVP_PKEY* ecdh_generate_xdh_key(int evp_type) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(evp_type, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

// ============================================================================
// Helper: derive shared secret from our pkey + peer pkey
// ============================================================================

static TmlBuffer* ecdh_derive_secret(EVP_PKEY* our_key, EVP_PKEY* peer_key) {
    if (!our_key || !peer_key)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(our_key, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_derive_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_derive_set_peer(ctx, peer_key) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    // Determine secret length
    size_t secret_len = 0;
    if (EVP_PKEY_derive(ctx, NULL, &secret_len) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    TmlBuffer* buf = tml_create_buffer((int64_t)secret_len);
    if (!buf) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_derive(ctx, buf->data, &secret_len) <= 0) {
        free(buf->data);
        free(buf);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    buf->length = (int64_t)secret_len;
    EVP_PKEY_CTX_free(ctx);
    return buf;
}

// ============================================================================
// Helper: create EVP_PKEY for NIST curve from raw public key bytes
// ============================================================================

static EVP_PKEY* ecdh_pkey_from_ec_public_bytes(int nid, const uint8_t* data, size_t len) {
    if (!data || len == 0)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!ctx)
        return NULL;

    // Build parameters for the peer public key
    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    if (!bld) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    const char* group_name = OBJ_nid2sn(nid);
    if (!group_name) {
        OSSL_PARAM_BLD_free(bld);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME, group_name, 0);
    OSSL_PARAM_BLD_push_octet_string(bld, OSSL_PKEY_PARAM_PUB_KEY, data, len);

    OSSL_PARAM* params = OSSL_PARAM_BLD_to_param(bld);
    OSSL_PARAM_BLD_free(bld);
    if (!params) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    EVP_PKEY_CTX* from_ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    if (!from_ctx) {
        OSSL_PARAM_free(params);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    EVP_PKEY* peer = NULL;
    if (EVP_PKEY_fromdata_init(from_ctx) <= 0 ||
        EVP_PKEY_fromdata(from_ctx, &peer, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
        peer = NULL;
    }

    OSSL_PARAM_free(params);
    EVP_PKEY_CTX_free(from_ctx);
    EVP_PKEY_CTX_free(ctx);
    return peer;
}

// ============================================================================
// 1. crypto_ecdh_create - Create ECDH context for a named curve
// ============================================================================

TML_EXPORT void* crypto_ecdh_create(const char* curve_name) {
    if (!curve_name)
        return NULL;

    TmlEcdh* ecdh = (TmlEcdh*)calloc(1, sizeof(TmlEcdh));
    if (!ecdh)
        return NULL;

    strncpy(ecdh->curve_name, curve_name, sizeof(ecdh->curve_name) - 1);
    ecdh->curve_name[sizeof(ecdh->curve_name) - 1] = '\0';

    // Detect X25519 / X448
    if (strcmp(curve_name, "x25519") == 0 || strcmp(curve_name, "X25519") == 0) {
        ecdh->is_x25519 = 1;
    } else if (strcmp(curve_name, "x448") == 0 || strcmp(curve_name, "X448") == 0) {
        ecdh->is_x448 = 1;
    }

    // Generate key pair immediately
    if (ecdh->is_x25519) {
        ecdh->pkey = ecdh_generate_xdh_key(EVP_PKEY_X25519);
    } else if (ecdh->is_x448) {
        ecdh->pkey = ecdh_generate_xdh_key(EVP_PKEY_X448);
    } else {
        int nid = ecdh_curve_to_nid(curve_name);
        if (nid == NID_undef) {
            free(ecdh);
            return NULL;
        }
        ecdh->pkey = ecdh_generate_ec_key(nid);
    }

    if (!ecdh->pkey) {
        free(ecdh);
        return NULL;
    }

    return (void*)ecdh;
}

// ============================================================================
// 2. crypto_ecdh_generate_keys - Generate key pair if not already generated
// ============================================================================

TML_EXPORT void crypto_ecdh_generate_keys(void* handle) {
    TmlEcdh* ecdh = (TmlEcdh*)handle;
    if (!ecdh)
        return;

    // If already have a key, nothing to do
    if (ecdh->pkey)
        return;

    if (ecdh->is_x25519) {
        ecdh->pkey = ecdh_generate_xdh_key(EVP_PKEY_X25519);
    } else if (ecdh->is_x448) {
        ecdh->pkey = ecdh_generate_xdh_key(EVP_PKEY_X448);
    } else {
        int nid = ecdh_curve_to_nid(ecdh->curve_name);
        if (nid != NID_undef) {
            ecdh->pkey = ecdh_generate_ec_key(nid);
        }
    }
}

// ============================================================================
// 3. crypto_ecdh_get_public_key - Export public key bytes
// ============================================================================

TML_EXPORT void* crypto_ecdh_get_public_key(void* handle, const char* format) {
    TmlEcdh* ecdh = (TmlEcdh*)handle;
    if (!ecdh || !ecdh->pkey)
        return NULL;

    (void)format; // format hint (e.g. "uncompressed", "compressed") - used for NIST below

    if (ecdh->is_x25519 || ecdh->is_x448) {
        // Raw public key extraction for XDH curves
        size_t pub_len = 0;
        if (EVP_PKEY_get_raw_public_key(ecdh->pkey, NULL, &pub_len) != 1)
            return NULL;
        TmlBuffer* buf = tml_create_buffer((int64_t)pub_len);
        if (!buf)
            return NULL;
        if (EVP_PKEY_get_raw_public_key(ecdh->pkey, buf->data, &pub_len) != 1) {
            free(buf->data);
            free(buf);
            return NULL;
        }
        buf->length = (int64_t)pub_len;
        return (void*)buf;
    }

    // NIST curves: extract public key as octet string
    size_t pub_len = 0;
    if (EVP_PKEY_get_octet_string_param(ecdh->pkey, OSSL_PKEY_PARAM_PUB_KEY, NULL, 0, &pub_len) !=
        1) {
        return NULL;
    }
    TmlBuffer* buf = tml_create_buffer((int64_t)pub_len);
    if (!buf)
        return NULL;
    if (EVP_PKEY_get_octet_string_param(ecdh->pkey, OSSL_PKEY_PARAM_PUB_KEY, buf->data, pub_len,
                                        &pub_len) != 1) {
        free(buf->data);
        free(buf);
        return NULL;
    }
    buf->length = (int64_t)pub_len;
    return (void*)buf;
}

// ============================================================================
// 4. crypto_ecdh_get_private_key - Export private key bytes
// ============================================================================

TML_EXPORT void* crypto_ecdh_get_private_key(void* handle) {
    TmlEcdh* ecdh = (TmlEcdh*)handle;
    if (!ecdh || !ecdh->pkey)
        return NULL;

    if (ecdh->is_x25519 || ecdh->is_x448) {
        // Raw private key extraction for XDH curves
        size_t priv_len = 0;
        if (EVP_PKEY_get_raw_private_key(ecdh->pkey, NULL, &priv_len) != 1)
            return NULL;
        TmlBuffer* buf = tml_create_buffer((int64_t)priv_len);
        if (!buf)
            return NULL;
        if (EVP_PKEY_get_raw_private_key(ecdh->pkey, buf->data, &priv_len) != 1) {
            free(buf->data);
            free(buf);
            return NULL;
        }
        buf->length = (int64_t)priv_len;
        return (void*)buf;
    }

    // NIST curves: extract private key as BIGNUM, then to bytes
    BIGNUM* priv_bn = NULL;
    if (EVP_PKEY_get_bn_param(ecdh->pkey, OSSL_PKEY_PARAM_PRIV_KEY, &priv_bn) != 1 || !priv_bn) {
        return NULL;
    }
    int bn_len = BN_num_bytes(priv_bn);
    TmlBuffer* buf = tml_create_buffer((int64_t)bn_len);
    if (!buf) {
        BN_free(priv_bn);
        return NULL;
    }
    BN_bn2bin(priv_bn, buf->data);
    buf->length = (int64_t)bn_len;
    BN_free(priv_bn);
    return (void*)buf;
}

// ============================================================================
// 5. crypto_ecdh_set_public_key - Import public key from bytes
// ============================================================================

TML_EXPORT int32_t crypto_ecdh_set_public_key(void* handle, void* key_handle) {
    TmlEcdh* ecdh = (TmlEcdh*)handle;
    TmlBuffer* key_buf = (TmlBuffer*)key_handle;
    if (!ecdh || !key_buf || key_buf->length <= 0)
        return 0;

    // Free existing key if present
    if (ecdh->pkey) {
        EVP_PKEY_free(ecdh->pkey);
        ecdh->pkey = NULL;
    }

    if (ecdh->is_x25519) {
        ecdh->pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, key_buf->data,
                                                 (size_t)key_buf->length);
        return ecdh->pkey ? 1 : 0;
    }

    if (ecdh->is_x448) {
        ecdh->pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X448, NULL, key_buf->data,
                                                 (size_t)key_buf->length);
        return ecdh->pkey ? 1 : 0;
    }

    // NIST curves: reconstruct EVP_PKEY from public key bytes
    int nid = ecdh_curve_to_nid(ecdh->curve_name);
    if (nid == NID_undef)
        return 0;

    ecdh->pkey = ecdh_pkey_from_ec_public_bytes(nid, key_buf->data, (size_t)key_buf->length);
    return ecdh->pkey ? 1 : 0;
}

// ============================================================================
// 6. crypto_ecdh_set_private_key - Import private key from bytes
// ============================================================================

TML_EXPORT int32_t crypto_ecdh_set_private_key(void* handle, void* key_handle) {
    TmlEcdh* ecdh = (TmlEcdh*)handle;
    TmlBuffer* key_buf = (TmlBuffer*)key_handle;
    if (!ecdh || !key_buf || key_buf->length <= 0)
        return 0;

    // Free existing key if present
    if (ecdh->pkey) {
        EVP_PKEY_free(ecdh->pkey);
        ecdh->pkey = NULL;
    }

    if (ecdh->is_x25519) {
        ecdh->pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, key_buf->data,
                                                  (size_t)key_buf->length);
        return ecdh->pkey ? 1 : 0;
    }

    if (ecdh->is_x448) {
        ecdh->pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X448, NULL, key_buf->data,
                                                  (size_t)key_buf->length);
        return ecdh->pkey ? 1 : 0;
    }

    // NIST curves: reconstruct EVP_PKEY from private key bytes + curve
    int nid = ecdh_curve_to_nid(ecdh->curve_name);
    if (nid == NID_undef)
        return 0;

    BIGNUM* priv_bn = BN_bin2bn(key_buf->data, (int)key_buf->length, NULL);
    if (!priv_bn)
        return 0;

    const char* group_name = OBJ_nid2sn(nid);
    if (!group_name) {
        BN_free(priv_bn);
        return 0;
    }

    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    if (!bld) {
        BN_free(priv_bn);
        return 0;
    }

    OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME, group_name, 0);
    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY, priv_bn);

    OSSL_PARAM* params = OSSL_PARAM_BLD_to_param(bld);
    OSSL_PARAM_BLD_free(bld);
    BN_free(priv_bn);
    if (!params)
        return 0;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    if (!ctx) {
        OSSL_PARAM_free(params);
        return 0;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
        EVP_PKEY_fromdata(ctx, &ecdh->pkey, EVP_PKEY_KEYPAIR, params) <= 0) {
        ecdh->pkey = NULL;
    }

    OSSL_PARAM_free(params);
    EVP_PKEY_CTX_free(ctx);
    return ecdh->pkey ? 1 : 0;
}

// ============================================================================
// 7. crypto_ecdh_compute_secret - Compute shared secret with peer's public key
// ============================================================================

TML_EXPORT void* crypto_ecdh_compute_secret(void* handle, void* other_public_handle) {
    TmlEcdh* ecdh = (TmlEcdh*)handle;
    TmlBuffer* peer_pub = (TmlBuffer*)other_public_handle;
    if (!ecdh || !ecdh->pkey || !peer_pub || peer_pub->length <= 0)
        return NULL;

    EVP_PKEY* peer_pkey = NULL;

    if (ecdh->is_x25519) {
        peer_pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, peer_pub->data,
                                                (size_t)peer_pub->length);
    } else if (ecdh->is_x448) {
        peer_pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X448, NULL, peer_pub->data,
                                                (size_t)peer_pub->length);
    } else {
        // NIST curves: reconstruct peer EVP_PKEY from public key bytes
        int nid = ecdh_curve_to_nid(ecdh->curve_name);
        if (nid == NID_undef)
            return NULL;
        peer_pkey = ecdh_pkey_from_ec_public_bytes(nid, peer_pub->data, (size_t)peer_pub->length);
    }

    if (!peer_pkey)
        return NULL;

    TmlBuffer* secret = ecdh_derive_secret(ecdh->pkey, peer_pkey);
    EVP_PKEY_free(peer_pkey);
    return (void*)secret;
}

// ============================================================================
// 8. crypto_ecdh_destroy - Free ECDH context
// ============================================================================

TML_EXPORT void crypto_ecdh_destroy(void* handle) {
    TmlEcdh* ecdh = (TmlEcdh*)handle;
    if (!ecdh)
        return;
    if (ecdh->pkey)
        EVP_PKEY_free(ecdh->pkey);
    free(ecdh);
}

// ============================================================================
// 9. crypto_ecdh_convert_key - Convert key between formats
// ============================================================================

TML_EXPORT void* crypto_ecdh_convert_key(void* key_handle, const char* curve, const char* from_fmt,
                                         const char* to_fmt) {
    TmlBuffer* key_buf = (TmlBuffer*)key_handle;
    if (!key_buf || !curve || !from_fmt || !to_fmt)
        return NULL;
    if (key_buf->length <= 0)
        return NULL;

    // Currently only support uncompressed <-> compressed for NIST curves
    int nid = ecdh_curve_to_nid(curve);
    if (nid == NID_undef)
        return NULL;

    // Get the EC_GROUP for point conversion
    EC_GROUP* group = EC_GROUP_new_by_curve_name(nid);
    if (!group)
        return NULL;

    EC_POINT* point = EC_POINT_new(group);
    if (!point) {
        EC_GROUP_free(group);
        return NULL;
    }

    // Decode the input point
    if (EC_POINT_oct2point(group, point, key_buf->data, (size_t)key_buf->length, NULL) != 1) {
        EC_POINT_free(point);
        EC_GROUP_free(group);
        return NULL;
    }

    // Determine output format
    point_conversion_form_t form = POINT_CONVERSION_UNCOMPRESSED;
    if (strcmp(to_fmt, "compressed") == 0) {
        form = POINT_CONVERSION_COMPRESSED;
    } else if (strcmp(to_fmt, "hybrid") == 0) {
        form = POINT_CONVERSION_HYBRID;
    }

    // Encode to output format
    size_t out_len = EC_POINT_point2oct(group, point, form, NULL, 0, NULL);
    if (out_len == 0) {
        EC_POINT_free(point);
        EC_GROUP_free(group);
        return NULL;
    }

    TmlBuffer* out = tml_create_buffer((int64_t)out_len);
    if (!out) {
        EC_POINT_free(point);
        EC_GROUP_free(group);
        return NULL;
    }

    if (EC_POINT_point2oct(group, point, form, out->data, out_len, NULL) == 0) {
        free(out->data);
        free(out);
        EC_POINT_free(point);
        EC_GROUP_free(group);
        return NULL;
    }

    out->length = (int64_t)out_len;
    EC_POINT_free(point);
    EC_GROUP_free(group);
    return (void*)out;
}

// ============================================================================
// 10. crypto_x25519 - One-shot X25519 key exchange from raw key bytes
// ============================================================================

TML_EXPORT void* crypto_x25519(void* priv_handle, void* pub_handle) {
    TmlBuffer* priv_buf = (TmlBuffer*)priv_handle;
    TmlBuffer* pub_buf = (TmlBuffer*)pub_handle;
    if (!priv_buf || !pub_buf)
        return NULL;
    if (priv_buf->length < 32 || pub_buf->length < 32)
        return NULL;

    EVP_PKEY* priv = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, priv_buf->data, 32);
    if (!priv)
        return NULL;

    EVP_PKEY* pub = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, pub_buf->data, 32);
    if (!pub) {
        EVP_PKEY_free(priv);
        return NULL;
    }

    TmlBuffer* secret = ecdh_derive_secret(priv, pub);
    EVP_PKEY_free(priv);
    EVP_PKEY_free(pub);
    return (void*)secret;
}

// ============================================================================
// 11. crypto_x448 - One-shot X448 key exchange from raw key bytes
// ============================================================================

TML_EXPORT void* crypto_x448(void* priv_handle, void* pub_handle) {
    TmlBuffer* priv_buf = (TmlBuffer*)priv_handle;
    TmlBuffer* pub_buf = (TmlBuffer*)pub_handle;
    if (!priv_buf || !pub_buf)
        return NULL;
    if (priv_buf->length < 56 || pub_buf->length < 56)
        return NULL;

    EVP_PKEY* priv = EVP_PKEY_new_raw_private_key(EVP_PKEY_X448, NULL, priv_buf->data, 56);
    if (!priv)
        return NULL;

    EVP_PKEY* pub = EVP_PKEY_new_raw_public_key(EVP_PKEY_X448, NULL, pub_buf->data, 56);
    if (!pub) {
        EVP_PKEY_free(priv);
        return NULL;
    }

    TmlBuffer* secret = ecdh_derive_secret(priv, pub);
    EVP_PKEY_free(priv);
    EVP_PKEY_free(pub);
    return (void*)secret;
}

// ============================================================================
// 12. crypto_x25519_generate_private - Generate random X25519 private key
// ============================================================================

TML_EXPORT void* crypto_x25519_generate_private(void) {
    EVP_PKEY* pkey = ecdh_generate_xdh_key(EVP_PKEY_X25519);
    if (!pkey)
        return NULL;

    size_t priv_len = 32;
    TmlBuffer* buf = tml_create_buffer(32);
    if (!buf) {
        EVP_PKEY_free(pkey);
        return NULL;
    }

    if (EVP_PKEY_get_raw_private_key(pkey, buf->data, &priv_len) != 1) {
        free(buf->data);
        free(buf);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    buf->length = (int64_t)priv_len;
    EVP_PKEY_free(pkey);
    return (void*)buf;
}

// ============================================================================
// 13. crypto_x25519_public_from_private - Derive X25519 public key from private
// ============================================================================

TML_EXPORT void* crypto_x25519_public_from_private(void* priv_handle) {
    TmlBuffer* priv_buf = (TmlBuffer*)priv_handle;
    if (!priv_buf || priv_buf->length < 32)
        return NULL;

    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, priv_buf->data, 32);
    if (!pkey)
        return NULL;

    size_t pub_len = 32;
    TmlBuffer* buf = tml_create_buffer(32);
    if (!buf) {
        EVP_PKEY_free(pkey);
        return NULL;
    }

    if (EVP_PKEY_get_raw_public_key(pkey, buf->data, &pub_len) != 1) {
        free(buf->data);
        free(buf);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    buf->length = (int64_t)pub_len;
    EVP_PKEY_free(pkey);
    return (void*)buf;
}

// ============================================================================
// 14. crypto_x448_generate_private - Generate random X448 private key
// ============================================================================

TML_EXPORT void* crypto_x448_generate_private(void) {
    EVP_PKEY* pkey = ecdh_generate_xdh_key(EVP_PKEY_X448);
    if (!pkey)
        return NULL;

    size_t priv_len = 56;
    TmlBuffer* buf = tml_create_buffer(56);
    if (!buf) {
        EVP_PKEY_free(pkey);
        return NULL;
    }

    if (EVP_PKEY_get_raw_private_key(pkey, buf->data, &priv_len) != 1) {
        free(buf->data);
        free(buf);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    buf->length = (int64_t)priv_len;
    EVP_PKEY_free(pkey);
    return (void*)buf;
}

// ============================================================================
// 15. crypto_x448_public_from_private - Derive X448 public key from private
// ============================================================================

TML_EXPORT void* crypto_x448_public_from_private(void* priv_handle) {
    TmlBuffer* priv_buf = (TmlBuffer*)priv_handle;
    if (!priv_buf || priv_buf->length < 56)
        return NULL;

    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X448, NULL, priv_buf->data, 56);
    if (!pkey)
        return NULL;

    size_t pub_len = 56;
    TmlBuffer* buf = tml_create_buffer(56);
    if (!buf) {
        EVP_PKEY_free(pkey);
        return NULL;
    }

    if (EVP_PKEY_get_raw_public_key(pkey, buf->data, &pub_len) != 1) {
        free(buf->data);
        free(buf);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    buf->length = (int64_t)pub_len;
    EVP_PKEY_free(pkey);
    return (void*)buf;
}

// NOTE: crypto_get_curves removed (Phase 43). TML builds curve list in pure TML:
//   std::crypto::ecdh::get_curves()

// ============================================================================
// 17. crypto_is_curve_supported - Check if a curve name is supported
// ============================================================================

TML_EXPORT int32_t crypto_is_curve_supported(const char* curve_name) {
    if (!curve_name)
        return 0;

    // Check X25519 / X448
    if (strcmp(curve_name, "x25519") == 0 || strcmp(curve_name, "X25519") == 0)
        return 1;
    if (strcmp(curve_name, "x448") == 0 || strcmp(curve_name, "X448") == 0)
        return 1;

    // Check NIST curves by NID
    int nid = ecdh_curve_to_nid(curve_name);
    if (nid != NID_undef) {
        // Verify OpenSSL actually supports this curve
        EC_GROUP* group = EC_GROUP_new_by_curve_name(nid);
        if (group) {
            EC_GROUP_free(group);
            return 1;
        }
    }

    return 0;
}

#else /* !TML_HAS_OPENSSL */

// ============================================================================
// Stubs when OpenSSL is not available
// ============================================================================

TML_EXPORT void* crypto_ecdh_create(const char* curve_name) {
    (void)curve_name;
    return NULL;
}
TML_EXPORT void crypto_ecdh_generate_keys(void* handle) {
    (void)handle;
}
TML_EXPORT void* crypto_ecdh_get_public_key(void* handle, const char* format) {
    (void)handle;
    (void)format;
    return NULL;
}
TML_EXPORT void* crypto_ecdh_get_private_key(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT int32_t crypto_ecdh_set_public_key(void* handle, void* key_handle) {
    (void)handle;
    (void)key_handle;
    return 0;
}
TML_EXPORT int32_t crypto_ecdh_set_private_key(void* handle, void* key_handle) {
    (void)handle;
    (void)key_handle;
    return 0;
}
TML_EXPORT void* crypto_ecdh_compute_secret(void* handle, void* other_public_handle) {
    (void)handle;
    (void)other_public_handle;
    return NULL;
}
TML_EXPORT void crypto_ecdh_destroy(void* handle) {
    (void)handle;
}
TML_EXPORT void* crypto_ecdh_convert_key(void* key_handle, const char* curve, const char* from_fmt,
                                         const char* to_fmt) {
    (void)key_handle;
    (void)curve;
    (void)from_fmt;
    (void)to_fmt;
    return NULL;
}
TML_EXPORT void* crypto_x25519(void* priv_handle, void* pub_handle) {
    (void)priv_handle;
    (void)pub_handle;
    return NULL;
}
TML_EXPORT void* crypto_x448(void* priv_handle, void* pub_handle) {
    (void)priv_handle;
    (void)pub_handle;
    return NULL;
}
TML_EXPORT void* crypto_x25519_generate_private(void) {
    return NULL;
}
TML_EXPORT void* crypto_x25519_public_from_private(void* priv_handle) {
    (void)priv_handle;
    return NULL;
}
TML_EXPORT void* crypto_x448_generate_private(void) {
    return NULL;
}
TML_EXPORT void* crypto_x448_public_from_private(void* priv_handle) {
    (void)priv_handle;
    return NULL;
}
TML_EXPORT int32_t crypto_is_curve_supported(const char* curve_name) {
    (void)curve_name;
    return 0;
}

#endif /* TML_HAS_OPENSSL */
