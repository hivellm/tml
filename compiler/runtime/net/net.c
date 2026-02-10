/**
 * @file net.c
 * @brief TML Runtime - Networking Functions
 *
 * Platform-independent socket operations for the TML language.
 * Supports both Windows (Winsock2) and POSIX (BSD sockets).
 *
 * ## Functions
 *
 * - `sys_socket_raw` - Create a socket
 * - `sys_bind_v4` - Bind to IPv4 address
 * - `sys_bind_v6` - Bind to IPv6 address
 * - `sys_listen_raw` - Listen for connections
 * - `sys_accept_v4` - Accept IPv4 connection
 * - `sys_connect_v4` - Connect to IPv4 address
 * - `sys_connect_v6` - Connect to IPv6 address
 * - `sys_send_raw` - Send data
 * - `sys_recv_raw` - Receive data
 * - `sys_sendto_v4` - Send to IPv4 address
 * - `sys_recvfrom_v4` - Receive from IPv4 address
 * - `sys_shutdown_raw` - Shutdown socket
 * - `sys_close_raw` - Close socket
 * - `sys_set_nonblocking_raw` - Set non-blocking mode
 * - `sys_setsockopt_raw` - Set socket option
 * - `sys_getsockopt_raw` - Get socket option
 * - `sys_setsockopt_timeout_raw` - Set socket timeout option
 * - `sys_getsockopt_timeout_raw` - Get socket timeout option
 * - `sys_getsockname_v4` - Get local IPv4 address
 * - `sys_getpeername_v4` - Get peer IPv4 address
 * - `sys_get_last_error` - Get last error code
 * - `sys_wsa_startup` - Initialize Winsock (Windows only)
 * - `sys_wsa_cleanup` - Cleanup Winsock (Windows only)
 */

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
// Windows
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

typedef SOCKET socket_t;
#define INVALID_SOCKET_VAL INVALID_SOCKET
#define SOCKET_ERROR_VAL SOCKET_ERROR

#else
// POSIX
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

typedef int socket_t;
#define INVALID_SOCKET_VAL (-1)
#define SOCKET_ERROR_VAL (-1)
#define closesocket close
#endif

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Converts a U32 IP (big-endian) and U16 port to sockaddr_in.
 */
static void make_sockaddr_v4(struct sockaddr_in* addr, uint32_t ip_bits, uint16_t port) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(ip_bits);
}

/**
 * @brief Converts a 16-byte IPv6 address and port to sockaddr_in6.
 */
static void make_sockaddr_v6(struct sockaddr_in6* addr, const uint8_t* ip_bytes, uint16_t port,
                             uint32_t flowinfo, uint32_t scope_id) {
    memset(addr, 0, sizeof(*addr));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    addr->sin6_flowinfo = htonl(flowinfo);
    memcpy(&addr->sin6_addr, ip_bytes, 16);
    addr->sin6_scope_id = scope_id;
}

// ============================================================================
// Socket Creation and Management
// ============================================================================

/**
 * @brief Creates a socket.
 * @param af Address family (AF_INET=2, AF_INET6=10 on Unix, 23 on Windows)
 * @param socket_type Socket type (SOCK_STREAM=1, SOCK_DGRAM=2)
 * @param protocol Protocol (0 for default, 6 for TCP, 17 for UDP)
 * @return Socket handle or -1 on error.
 */
int64_t sys_socket_raw(int32_t af, int32_t socket_type, int32_t protocol) {
    socket_t sock = socket(af, socket_type, protocol);
    if (sock == INVALID_SOCKET_VAL) {
        return -1;
    }
    return (int64_t)sock;
}

/**
 * @brief Binds a socket to an IPv4 address.
 * @param handle Socket handle
 * @param ip_bits IPv4 address in host byte order (big-endian representation)
 * @param port Port number
 * @return 0 on success, -1 on error.
 */
