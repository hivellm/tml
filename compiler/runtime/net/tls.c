/**
 * @file tls.c
 * @brief TML Runtime - TLS/SSL Support via OpenSSL
 *
 * Provides Transport Layer Security (TLS) for secure network communication.
 * Wraps OpenSSL's SSL library to expose a clean C API for TML FFI.
 *
 * Features:
 * - TLS client and server contexts
 * - Certificate and key loading (PEM)
 * - Hostname verification
 * - TLS 1.2 and TLS 1.3 support
 * - ALPN protocol negotiation
 * - Peer certificate inspection
 * - Read/write on encrypted streams
 *
 * Platform support:
 * - Windows (vcpkg OpenSSL or standalone install)
 * - Linux (system OpenSSL)
 * - macOS (system or Homebrew OpenSSL)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Use mem_alloc so the memory tracker can track these allocations
extern void* mem_alloc(int64_t);

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <wincrypt.h>
#include <windows.h>
#include <winsock2.h>
#pragma comment(lib, "crypt32.lib")
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <unistd.h>
#endif

/* Only compile TLS support when OpenSSL is available */
#ifdef TML_HAS_OPENSSL

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

/* Thread-local storage for error messages */
#ifdef _WIN32
static __declspec(thread) char tls_error_buf[512];
#else
static __thread char tls_error_buf[512];
#endif

static const char* get_last_openssl_error(void) {
    unsigned long err = ERR_peek_last_error();
    if (err == 0) {
        tls_error_buf[0] = '\0';
        return tls_error_buf;
    }
    ERR_error_string_n(err, tls_error_buf, sizeof(tls_error_buf));
    return tls_error_buf;
}

/* ============================================================================
 * TLS Context (wraps SSL_CTX)
 * ============================================================================ */

#ifdef _WIN32
/**
 * Load certificates from the Windows system certificate store into OpenSSL's
 * X509_STORE. This is necessary because SSL_CTX_set_default_verify_paths()
 * looks for OpenSSL's cert bundle file which is typically absent on Windows.
 *
 * Opens the "ROOT" system store (Trusted Root CAs) and adds each certificate
 * to OpenSSL's trust store.
 *
 * Returns number of certificates loaded, or -1 on error.
 */
static int load_windows_cert_store(SSL_CTX* ctx) {
    HCERTSTORE hStore = CertOpenSystemStoreA(0, "ROOT");
    if (!hStore)
        return -1;

    X509_STORE* store = SSL_CTX_get_cert_store(ctx);
    if (!store) {
        CertCloseStore(hStore, 0);
        return -1;
    }

    int count = 0;
    PCCERT_CONTEXT pContext = NULL;
    while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
        /* Convert from Windows DER format to OpenSSL X509 */
        const unsigned char* data = pContext->pbCertEncoded;
        X509* x509 = d2i_X509(NULL, &data, (long)pContext->cbCertEncoded);
        if (x509) {
            if (X509_STORE_add_cert(store, x509) == 1) {
                count++;
            }
            X509_free(x509);
        }
    }

    CertCloseStore(hStore, 0);
    return count;
}
#endif

/**
 * Create a TLS client context.
 * Returns an opaque SSL_CTX* handle, or NULL on error.
 */
TML_EXPORT void* tls_context_client_new(void) {
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx)
        return NULL;

    /* Require TLS 1.2 minimum */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /* Load CA certificates for verification */
#ifdef _WIN32
    /* On Windows, load from the system certificate store (wincrypt) since
       SSL_CTX_set_default_verify_paths() relies on an OpenSSL cert bundle
       that is typically absent on Windows installations. */
    if (load_windows_cert_store(ctx) < 0) {
        /* Fallback to OpenSSL default paths (may work if cert bundle exists) */
        SSL_CTX_set_default_verify_paths(ctx);
    }
#else
    /* On Linux/macOS, OpenSSL's default paths find the system CA bundle */
    SSL_CTX_set_default_verify_paths(ctx);
#endif

    /* Enable certificate verification */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    return ctx;
}

/**
 * Create a TLS server context.
 * Returns an opaque SSL_CTX* handle, or NULL on error.
 */
TML_EXPORT void* tls_context_server_new(void) {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx)
        return NULL;

    /* Require TLS 1.2 minimum */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    return ctx;
}

/**
 * Free a TLS context.
 */
TML_EXPORT void tls_context_free(void* ctx_handle) {
    if (ctx_handle) {
        SSL_CTX_free((SSL_CTX*)ctx_handle);
    }
}

