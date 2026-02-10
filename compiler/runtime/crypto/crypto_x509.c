/**
 * @file crypto_x509.c
 * @brief TML Runtime - X.509 Certificate Functions
 *
 * Implements X.509 certificate parsing, inspection, verification, and export:
 * - PEM/DER certificate parsing
 * - Subject/issuer field extraction (CN, O, OU, C, ST, L)
 * - Serial number, validity dates, fingerprints
 * - Public key extraction, signature algorithm, CA flag, key usage
 * - Certificate verification against keys, stores, and hostnames
 * - PEM/DER/text export
 * - Certificate store management (create, add, verify)
 * - PEM bundle parsing helpers
 *
 * Uses OpenSSL 3.0+ API.
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL

#include <openssl/asn1.h>
#include <openssl/objects.h>
#include <openssl/x509_vfy.h>
#include <time.h>

// ============================================================================
// Internal helpers
// ============================================================================

/**
 * Extract a single X509_NAME entry by NID.
 * Returns a malloc'd UTF-8 string, or tml_strdup("") if not found.
 */
static char* get_x509_name_entry(X509_NAME* name, int nid) {
    if (!name)
        return tml_strdup("");
    int idx = X509_NAME_get_index_by_NID(name, nid, -1);
    if (idx < 0)
        return tml_strdup("");
    X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
    if (!entry)
        return tml_strdup("");
    ASN1_STRING* str = X509_NAME_ENTRY_get_data(entry);
    if (!str)
        return tml_strdup("");
    unsigned char* utf8 = NULL;
    int len = ASN1_STRING_to_UTF8(&utf8, str);
    if (len < 0)
        return tml_strdup("");
    char* result = tml_strdup((char*)utf8);
    OPENSSL_free(utf8);
    return result;
}

/**
 * Compute a certificate fingerprint using the given digest.
 * Returns a malloc'd hex string (lowercase, colon-separated),
 * or tml_strdup("") on failure.
 */
static char* get_fingerprint(X509* cert, const EVP_MD* md) {
    if (!cert || !md)
        return tml_strdup("");
    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (X509_digest(cert, md, buf, &len) != 1 || len == 0) {
        return tml_strdup("");
    }
    /* Each byte -> "xx:" (3 chars) minus trailing colon, plus NUL */
    size_t hex_len = (size_t)len * 3;
    char* hex = (char*)malloc(hex_len);
    if (!hex)
        return tml_strdup("");
    size_t pos = 0;
    for (unsigned int i = 0; i < len; i++) {
        if (i > 0)
            hex[pos++] = ':';
        static const char digits[] = "0123456789abcdef";
        hex[pos++] = digits[(buf[i] >> 4) & 0x0F];
        hex[pos++] = digits[buf[i] & 0x0F];
    }
    hex[pos] = '\0';
    return hex;
}

/**
 * Convert an ASN1_TIME to an ISO 8601 string (YYYY-MM-DDTHH:MM:SSZ).
 * Returns a malloc'd string, or tml_strdup("") on failure.
 */
static char* asn1_time_to_iso8601(const ASN1_TIME* t) {
    if (!t)
        return tml_strdup("");
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    if (ASN1_TIME_to_tm(t, &tm_val) != 1)
        return tml_strdup("");
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm_val.tm_year + 1900,
             tm_val.tm_mon + 1, tm_val.tm_mday, tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);
    return tml_strdup(buf);
}

/**
 * Convert an ASN1_TIME to a Unix timestamp (seconds since epoch).
 * Returns 0 on failure.
 */
static int64_t asn1_time_to_timestamp(const ASN1_TIME* t) {
    if (!t)
        return 0;
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    if (ASN1_TIME_to_tm(t, &tm_val) != 1)
        return 0;
#ifdef _WIN32
    return (int64_t)_mkgmtime(&tm_val);
#else
    return (int64_t)timegm(&tm_val);
#endif
}

// ============================================================================
// Certificate parsing
// ============================================================================

