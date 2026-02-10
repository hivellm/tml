/**
 * @file dns.c
 * @brief TML Runtime - DNS Resolution Functions
 *
 * Platform-independent DNS resolution for the TML language.
 * Uses getaddrinfo/getnameinfo (Winsock2 on Windows, POSIX on Unix).
 *
 * ## Functions
 *
 * - `tml_sys_dns_lookup4`      - Lookup hostname -> first IPv4 address
 * - `tml_sys_dns_lookup6_hi`   - Lookup hostname -> first IPv6 high 8 bytes
 * - `tml_sys_dns_lookup6_lo`   - Get low 8 bytes of last IPv6 lookup
 * - `tml_sys_dns_lookup_all`   - Lookup hostname -> all addresses (TLS buffer)
 * - `tml_sys_dns_result_family` - Get address family of result at index
 * - `tml_sys_dns_result_v4`    - Get IPv4 address of result at index
 * - `tml_sys_dns_result_v6_hi` - Get IPv6 high 8 bytes of result at index
 * - `tml_sys_dns_result_v6_lo` - Get IPv6 low 8 bytes of result at index
 * - `tml_sys_dns_reverse4`     - Reverse DNS for IPv4 address
 * - `tml_sys_dns_reverse6`     - Reverse DNS for IPv6 address
 * - `tml_sys_dns_get_last_error` - Get last DNS error code
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
// Windows
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
// POSIX
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

// ============================================================================
// Winsock Initialization (Windows only)
// ============================================================================

#ifdef _WIN32
static int dns_wsa_initialized = 0;

static void dns_ensure_wsa(void) {
    if (!dns_wsa_initialized) {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0) {
            dns_wsa_initialized = 1;
        }
    }
}
#else
static inline void dns_ensure_wsa(void) { /* no-op on POSIX */ }
#endif

// ============================================================================
// Thread-Local Storage
// ============================================================================

#define DNS_MAX_RESULTS 32

#ifdef _WIN32
#define TLS_VAR __declspec(thread)
#else
#define TLS_VAR __thread
#endif

typedef struct {
    int family; // AF_INET or AF_INET6
    union {
        uint32_t v4; // IPv4 address in host byte order
        struct {
            int64_t hi; // IPv6 high 8 bytes
            int64_t lo; // IPv6 low 8 bytes
        } v6;
    } addr;
} DnsResult;

static TLS_VAR DnsResult tls_dns_results[DNS_MAX_RESULTS];
static TLS_VAR int32_t tls_dns_result_count = 0;
static TLS_VAR int64_t tls_dns_last_v6_lo = 0;
static TLS_VAR int32_t tls_dns_last_error = 0;
static TLS_VAR char tls_dns_hostname_buf[1025]; // NI_MAXHOST

// ============================================================================
// IPv6 Helpers
// ============================================================================

// Extract high 8 bytes of IPv6 as int64
static int64_t ipv6_to_hi(const struct in6_addr* addr) {
    const uint8_t* b = addr->s6_addr;
    return ((int64_t)b[0] << 56) | ((int64_t)b[1] << 48) | ((int64_t)b[2] << 40) |
           ((int64_t)b[3] << 32) | ((int64_t)b[4] << 24) | ((int64_t)b[5] << 16) |
           ((int64_t)b[6] << 8) | (int64_t)b[7];
}

// Extract low 8 bytes of IPv6 as int64
static int64_t ipv6_to_lo(const struct in6_addr* addr) {
    const uint8_t* b = addr->s6_addr;
    return ((int64_t)b[8] << 56) | ((int64_t)b[9] << 48) | ((int64_t)b[10] << 40) |
           ((int64_t)b[11] << 32) | ((int64_t)b[12] << 24) | ((int64_t)b[13] << 16) |
           ((int64_t)b[14] << 8) | (int64_t)b[15];
}

// ============================================================================
// Forward Lookups
// ============================================================================

// Lookup hostname -> first IPv4 address (host byte order), or -1 on error
int64_t tml_sys_dns_lookup4(const char* hostname) {
    dns_ensure_wsa();
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || !res) {
        tls_dns_last_error = rc;
        if (res)
            freeaddrinfo(res);
        return -1;
    }

    struct sockaddr_in* sin = (struct sockaddr_in*)res->ai_addr;
    uint32_t addr = ntohl(sin->sin_addr.s_addr);
    freeaddrinfo(res);
    return (int64_t)addr;
}

// Lookup hostname -> first IPv6 high 8 bytes, or -1 on error
int64_t tml_sys_dns_lookup6_hi(const char* hostname) {
    dns_ensure_wsa();
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || !res) {
        tls_dns_last_error = rc;
        if (res)
            freeaddrinfo(res);
        return -1;
    }

    struct sockaddr_in6* sin6 = (struct sockaddr_in6*)res->ai_addr;
    int64_t hi = ipv6_to_hi(&sin6->sin6_addr);
    tls_dns_last_v6_lo = ipv6_to_lo(&sin6->sin6_addr);
    freeaddrinfo(res);
    return hi;
}

// Get low 8 bytes of last IPv6 lookup result
int64_t tml_sys_dns_lookup6_lo(void) {
    return tls_dns_last_v6_lo;
}

// ============================================================================
// Bulk Lookups
// ============================================================================