/**
 * Load a certificate file (PEM format) into the context.
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_certificate(void* ctx_handle, const char* cert_path) {
    if (!ctx_handle || !cert_path)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_path) != 1) {
        return -1;
    }
    return 0;
}

/**
 * Load a private key file (PEM format) into the context.
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_private_key(void* ctx_handle, const char* key_path) {
    if (!ctx_handle || !key_path)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
        return -1;
    }
    /* Verify key matches certificate */
    if (SSL_CTX_check_private_key(ctx) != 1) {
        return -1;
    }
    return 0;
}

/**
 * Load a CA certificate file or directory for peer verification.
 * file_path: PEM file with CA certificates (can be NULL)
 * dir_path: directory with hashed CA certificates (can be NULL)
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_ca(void* ctx_handle, const char* file_path,
                                      const char* dir_path) {
    if (!ctx_handle)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;

    /* If both NULL, load system defaults */
    if (!file_path && !dir_path) {
        if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
            return -1;
        }
        return 0;
    }

    if (SSL_CTX_load_verify_locations(ctx, file_path, dir_path) != 1) {
        return -1;
    }
    return 0;
}

/**
 * Set verification mode.
 * mode: 0 = none, 1 = peer (client verifies server), 2 = peer + fail_if_no_cert (server requires
 * client cert)
 */
TML_EXPORT void tls_context_set_verify_mode(void* ctx_handle, int32_t mode) {
    if (!ctx_handle)
        return;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    int ssl_mode;
    switch (mode) {
    case 0:
        ssl_mode = SSL_VERIFY_NONE;
        break;
    case 1:
        ssl_mode = SSL_VERIFY_PEER;
        break;
    case 2:
        ssl_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        break;
    default:
        ssl_mode = SSL_VERIFY_PEER;
        break;
    }
    SSL_CTX_set_verify(ctx, ssl_mode, NULL);
}

/**
 * Set minimum TLS protocol version.
 * version: 0x0301 = TLS 1.0, 0x0302 = TLS 1.1, 0x0303 = TLS 1.2, 0x0304 = TLS 1.3
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_min_version(void* ctx_handle, int32_t version) {
    if (!ctx_handle)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    if (SSL_CTX_set_min_proto_version(ctx, version) != 1) {
        return -1;
    }
    return 0;
}

/**
 * Set maximum TLS protocol version.
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_max_version(void* ctx_handle, int32_t version) {
    if (!ctx_handle)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    if (SSL_CTX_set_max_proto_version(ctx, version) != 1) {
        return -1;
    }
    return 0;
}

/**
 * Set ALPN protocols for the context.
 * protos: wire-format ALPN protocol list (length-prefixed strings concatenated)
 * protos_len: total length of protos
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_alpn(void* ctx_handle, const uint8_t* protos,
                                        int32_t protos_len) {
    if (!ctx_handle || !protos || protos_len <= 0)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    if (SSL_CTX_set_alpn_protos(ctx, protos, (unsigned int)protos_len) != 0) {
        return -1;
    }
    return 0;
}

/**
 * Set cipher list for TLS 1.2 and below.
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_ciphers(void* ctx_handle, const char* ciphers) {
    if (!ctx_handle || !ciphers)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    if (SSL_CTX_set_cipher_list(ctx, ciphers) != 1) {
        return -1;
    }
    return 0;
}

/**
 * Set cipher suites for TLS 1.3.
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_context_set_ciphersuites(void* ctx_handle, const char* ciphersuites) {
    if (!ctx_handle || !ciphersuites)
        return -1;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    if (SSL_CTX_set_ciphersuites(ctx, ciphersuites) != 1) {
        return -1;
    }
    return 0;
}

/* ============================================================================
 * TLS Stream (wraps SSL*)
 * ============================================================================ */

/**
 * Create a new TLS stream from a context and raw socket fd.
 * Returns an opaque SSL* handle, or NULL on error.
 */
TML_EXPORT void* tls_stream_new(void* ctx_handle, int64_t socket_fd) {
    if (!ctx_handle)
        return NULL;
    SSL_CTX* ctx = (SSL_CTX*)ctx_handle;
    SSL* ssl = SSL_new(ctx);
    if (!ssl)
        return NULL;

    if (SSL_set_fd(ssl, (int)socket_fd) != 1) {
        SSL_free(ssl);
        return NULL;
    }
    return ssl;
}

/**
 * Set the SNI hostname for a TLS client connection.
 * Must be called before tls_stream_connect().
 * Returns 0 on success, -1 on error.
 */