TML_EXPORT void* crypto_x509_from_pem(const char* pem) {
    if (!pem)
        return NULL;
    BIO* bio = tml_bio_from_str(pem);
    if (!bio)
        return NULL;
    X509* cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    BIO_free(bio);
    return (void*)cert;
}

TML_EXPORT void* crypto_x509_from_der(void* buf_handle) {
    TmlBuffer* buf = (TmlBuffer*)buf_handle;
    if (!buf || buf->length <= 0)
        return NULL;
    const unsigned char* p = buf->data;
    X509* cert = d2i_X509(NULL, &p, (long)buf->length);
    return (void*)cert;
}

// ============================================================================
// Subject fields
// ============================================================================

TML_EXPORT const char* crypto_x509_get_subject(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    X509_NAME* name = X509_get_subject_name(cert);
    if (!name)
        return tml_strdup("");
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return tml_strdup("");
    X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253 & ~ASN1_STRFLGS_ESC_MSB);
    char* result = tml_bio_to_str(bio);
    BIO_free(bio);
    return result;
}

TML_EXPORT const char* crypto_x509_get_subject_cn(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_subject_name(cert), NID_commonName);
}

TML_EXPORT const char* crypto_x509_get_subject_o(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_subject_name(cert), NID_organizationName);
}

TML_EXPORT const char* crypto_x509_get_subject_ou(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_subject_name(cert), NID_organizationalUnitName);
}

TML_EXPORT const char* crypto_x509_get_subject_c(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_subject_name(cert), NID_countryName);
}

TML_EXPORT const char* crypto_x509_get_subject_st(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_subject_name(cert), NID_stateOrProvinceName);
}

TML_EXPORT const char* crypto_x509_get_subject_l(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_subject_name(cert), NID_localityName);
}

// ============================================================================
// Issuer fields
// ============================================================================

TML_EXPORT const char* crypto_x509_get_issuer(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    X509_NAME* name = X509_get_issuer_name(cert);
    if (!name)
        return tml_strdup("");
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return tml_strdup("");
    X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253 & ~ASN1_STRFLGS_ESC_MSB);
    char* result = tml_bio_to_str(bio);
    BIO_free(bio);
    return result;
}

TML_EXPORT const char* crypto_x509_get_issuer_cn(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_issuer_name(cert), NID_commonName);
}

TML_EXPORT const char* crypto_x509_get_issuer_o(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_x509_name_entry(X509_get_issuer_name(cert), NID_organizationName);
}

// ============================================================================
// Certificate metadata
// ============================================================================

TML_EXPORT const char* crypto_x509_get_serial(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    const ASN1_INTEGER* serial = X509_get0_serialNumber(cert);
    if (!serial)
        return tml_strdup("");
    BIGNUM* bn = ASN1_INTEGER_to_BN(serial, NULL);
    if (!bn)
        return tml_strdup("");
    char* hex = BN_bn2hex(bn);
    BN_free(bn);
    if (!hex)
        return tml_strdup("");
    char* result = tml_strdup(hex);
    OPENSSL_free(hex);
    return result;
}

TML_EXPORT const char* crypto_x509_get_not_before(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    const ASN1_TIME* t = X509_get0_notBefore(cert);
    return asn1_time_to_iso8601(t);
}

TML_EXPORT const char* crypto_x509_get_not_after(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    const ASN1_TIME* t = X509_get0_notAfter(cert);
    return asn1_time_to_iso8601(t);
}

TML_EXPORT int64_t crypto_x509_get_not_before_ts(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return 0;
    const ASN1_TIME* t = X509_get0_notBefore(cert);
    return asn1_time_to_timestamp(t);
}

TML_EXPORT int64_t crypto_x509_get_not_after_ts(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return 0;
    const ASN1_TIME* t = X509_get0_notAfter(cert);
    return asn1_time_to_timestamp(t);
}

// ============================================================================
// Fingerprints
// ============================================================================

TML_EXPORT const char* crypto_x509_fingerprint_sha1(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_fingerprint(cert, EVP_sha1());
}