// Lookup hostname -> all addresses; returns count or -1
int32_t tml_sys_dns_lookup_all(const char* hostname, int32_t family_hint, int32_t max_results) {
    dns_ensure_wsa();
    struct addrinfo hints, *res = NULL, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = (family_hint == 0)   ? AF_UNSPEC
                      : (family_hint == 4) ? AF_INET
                      : (family_hint == 6) ? AF_INET6
                                           : AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || !res) {
        tls_dns_last_error = rc;
        tls_dns_result_count = 0;
        if (res)
            freeaddrinfo(res);
        return -1;
    }

    int32_t count = 0;
    int32_t limit =
        (max_results > 0 && max_results < DNS_MAX_RESULTS) ? max_results : DNS_MAX_RESULTS;

    for (rp = res; rp != NULL && count < limit; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in* sin = (struct sockaddr_in*)rp->ai_addr;
            tls_dns_results[count].family = AF_INET;
            tls_dns_results[count].addr.v4 = ntohl(sin->sin_addr.s_addr);
            count++;
        } else if (rp->ai_family == AF_INET6) {
            struct sockaddr_in6* sin6 = (struct sockaddr_in6*)rp->ai_addr;
            tls_dns_results[count].family = AF_INET6;
            tls_dns_results[count].addr.v6.hi = ipv6_to_hi(&sin6->sin6_addr);
            tls_dns_results[count].addr.v6.lo = ipv6_to_lo(&sin6->sin6_addr);
            count++;
        }
    }

    freeaddrinfo(res);
    tls_dns_result_count = count;
    return count;
}

// ============================================================================
// Result Accessors
// ============================================================================

// Get address family of result at index. Returns 2 (IPv4) or 23 (IPv6), or -1
int32_t tml_sys_dns_result_family(int32_t index) {
    if (index < 0 || index >= tls_dns_result_count)
        return -1;
#ifdef _WIN32
    return (tls_dns_results[index].family == AF_INET) ? AF_INET : AF_INET6;
#else
    return (tls_dns_results[index].family == AF_INET) ? 2 : 23;
#endif
}

// Get IPv4 address of result at index
int64_t tml_sys_dns_result_v4(int32_t index) {
    if (index < 0 || index >= tls_dns_result_count)
        return -1;
    if (tls_dns_results[index].family != AF_INET)
        return -1;
    return (int64_t)tls_dns_results[index].addr.v4;
}

// Get IPv6 high 8 bytes of result at index
int64_t tml_sys_dns_result_v6_hi(int32_t index) {
    if (index < 0 || index >= tls_dns_result_count)
        return -1;
    if (tls_dns_results[index].family != AF_INET6)
        return -1;
    return tls_dns_results[index].addr.v6.hi;
}

// Get IPv6 low 8 bytes of result at index
int64_t tml_sys_dns_result_v6_lo(int32_t index) {
    if (index < 0 || index >= tls_dns_result_count)
        return -1;
    if (tls_dns_results[index].family != AF_INET6)
        return -1;
    return tls_dns_results[index].addr.v6.lo;
}

// ============================================================================
// Reverse DNS
// ============================================================================

// Reverse DNS for IPv4 address (4 octets) -> hostname
const char* tml_sys_dns_reverse4(int32_t a, int32_t b, int32_t c, int32_t d) {
    dns_ensure_wsa();
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    uint32_t ip = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
    sa.sin_addr.s_addr = htonl(ip);

    int rc = getnameinfo((struct sockaddr*)&sa, sizeof(sa), tls_dns_hostname_buf, 1025, NULL, 0, 0);
    if (rc != 0) {
        tls_dns_last_error = rc;
        tls_dns_hostname_buf[0] = '\0';
        return tls_dns_hostname_buf;
    }
    return tls_dns_hostname_buf;
}

// Reverse DNS for IPv6 address (hi/lo 64-bit halves) -> hostname
const char* tml_sys_dns_reverse6(int64_t hi, int64_t lo) {
    dns_ensure_wsa();
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;

    uint8_t* b = sa.sin6_addr.s6_addr;
    b[0] = (uint8_t)(hi >> 56);
    b[1] = (uint8_t)(hi >> 48);
    b[2] = (uint8_t)(hi >> 40);
    b[3] = (uint8_t)(hi >> 32);
    b[4] = (uint8_t)(hi >> 24);
    b[5] = (uint8_t)(hi >> 16);
    b[6] = (uint8_t)(hi >> 8);
    b[7] = (uint8_t)hi;
    b[8] = (uint8_t)(lo >> 56);
    b[9] = (uint8_t)(lo >> 48);
    b[10] = (uint8_t)(lo >> 40);
    b[11] = (uint8_t)(lo >> 32);
    b[12] = (uint8_t)(lo >> 24);
    b[13] = (uint8_t)(lo >> 16);
    b[14] = (uint8_t)(lo >> 8);
    b[15] = (uint8_t)lo;

    int rc = getnameinfo((struct sockaddr*)&sa, sizeof(sa), tls_dns_hostname_buf, 1025, NULL, 0, 0);
    if (rc != 0) {
        tls_dns_last_error = rc;
        tls_dns_hostname_buf[0] = '\0';
        return tls_dns_hostname_buf;
    }
    return tls_dns_hostname_buf;
}

// ============================================================================
// Error Handling
// ============================================================================

// Get last DNS error code
int32_t tml_sys_dns_get_last_error(void) {
    return tls_dns_last_error;
}