TML_EXPORT int32_t tls_stream_set_hostname(void* ssl_handle, const char* hostname) {
    if (!ssl_handle || !hostname)
        return -1;
    SSL* ssl = (SSL*)ssl_handle;

    /* Set SNI extension */
    if (SSL_set_tlsext_host_name(ssl, hostname) != 1) {
        return -1;
    }

    /* Enable hostname verification */
    SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (SSL_set1_host(ssl, hostname) != 1) {
        return -1;
    }

    return 0;
}

/**
 * Perform TLS client handshake.
 * Returns 0 on success, negative on error.
 */
TML_EXPORT int32_t tls_stream_connect(void* ssl_handle) {
    if (!ssl_handle)
        return -1;
    SSL* ssl = (SSL*)ssl_handle;
    int ret = SSL_connect(ssl);
    if (ret != 1) {
        return -1;
    }
    return 0;
}

/**
 * Perform TLS server handshake (accept incoming connection).
 * Returns 0 on success, negative on error.
 */
TML_EXPORT int32_t tls_stream_accept(void* ssl_handle) {
    if (!ssl_handle)
        return -1;
    SSL* ssl = (SSL*)ssl_handle;
    int ret = SSL_accept(ssl);
    if (ret != 1) {
        return -1;
    }
    return 0;
}

/**
 * Read data from a TLS stream.
 * Returns number of bytes read, 0 on connection close, negative on error.
 */
TML_EXPORT int64_t tls_stream_read(void* ssl_handle, uint8_t* buf, int64_t buf_len) {
    if (!ssl_handle || !buf || buf_len <= 0)
        return -1;
    SSL* ssl = (SSL*)ssl_handle;
    int ret = SSL_read(ssl, buf, (int)buf_len);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_ZERO_RETURN) {
            return 0; /* Clean shutdown */
        }
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return -2; /* Would block */
        }
        return -1; /* Error */
    }
    return (int64_t)ret;
}

/**
 * Write data to a TLS stream.
 * Returns number of bytes written, negative on error.
 */
TML_EXPORT int64_t tls_stream_write(void* ssl_handle, const uint8_t* buf, int64_t buf_len) {
    if (!ssl_handle || !buf || buf_len <= 0)
        return -1;
    SSL* ssl = (SSL*)ssl_handle;
    int ret = SSL_write(ssl, buf, (int)buf_len);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return -2; /* Would block */
        }
        return -1; /* Error */
    }
    return (int64_t)ret;
}

/**
 * Initiate a clean TLS shutdown.
 * Returns 0 on success (or shutdown-in-progress), -1 on error.
 */
TML_EXPORT int32_t tls_stream_shutdown(void* ssl_handle) {
    if (!ssl_handle)
        return -1;
    SSL* ssl = (SSL*)ssl_handle;
    int ret = SSL_shutdown(ssl);
    /* ret==0 means shutdown sent but not yet received; ret==1 means complete */
    if (ret < 0) {
        return -1;
    }
    return 0;
}

/**
 * Free a TLS stream.
 */
TML_EXPORT void tls_stream_free(void* ssl_handle) {
    if (ssl_handle) {
        SSL_free((SSL*)ssl_handle);
    }
}

/* ============================================================================
 * Lowlevel wrappers for TML (tml_ prefix for lowlevel func mapping)
 * ============================================================================ */

/**
 * Read from TLS stream (lowlevel wrapper for buffer passing).
 * Signature matches: lowlevel func tls_stream_read(ssl, buf, len) -> I64
 */
int64_t tml_tls_stream_read(void* ssl_handle, uint8_t* buf, int64_t buf_len) {
    return tls_stream_read(ssl_handle, buf, buf_len);
}

/**
 * Write to TLS stream (lowlevel wrapper for buffer passing).
 * Signature matches: lowlevel func tls_stream_write(ssl, buf, len) -> I64
 */
int64_t tml_tls_stream_write(void* ssl_handle, const uint8_t* buf, int64_t buf_len) {
    return tls_stream_write(ssl_handle, buf, buf_len);
}

/**
 * Write string to TLS stream (lowlevel wrapper).
 * Signature matches: lowlevel func tls_stream_write_str(ssl, s, len) -> I64
 */
int64_t tml_tls_stream_write_str(void* ssl_handle, const char* str, int64_t str_len) {
    return tls_stream_write(ssl_handle, (const uint8_t*)str, str_len);
}

/* ============================================================================
 * TLS Stream Inspection
 * ============================================================================ */

/**
 * Get the negotiated TLS protocol version string.
 * Returns a static string like "TLSv1.3", "TLSv1.2", etc.
 */