TML_EXPORT const char* crypto_x509_fingerprint_sha256(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_fingerprint(cert, EVP_sha256());
}

TML_EXPORT const char* crypto_x509_fingerprint_sha512(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    return get_fingerprint(cert, EVP_sha512());
}

// ============================================================================
// Key and signature
// ============================================================================

TML_EXPORT void* crypto_x509_get_public_key(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return NULL;
    EVP_PKEY* pkey = X509_get_pubkey(cert);
    return (void*)pkey;
}

TML_EXPORT const char* crypto_x509_get_sig_alg(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    const X509_ALGOR* alg = X509_get0_tbs_sigalg(cert);
    if (!alg)
        return tml_strdup("");
    const ASN1_OBJECT* obj = NULL;
    X509_ALGOR_get0(&obj, NULL, NULL, alg);
    if (!obj)
        return tml_strdup("");
    int nid = OBJ_obj2nid(obj);
    if (nid == NID_undef) {
        /* Unknown OID: return the dotted numeric form */
        char oid_buf[256];
        OBJ_obj2txt(oid_buf, sizeof(oid_buf), obj, 1);
        return tml_strdup(oid_buf);
    }
    return tml_strdup(OBJ_nid2sn(nid));
}

TML_EXPORT int32_t crypto_x509_is_ca(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return 0;
    /* Check basic constraints extension for CA flag */
    BASIC_CONSTRAINTS* bc =
        (BASIC_CONSTRAINTS*)X509_get_ext_d2i(cert, NID_basic_constraints, NULL, NULL);
    if (!bc)
        return 0;
    int32_t is_ca = bc->ca ? 1 : 0;
    BASIC_CONSTRAINTS_free(bc);
    return is_ca;
}

TML_EXPORT int64_t crypto_x509_get_key_usage(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return 0;
    /* Returns a bitmask of X509v3 Key Usage bits */
    ASN1_BIT_STRING* usage = (ASN1_BIT_STRING*)X509_get_ext_d2i(cert, NID_key_usage, NULL, NULL);
    if (!usage)
        return 0;
    int64_t bits = 0;
    /* OpenSSL key usage bits are stored big-endian in the ASN1_BIT_STRING */
    if (usage->length > 0)
        bits = (int64_t)(usage->data[0]);
    if (usage->length > 1)
        bits |= ((int64_t)(usage->data[1])) << 8;
    ASN1_BIT_STRING_free(usage);
    return bits;
}

TML_EXPORT void* crypto_x509_get_san(void* handle) {
    /* SAN extraction requires List[Str] return type.
     * Returning NULL for now - will be implemented when List FFI is available. */
    (void)handle;
    return NULL;
}

// ============================================================================
// Verification
// ============================================================================

TML_EXPORT int32_t crypto_x509_verify(void* cert_handle, void* key_handle) {
    X509* cert = (X509*)cert_handle;
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    if (!cert || !pkey)
        return 0;
    return X509_verify(cert, pkey) == 1 ? 1 : 0;
}

TML_EXPORT int32_t crypto_x509_check_issued(void* cert_handle, void* issuer_handle) {
    X509* cert = (X509*)cert_handle;
    X509* issuer = (X509*)issuer_handle;
    if (!cert || !issuer)
        return 0;
    return X509_check_issued(issuer, cert) == X509_V_OK ? 1 : 0;
}

TML_EXPORT int32_t crypto_x509_check_host(void* handle, const char* hostname) {
    X509* cert = (X509*)handle;
    if (!cert || !hostname)
        return 0;
    return X509_check_host(cert, hostname, strlen(hostname), 0, NULL) == 1 ? 1 : 0;
}

TML_EXPORT int32_t crypto_x509_check_email(void* handle, const char* email) {
    X509* cert = (X509*)handle;
    if (!cert || !email)
        return 0;
    return X509_check_email(cert, email, strlen(email), 0) == 1 ? 1 : 0;
}