int32_t sys_bind_v4(int64_t handle, uint32_t ip_bits, uint16_t port) {
    struct sockaddr_in addr;
    make_sockaddr_v4(&addr, ip_bits, port);

    int result = bind((socket_t)handle, (struct sockaddr*)&addr, sizeof(addr));
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Binds a socket to an IPv6 address.
 */
int32_t sys_bind_v6(int64_t handle, const uint8_t* ip_bytes, uint16_t port, uint32_t flowinfo,
                    uint32_t scope_id) {
    struct sockaddr_in6 addr;
    make_sockaddr_v6(&addr, ip_bytes, port, flowinfo, scope_id);

    int result = bind((socket_t)handle, (struct sockaddr*)&addr, sizeof(addr));
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Listens for incoming connections.
 */
int32_t sys_listen_raw(int64_t handle, int32_t backlog) {
    int result = listen((socket_t)handle, backlog);
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Accepts an incoming IPv4 connection.
 * @param handle Listening socket handle
 * @param out_ip Pointer to receive peer IP address (host byte order)
 * @param out_port Pointer to receive peer port number
 * @return New socket handle or -1 on error.
 */
int64_t sys_accept_v4(int64_t handle, uint32_t* out_ip, uint16_t* out_port) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    socket_t new_sock = accept((socket_t)handle, (struct sockaddr*)&addr, &addr_len);
    if (new_sock == INVALID_SOCKET_VAL) {
        return -1;
    }

    *out_ip = ntohl(addr.sin_addr.s_addr);
    *out_port = ntohs(addr.sin_port);
    return (int64_t)new_sock;
}

/**
 * @brief Connects to an IPv4 address.
 */
int32_t sys_connect_v4(int64_t handle, uint32_t ip_bits, uint16_t port) {
    struct sockaddr_in addr;
    make_sockaddr_v4(&addr, ip_bits, port);

    int result = connect((socket_t)handle, (struct sockaddr*)&addr, sizeof(addr));
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Connects to an IPv6 address.
 */
int32_t sys_connect_v6(int64_t handle, const uint8_t* ip_bytes, uint16_t port, uint32_t flowinfo,
                       uint32_t scope_id) {
    struct sockaddr_in6 addr;
    make_sockaddr_v6(&addr, ip_bytes, port, flowinfo, scope_id);

    int result = connect((socket_t)handle, (struct sockaddr*)&addr, sizeof(addr));
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

// ============================================================================
// Data Transfer
// ============================================================================

/**
 * @brief Sends data on a connected socket.
 */
int64_t sys_send_raw(int64_t handle, const uint8_t* buf, int64_t len, int32_t flags) {
#ifdef _WIN32
    int result = send((socket_t)handle, (const char*)buf, (int)len, flags);
#else
    ssize_t result = send((socket_t)handle, buf, (size_t)len, flags);
#endif
    return (int64_t)result;
}

/**
 * @brief Receives data from a connected socket.
 */
int64_t sys_recv_raw(int64_t handle, uint8_t* buf, int64_t len, int32_t flags) {
#ifdef _WIN32
    int result = recv((socket_t)handle, (char*)buf, (int)len, flags);
#else
    ssize_t result = recv((socket_t)handle, buf, (size_t)len, flags);
#endif
    return (int64_t)result;
}

/**
 * @brief Sends data to an IPv4 address (UDP).
 */
int64_t sys_sendto_v4(int64_t handle, const uint8_t* buf, int64_t len, int32_t flags,
                      uint32_t ip_bits, uint16_t port) {
    struct sockaddr_in addr;
    make_sockaddr_v4(&addr, ip_bits, port);

#ifdef _WIN32
    int result = sendto((socket_t)handle, (const char*)buf, (int)len, flags,
                        (struct sockaddr*)&addr, sizeof(addr));
#else
    ssize_t result =
        sendto((socket_t)handle, buf, (size_t)len, flags, (struct sockaddr*)&addr, sizeof(addr));
#endif
    return (int64_t)result;
}

/**
 * @brief Receives data from an IPv4 address (UDP).
 */
int64_t sys_recvfrom_v4(int64_t handle, uint8_t* buf, int64_t len, int32_t flags, uint32_t* out_ip,
                        uint16_t* out_port) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

#ifdef _WIN32
    int result =
        recvfrom((socket_t)handle, (char*)buf, (int)len, flags, (struct sockaddr*)&addr, &addr_len);
#else
    ssize_t result =
        recvfrom((socket_t)handle, buf, (size_t)len, flags, (struct sockaddr*)&addr, &addr_len);
#endif

    if (result >= 0) {
        *out_ip = ntohl(addr.sin_addr.s_addr);
        *out_port = ntohs(addr.sin_port);
    }
    return (int64_t)result;
}

// ============================================================================
// Socket Control
// ============================================================================

/**
 * @brief Shuts down part of a socket connection.
 */
int32_t sys_shutdown_raw(int64_t handle, int32_t how) {
#ifdef _WIN32
    // Windows uses SD_RECEIVE=0, SD_SEND=1, SD_BOTH=2
    int result = shutdown((socket_t)handle, how);
#else
    // POSIX uses SHUT_RD=0, SHUT_WR=1, SHUT_RDWR=2
    int result = shutdown((socket_t)handle, how);
#endif
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Closes a socket.
 */
int32_t sys_close_raw(int64_t handle) {
    int result = closesocket((socket_t)handle);
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Sets non-blocking mode on a socket.
 */
int32_t sys_set_nonblocking_raw(int64_t handle, int32_t nonblocking) {
#ifdef _WIN32
    u_long mode = nonblocking ? 1 : 0;
    int result = ioctlsocket((socket_t)handle, FIONBIO, &mode);
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
#else
    int flags = fcntl((socket_t)handle, F_GETFL, 0);
    if (flags == -1)
        return -1;

    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    int result = fcntl((socket_t)handle, F_SETFL, flags);
    return (result == -1) ? -1 : 0;
#endif
}

/**
 * @brief Sets a socket option.
 */
int32_t sys_setsockopt_raw(int64_t handle, int32_t level, int32_t optname, int32_t value) {
#ifdef _WIN32
    // Windows SOL_SOCKET is 0xFFFF, but we use 1 in TML for compatibility
    // Map TML's SOL_SOCKET (1) to Windows' SOL_SOCKET (0xFFFF)
    if (level == 1)
        level = SOL_SOCKET;
#endif

    int result = setsockopt((socket_t)handle, level, optname, (const char*)&value, sizeof(value));
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Gets a socket option.
 */
int32_t sys_getsockopt_raw(int64_t handle, int32_t level, int32_t optname, int32_t* out_value) {
#ifdef _WIN32
    if (level == 1)
        level = SOL_SOCKET;
#endif

    int value = 0;
    socklen_t len = sizeof(value);
    int result = getsockopt((socket_t)handle, level, optname, (char*)&value, &len);
    if (result == SOCKET_ERROR_VAL) {
        return -1;
    }
    *out_value = value;
    return 0;
}

/**
 * @brief Sets a socket timeout option (in milliseconds).
 */
int32_t sys_setsockopt_timeout_raw(int64_t handle, int32_t level, int32_t optname, int64_t millis) {
#ifdef _WIN32
    if (level == 1)
        level = SOL_SOCKET;
    // Windows uses DWORD for timeouts (in milliseconds)
    DWORD timeout = (DWORD)millis;
    int result =
        setsockopt((socket_t)handle, level, optname, (const char*)&timeout, sizeof(timeout));
#else
    // POSIX uses struct timeval
    struct timeval tv;
    tv.tv_sec = millis / 1000;
    tv.tv_usec = (millis % 1000) * 1000;
    int result = setsockopt((socket_t)handle, level, optname, &tv, sizeof(tv));
#endif
    return (result == SOCKET_ERROR_VAL) ? -1 : 0;
}

/**
 * @brief Gets a socket timeout option (returns milliseconds, -1 on error).
 */
int64_t sys_getsockopt_timeout_raw(int64_t handle, int32_t level, int32_t optname) {
#ifdef _WIN32
    if (level == 1)
        level = SOL_SOCKET;
    // Windows uses DWORD for timeouts (in milliseconds)
    DWORD timeout = 0;
    socklen_t len = sizeof(timeout);
    int result = getsockopt((socket_t)handle, level, optname, (char*)&timeout, &len);
    if (result == SOCKET_ERROR_VAL) {
        return -1;
    }
    return (int64_t)timeout;
#else
    // POSIX uses struct timeval
    struct timeval tv;
    socklen_t len = sizeof(tv);
    int result = getsockopt((socket_t)handle, level, optname, &tv, &len);
    if (result == -1) {
        return -1;
    }
    return (int64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

/**
 * @brief Gets the local address of a socket (IPv4).
 */
int32_t sys_getsockname_v4(int64_t handle, uint32_t* out_ip, uint16_t* out_port) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int result = getsockname((socket_t)handle, (struct sockaddr*)&addr, &addr_len);
    if (result == SOCKET_ERROR_VAL) {
        return -1;
    }

    *out_ip = ntohl(addr.sin_addr.s_addr);
    *out_port = ntohs(addr.sin_port);
    return 0;
}

/**
 * @brief Gets the peer address of a socket (IPv4).
 */
int32_t sys_getpeername_v4(int64_t handle, uint32_t* out_ip, uint16_t* out_port) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int result = getpeername((socket_t)handle, (struct sockaddr*)&addr, &addr_len);
    if (result == SOCKET_ERROR_VAL) {
        return -1;
    }

    *out_ip = ntohl(addr.sin_addr.s_addr);
    *out_port = ntohs(addr.sin_port);
    return 0;
}

// ============================================================================
// Error Handling
// ============================================================================

/**
 * @brief Gets the last socket error code.
 */
int32_t sys_get_last_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// ============================================================================
// Platform Initialization (Windows only)
// ============================================================================

#ifdef _WIN32
static int wsa_initialized = 0;

/**
 * @brief Initializes Winsock.
 * @return 0 on success, error code on failure.
 */
int32_t sys_wsa_startup(void) {
    if (wsa_initialized)
        return 0;

    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result == 0) {
        wsa_initialized = 1;
    }
    return result;
}

/**
 * @brief Cleans up Winsock.
 */
void sys_wsa_cleanup(void) {
    if (wsa_initialized) {
        WSACleanup();
        wsa_initialized = 0;
    }
}
#endif

// ============================================================================
// TML Wrapper Functions
// ============================================================================
// These match the names generated by TML's lowlevel function declarations.
// For functions that return multiple values (accept, recvfrom, getsockname,
// getpeername, getsockopt), we use thread-local storage so TML can retrieve
// the results with separate getter calls.

// Thread-local storage for multi-value returns
#ifdef _WIN32
static __declspec(thread) uint32_t tls_addr_ip = 0;
static __declspec(thread) uint16_t tls_addr_port = 0;
static __declspec(thread) int32_t tls_sockopt_value = 0;
#else
static __thread uint32_t tls_addr_ip = 0;
static __thread uint16_t tls_addr_port = 0;
static __thread int32_t tls_sockopt_value = 0;
#endif

int64_t tml_sys_socket(int32_t family, int32_t sock_type, int32_t protocol) {
#ifdef _WIN32
    if (!wsa_initialized) {
        sys_wsa_startup();
    }
#endif
    return sys_socket_raw(family, sock_type, protocol);
}

int32_t tml_sys_set_nonblocking(int64_t handle, int32_t nonblocking) {
    return sys_set_nonblocking_raw(handle, nonblocking);
}

int32_t tml_sys_setsockopt(int64_t handle, int32_t level, int32_t optname, int32_t value) {
    return sys_setsockopt_raw(handle, level, optname, value);
}

int32_t tml_sys_bind_v4(int64_t handle, int32_t ip_bits, int32_t port) {
    return sys_bind_v4(handle, (uint32_t)ip_bits, (uint16_t)port);
}

int32_t tml_sys_listen(int64_t handle, int32_t backlog) {
    return sys_listen_raw(handle, backlog);
}

int32_t tml_sys_connect_v4(int64_t handle, int32_t ip_bits, int32_t port) {
    return sys_connect_v4(handle, (uint32_t)ip_bits, (uint16_t)port);
}

// Accept: returns new socket handle, stores peer addr in TLS
int64_t tml_sys_accept_v4(int64_t handle) {
    uint32_t ip = 0;
    uint16_t port = 0;
    int64_t result = sys_accept_v4(handle, &ip, &port);
    if (result >= 0) {
        tls_addr_ip = ip;
        tls_addr_port = port;
    }
    return result;
}

// Send/Recv (simple wrappers)
int64_t tml_sys_send(int64_t handle, const uint8_t* buf, int64_t len) {
    return sys_send_raw(handle, buf, len, 0);
}

int64_t tml_sys_recv(int64_t handle, uint8_t* buf, int64_t len) {
    return sys_recv_raw(handle, buf, len, 0);
}

int64_t tml_sys_peek(int64_t handle, uint8_t* buf, int64_t len) {
#ifdef _WIN32
    return sys_recv_raw(handle, buf, len, MSG_PEEK);
#else
    return sys_recv_raw(handle, buf, len, MSG_PEEK);
#endif
}

// SendTo/RecvFrom for UDP
int64_t tml_sys_sendto_v4(int64_t handle, const uint8_t* buf, int64_t len, int32_t ip_bits,
                          int32_t port) {
    return sys_sendto_v4(handle, buf, len, 0, (uint32_t)ip_bits, (uint16_t)port);
}

int64_t tml_sys_recvfrom_v4(int64_t handle, uint8_t* buf, int64_t len) {
    uint32_t ip = 0;
    uint16_t port = 0;
    int64_t result = sys_recvfrom_v4(handle, buf, len, 0, &ip, &port);
    if (result >= 0) {
        tls_addr_ip = ip;
        tls_addr_port = port;
    }
    return result;
}

// Socket address info (store in TLS)
int32_t tml_sys_getsockname_v4(int64_t handle) {
    uint32_t ip = 0;
    uint16_t port = 0;
    int32_t result = sys_getsockname_v4(handle, &ip, &port);
    if (result == 0) {
        tls_addr_ip = ip;
        tls_addr_port = port;
    }
    return result;
}

int32_t tml_sys_getpeername_v4(int64_t handle) {
    uint32_t ip = 0;
    uint16_t port = 0;
    int32_t result = sys_getpeername_v4(handle, &ip, &port);
    if (result == 0) {
        tls_addr_ip = ip;
        tls_addr_port = port;
    }
    return result;
}

// Getters for TLS-stored address values
int32_t tml_sys_sockaddr_get_ip(void) {
    return (int32_t)tls_addr_ip;
}

int32_t tml_sys_sockaddr_get_port(void) {
    return (int32_t)tls_addr_port;
}

// Getsockopt wrapper: stores result in TLS, returns 0/-1
int32_t tml_sys_getsockopt(int64_t handle, int32_t level, int32_t optname) {
    int32_t value = 0;
    int32_t result = sys_getsockopt_raw(handle, level, optname, &value);
    if (result == 0) {
        tls_sockopt_value = value;
    }
    return result;
}

int32_t tml_sys_getsockopt_value(void) {
    return tls_sockopt_value;
}

// Timeout socket options
int32_t tml_sys_setsockopt_timeout(int64_t handle, int32_t level, int32_t optname, int64_t millis) {
    return sys_setsockopt_timeout_raw(handle, level, optname, millis);
}

int64_t tml_sys_getsockopt_timeout(int64_t handle, int32_t level, int32_t optname) {
    return sys_getsockopt_timeout_raw(handle, level, optname);
}

// Shutdown and close
int32_t tml_sys_shutdown(int64_t handle, int32_t how) {
    return sys_shutdown_raw(handle, how);
}

int32_t tml_sys_close(int64_t handle) {
    return sys_close_raw(handle);
}

int32_t tml_sys_get_last_error(void) {
    return sys_get_last_error();
}