TML_EXPORT const char* tls_stream_get_version(void* ssl_handle) {
    if (!ssl_handle)
        return "unknown";
    SSL* ssl = (SSL*)ssl_handle;
    return SSL_get_version(ssl);
}

/**
 * Get the negotiated cipher name.
 * Returns a static string like "TLS_AES_256_GCM_SHA384".
 */
TML_EXPORT const char* tls_stream_get_cipher(void* ssl_handle) {
    if (!ssl_handle)
        return "unknown";
    SSL* ssl = (SSL*)ssl_handle;
    return SSL_get_cipher_name(ssl);
}

/**
 * Get the negotiated ALPN protocol.
 * Returns a malloc'd string, or empty string if no ALPN was negotiated.
 */
TML_EXPORT char* tls_stream_get_alpn(void* ssl_handle) {
    if (!ssl_handle) {
        char* empty = (char*)mem_alloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    SSL* ssl = (SSL*)ssl_handle;
    const unsigned char* data = NULL;
    unsigned int len = 0;
    SSL_get0_alpn_selected(ssl, &data, &len);
    if (!data || len == 0) {
        char* empty = (char*)mem_alloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    char* result = (char*)mem_alloc((int64_t)(len + 1));
    if (!result)
        return NULL;
    memcpy(result, data, len);
    result[len] = '\0';
    return result;
}

/**
 * Get peer certificate subject CN.
 * Returns a malloc'd string, or empty string if no peer cert.
 */
TML_EXPORT char* tls_stream_get_peer_cn(void* ssl_handle) {
    if (!ssl_handle) {
        char* empty = (char*)mem_alloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    SSL* ssl = (SSL*)ssl_handle;
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        char* empty = (char*)mem_alloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    X509_NAME* subject = X509_get_subject_name(cert);
    char cn[256] = {0};
    X509_NAME_get_text_by_NID(subject, NID_commonName, cn, sizeof(cn));
    X509_free(cert);

    size_t len = strlen(cn);
    char* result = (char*)mem_alloc((int64_t)(len + 1));
    if (!result)
        return NULL;
    memcpy(result, cn, len + 1);
    return result;
}

/**
 * Get peer certificate as PEM string.
 * Returns a malloc'd string, or empty string if no peer cert.
 */
TML_EXPORT char* tls_stream_get_peer_cert_pem(void* ssl_handle) {
    if (!ssl_handle) {
        char* empty = (char*)mem_alloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    SSL* ssl = (SSL*)ssl_handle;
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        char* empty = (char*)mem_alloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) {
        X509_free(cert);
        char* empty = (char*)mem_alloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    PEM_write_bio_X509(bio, cert);
    char* data = NULL;
    long len = BIO_get_mem_data(bio, &data);

    char* result = NULL;
    if (len > 0 && data) {
        result = (char*)mem_alloc((int64_t)(len + 1));
        if (result) {
            memcpy(result, data, len);
            result[len] = '\0';
        }
    } else {
        result = (char*)mem_alloc(1);
        if (result)
            result[0] = '\0';
    }

    BIO_free(bio);
    X509_free(cert);
    return result;
}

/**
 * Get the verification result of the peer certificate.
 * Returns 0 on success (X509_V_OK), positive error code on failure.
 */
TML_EXPORT int32_t tls_stream_get_verify_result(void* ssl_handle) {
    if (!ssl_handle)
        return -1;
    SSL* ssl = (SSL*)ssl_handle;
    long result = SSL_get_verify_result(ssl);
    return (int32_t)result;
}

/**
 * Check if the peer certificate verified successfully.
 * Returns 1 if verified, 0 if not.
 */
TML_EXPORT int32_t tls_stream_peer_verified(void* ssl_handle) {
    if (!ssl_handle)
        return 0;
    SSL* ssl = (SSL*)ssl_handle;
    return SSL_get_verify_result(ssl) == X509_V_OK ? 1 : 0;
}

/* ============================================================================
 * Error Helpers
 * ============================================================================ */

/**
 * Get the last TLS/SSL error message.
 * Returns a pointer to a thread-local buffer.
 */
TML_EXPORT const char* tls_get_error(void) {
    return get_last_openssl_error();
}

/**
 * Clear the OpenSSL error queue.
 */
TML_EXPORT void tls_clear_errors(void) {
    ERR_clear_error();
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * Initialize OpenSSL for TLS usage.
 * Safe to call multiple times.
 */
TML_EXPORT void tls_init(void) {
    /* OpenSSL 1.1.0+ auto-initializes, but explicit init is harmless */
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
}

#else /* !TML_HAS_OPENSSL */

/* ============================================================================
 * Stub implementations when OpenSSL is not available
 * ============================================================================ */

TML_EXPORT void tls_init(void) {}
TML_EXPORT void* tls_context_client_new(void) {
    return NULL;
}
TML_EXPORT void* tls_context_server_new(void) {
    return NULL;
}
TML_EXPORT void tls_context_free(void* h) {
    (void)h;
}
TML_EXPORT int32_t tls_context_set_certificate(void* h, const char* p) {
    (void)h;
    (void)p;
    return -1;
}
TML_EXPORT int32_t tls_context_set_private_key(void* h, const char* p) {
    (void)h;
    (void)p;
    return -1;
}
TML_EXPORT int32_t tls_context_set_ca(void* h, const char* f, const char* d) {
    (void)h;
    (void)f;
    (void)d;
    return -1;
}
TML_EXPORT void tls_context_set_verify_mode(void* h, int32_t m) {
    (void)h;
    (void)m;
}
TML_EXPORT int32_t tls_context_set_min_version(void* h, int32_t v) {
    (void)h;
    (void)v;
    return -1;
}
TML_EXPORT int32_t tls_context_set_max_version(void* h, int32_t v) {
    (void)h;
    (void)v;
    return -1;
}
TML_EXPORT int32_t tls_context_set_alpn(void* h, const uint8_t* p, int32_t l) {
    (void)h;
    (void)p;
    (void)l;
    return -1;
}
TML_EXPORT int32_t tls_context_set_ciphers(void* h, const char* c) {
    (void)h;
    (void)c;
    return -1;
}
TML_EXPORT int32_t tls_context_set_ciphersuites(void* h, const char* c) {
    (void)h;
    (void)c;
    return -1;
}
TML_EXPORT void* tls_stream_new(void* h, int64_t fd) {
    (void)h;
    (void)fd;
    return NULL;
}
TML_EXPORT int32_t tls_stream_set_hostname(void* h, const char* n) {
    (void)h;
    (void)n;
    return -1;
}
TML_EXPORT int32_t tls_stream_connect(void* h) {
    (void)h;
    return -1;
}
TML_EXPORT int32_t tls_stream_accept(void* h) {
    (void)h;
    return -1;
}
TML_EXPORT int64_t tls_stream_read(void* h, uint8_t* b, int64_t l) {
    (void)h;
    (void)b;
    (void)l;
    return -1;
}
TML_EXPORT int64_t tls_stream_write(void* h, const uint8_t* b, int64_t l) {
    (void)h;
    (void)b;
    (void)l;
    return -1;
}
TML_EXPORT int32_t tls_stream_shutdown(void* h) {
    (void)h;
    return -1;
}
TML_EXPORT void tls_stream_free(void* h) {
    (void)h;
}
TML_EXPORT const char* tls_stream_get_version(void* h) {
    (void)h;
    return "none";
}
TML_EXPORT const char* tls_stream_get_cipher(void* h) {
    (void)h;
    return "none";
}
TML_EXPORT char* tls_stream_get_alpn(void* h) {
    (void)h;
    char* e = (char*)mem_alloc(1);
    if (e)
        e[0] = '\0';
    return e;
}
TML_EXPORT char* tls_stream_get_peer_cn(void* h) {
    (void)h;
    char* e = (char*)mem_alloc(1);
    if (e)
        e[0] = '\0';
    return e;
}
TML_EXPORT char* tls_stream_get_peer_cert_pem(void* h) {
    (void)h;
    char* e = (char*)mem_alloc(1);
    if (e)
        e[0] = '\0';
    return e;
}
TML_EXPORT int32_t tls_stream_get_verify_result(void* h) {
    (void)h;
    return -1;
}
TML_EXPORT int32_t tls_stream_peer_verified(void* h) {
    (void)h;
    return 0;
}
TML_EXPORT const char* tls_get_error(void) {
    return "TLS not available (OpenSSL not found)";
}
TML_EXPORT void tls_clear_errors(void) {}

/* Lowlevel stubs */
int64_t tml_tls_stream_read(void* h, uint8_t* b, int64_t l) {
    (void)h;
    (void)b;
    (void)l;
    return -1;
}
int64_t tml_tls_stream_write(void* h, const uint8_t* b, int64_t l) {
    (void)h;
    (void)b;
    (void)l;
    return -1;
}
int64_t tml_tls_stream_write_str(void* h, const char* s, int64_t l) {
    (void)h;
    (void)s;
    (void)l;
    return -1;
}

#endif /* TML_HAS_OPENSSL */