TML_EXPORT int32_t crypto_x509_check_ip(void* handle, const char* ip) {
    X509* cert = (X509*)handle;
    if (!cert || !ip)
        return 0;
    return X509_check_ip_asc(cert, ip, 0) == 1 ? 1 : 0;
}

TML_EXPORT int32_t crypto_x509_check_private_key(void* cert_handle, void* key_handle) {
    X509* cert = (X509*)cert_handle;
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    if (!cert || !pkey)
        return 0;
    return X509_check_private_key(cert, pkey) == 1 ? 1 : 0;
}

TML_EXPORT int32_t crypto_x509_is_valid_now(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return 0;
    /* X509_cmp_current_time returns:
     *  < 0 if the time is before current time
     *  > 0 if the time is after current time
     *  = 0 on error
     * notBefore should be <= now (cmp returns < 0 or 0-error)
     * notAfter should be >= now (cmp returns > 0)
     */
    const ASN1_TIME* not_before = X509_get0_notBefore(cert);
    const ASN1_TIME* not_after = X509_get0_notAfter(cert);
    if (!not_before || !not_after)
        return 0;
    /* pnotbefore <= now: X509_cmp_current_time(notBefore) should be < 0 */
    int before_cmp = X509_cmp_current_time(not_before);
    /* notAfter >= now: X509_cmp_current_time(notAfter) should be > 0 */
    int after_cmp = X509_cmp_current_time(not_after);
    if (before_cmp == 0 || after_cmp == 0)
        return 0; /* error */
    return (before_cmp < 0 && after_cmp > 0) ? 1 : 0;
}

// ============================================================================
// Export
// ============================================================================

TML_EXPORT const char* crypto_x509_to_pem(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return tml_strdup("");
    if (PEM_write_bio_X509(bio, cert) != 1) {
        BIO_free(bio);
        return tml_strdup("");
    }
    char* result = tml_bio_to_str(bio);
    BIO_free(bio);
    return result;
}

TML_EXPORT void* crypto_x509_to_der(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return NULL;
    unsigned char* der = NULL;
    int len = i2d_X509(cert, &der);
    if (len <= 0 || !der)
        return NULL;
    TmlBuffer* buf = tml_create_buffer_with_data(der, len);
    OPENSSL_free(der);
    return (void*)buf;
}

TML_EXPORT const char* crypto_x509_to_text(void* handle) {
    X509* cert = (X509*)handle;
    if (!cert)
        return tml_strdup("");
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return tml_strdup("");
    if (X509_print_ex(bio, cert, XN_FLAG_RFC2253, X509_FLAG_COMPAT) != 1) {
        BIO_free(bio);
        return tml_strdup("");
    }
    char* result = tml_bio_to_str(bio);
    BIO_free(bio);
    return result;
}

TML_EXPORT void crypto_x509_destroy(void* handle) {
    X509* cert = (X509*)handle;
    if (cert)
        X509_free(cert);
}

// ============================================================================
// Certificate store
// ============================================================================

TML_EXPORT void* crypto_x509_store_create(void) {
    X509_STORE* store = X509_STORE_new();
    return (void*)store;
}

TML_EXPORT void* crypto_x509_store_system(void) {
    X509_STORE* store = X509_STORE_new();
    if (!store)
        return NULL;
    if (X509_STORE_set_default_paths(store) != 1) {
        X509_STORE_free(store);
        return NULL;
    }
    return (void*)store;
}

TML_EXPORT int32_t crypto_x509_store_add_cert(void* store, void* cert) {
    X509_STORE* s = (X509_STORE*)store;
    X509* c = (X509*)cert;
    if (!s || !c)
        return 0;
    return X509_STORE_add_cert(s, c) == 1 ? 1 : 0;
}

TML_EXPORT int64_t crypto_x509_store_add_pem_file(void* store, const char* path) {
    X509_STORE* s = (X509_STORE*)store;
    if (!s || !path)
        return 0;
    /* Load all certificates from a PEM file into the store */
    BIO* bio = BIO_new_file(path, "r");
    if (!bio)
        return 0;
    int64_t count = 0;
    X509* cert = NULL;
    while ((cert = PEM_read_bio_X509(bio, NULL, NULL, NULL)) != NULL) {
        if (X509_STORE_add_cert(s, cert) == 1) {
            count++;
        }
        X509_free(cert);
    }
    /* Clear the error from the failed PEM_read at EOF */
    ERR_clear_error();
    BIO_free(bio);
    return count;
}

TML_EXPORT int32_t crypto_x509_store_verify(void* store, void* cert) {
    X509_STORE* s = (X509_STORE*)store;
    X509* c = (X509*)cert;
    if (!s || !c)
        return 0;
    X509_STORE_CTX* ctx = X509_STORE_CTX_new();
    if (!ctx)
        return 0;
    if (X509_STORE_CTX_init(ctx, s, c, NULL) != 1) {
        X509_STORE_CTX_free(ctx);
        return 0;
    }
    int32_t result = X509_verify_cert(ctx) == 1 ? 1 : 0;
    X509_STORE_CTX_free(ctx);
    return result;
}

TML_EXPORT int32_t crypto_x509_store_verify_chain(void* store, void* cert, void* chain_handles) {
    X509_STORE* s = (X509_STORE*)store;
    X509* c = (X509*)cert;
    STACK_OF(X509)* chain = (STACK_OF(X509)*)chain_handles;
    if (!s || !c)
        return 0;
    X509_STORE_CTX* ctx = X509_STORE_CTX_new();
    if (!ctx)
        return 0;
    if (X509_STORE_CTX_init(ctx, s, c, chain) != 1) {
        X509_STORE_CTX_free(ctx);
        return 0;
    }
    int32_t result = X509_verify_cert(ctx) == 1 ? 1 : 0;
    X509_STORE_CTX_free(ctx);
    return result;
}

TML_EXPORT void crypto_x509_store_destroy(void* store) {
    X509_STORE* s = (X509_STORE*)store;
    if (s)
        X509_STORE_free(s);
}

// ============================================================================
// PEM bundle helpers
// ============================================================================

TML_EXPORT int64_t crypto_x509_count_pem_certs(const char* pem) {
    if (!pem)
        return 0;
    int64_t count = 0;
    const char* p = pem;
    while ((p = strstr(p, "-----BEGIN CERTIFICATE-----")) != NULL) {
        count++;
        p += 27; /* length of "-----BEGIN CERTIFICATE-----" */
    }
    return count;
}

TML_EXPORT const char* crypto_x509_extract_pem_cert(const char* pem, int64_t index) {
    if (!pem || index < 0)
        return tml_strdup("");
    const char* begin_marker = "-----BEGIN CERTIFICATE-----";
    const char* end_marker = "-----END CERTIFICATE-----";
    size_t begin_len = 27;
    size_t end_len = 25;

    const char* p = pem;
    int64_t current = 0;
    while ((p = strstr(p, begin_marker)) != NULL) {
        if (current == index) {
            const char* end = strstr(p, end_marker);
            if (!end)
                return tml_strdup("");
            end += end_len;
            /* Include any trailing newline */
            if (*end == '\r')
                end++;
            if (*end == '\n')
                end++;
            size_t cert_len = (size_t)(end - p);
            char* result = (char*)malloc(cert_len + 1);
            if (!result)
                return tml_strdup("");
            memcpy(result, p, cert_len);
            result[cert_len] = '\0';
            return result;
        }
        p += begin_len;
        current++;
    }
    return tml_strdup("");
}

#else /* !TML_HAS_OPENSSL */

// ============================================================================
// Stubs when OpenSSL is not available
// ============================================================================

/* Certificate parsing */
TML_EXPORT void* crypto_x509_from_pem(const char* pem) {
    (void)pem;
    return NULL;
}
TML_EXPORT void* crypto_x509_from_der(void* buf) {
    (void)buf;
    return NULL;
}

/* Subject fields */
TML_EXPORT const char* crypto_x509_get_subject(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_subject_cn(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_subject_o(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_subject_ou(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_subject_c(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_subject_st(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_subject_l(void* handle) {
    (void)handle;
    return "";
}

/* Issuer fields */
TML_EXPORT const char* crypto_x509_get_issuer(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_issuer_cn(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_issuer_o(void* handle) {
    (void)handle;
    return "";
}

/* Certificate metadata */
TML_EXPORT const char* crypto_x509_get_serial(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_not_before(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_get_not_after(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT int64_t crypto_x509_get_not_before_ts(void* handle) {
    (void)handle;
    return 0;
}
TML_EXPORT int64_t crypto_x509_get_not_after_ts(void* handle) {
    (void)handle;
    return 0;
}

/* Fingerprints */
TML_EXPORT const char* crypto_x509_fingerprint_sha1(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_fingerprint_sha256(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_x509_fingerprint_sha512(void* handle) {
    (void)handle;
    return "";
}

/* Key and signature */
TML_EXPORT void* crypto_x509_get_public_key(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT const char* crypto_x509_get_sig_alg(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT int32_t crypto_x509_is_ca(void* handle) {
    (void)handle;
    return 0;
}
TML_EXPORT int64_t crypto_x509_get_key_usage(void* handle) {
    (void)handle;
    return 0;
}
TML_EXPORT void* crypto_x509_get_san(void* handle) {
    (void)handle;
    return NULL;
}

/* Verification */
TML_EXPORT int32_t crypto_x509_verify(void* cert, void* key) {
    (void)cert;
    (void)key;
    return 0;
}
TML_EXPORT int32_t crypto_x509_check_issued(void* cert, void* issuer) {
    (void)cert;
    (void)issuer;
    return 0;
}
TML_EXPORT int32_t crypto_x509_check_host(void* handle, const char* hostname) {
    (void)handle;
    (void)hostname;
    return 0;
}
TML_EXPORT int32_t crypto_x509_check_email(void* handle, const char* email) {
    (void)handle;
    (void)email;
    return 0;
}
TML_EXPORT int32_t crypto_x509_check_ip(void* handle, const char* ip) {
    (void)handle;
    (void)ip;
    return 0;
}
TML_EXPORT int32_t crypto_x509_check_private_key(void* cert, void* key) {
    (void)cert;
    (void)key;
    return 0;
}
TML_EXPORT int32_t crypto_x509_is_valid_now(void* handle) {
    (void)handle;
    return 0;
}

/* Export */
TML_EXPORT const char* crypto_x509_to_pem(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT void* crypto_x509_to_der(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT const char* crypto_x509_to_text(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT void crypto_x509_destroy(void* handle) {
    (void)handle;
}

/* Certificate store */
TML_EXPORT void* crypto_x509_store_create(void) {
    return NULL;
}
TML_EXPORT void* crypto_x509_store_system(void) {
    return NULL;
}
TML_EXPORT int32_t crypto_x509_store_add_cert(void* store, void* cert) {
    (void)store;
    (void)cert;
    return 0;
}
TML_EXPORT int64_t crypto_x509_store_add_pem_file(void* store, const char* path) {
    (void)store;
    (void)path;
    return 0;
}
TML_EXPORT int32_t crypto_x509_store_verify(void* store, void* cert) {
    (void)store;
    (void)cert;
    return 0;
}
TML_EXPORT int32_t crypto_x509_store_verify_chain(void* store, void* cert, void* chain) {
    (void)store;
    (void)cert;
    (void)chain;
    return 0;
}
TML_EXPORT void crypto_x509_store_destroy(void* store) {
    (void)store;
}

/* PEM bundle helpers */
TML_EXPORT int64_t crypto_x509_count_pem_certs(const char* pem) {
    (void)pem;
    return 0;
}
TML_EXPORT const char* crypto_x509_extract_pem_cert(const char* pem, int64_t index) {
    (void)pem;
    (void)index;
    return "";
}

#endif /* TML_HAS_OPENSSL */
