/**
 * @file inspector.c
 * @brief TML Inspector — Chrome DevTools Protocol over WebSocket
 *
 * Minimal WebSocket server (RFC 6455) speaking Chrome DevTools Protocol.
 * Runs on a background thread, accepts one client at a time.
 *
 * Public API:
 *   tml_inspector_init(port)          — start server
 *   tml_inspector_shutdown()          — stop server
 *   tml_inspector_wait_for_debugger() — block until client connects
 *   tml_inspector_url()               — get ws:// URL
 *   tml_inspector_is_active()         — check if server is running
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define INVALID_SOCK INVALID_SOCKET
#define sock_close closesocket
#define sock_error WSAGetLastError()
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCK (-1)
#define sock_close close
#define sock_error errno
#endif

// ============================================================================
// State
// ============================================================================

static volatile int g_active = 0;
static volatile int g_client_connected = 0;
static volatile int g_shutdown_requested = 0;
static socket_t g_server_sock = INVALID_SOCK;
static socket_t g_client_sock = INVALID_SOCK;
static int g_port = 9229;
static char g_url[256] = {0};

#ifdef _WIN32
static HANDLE g_thread = NULL;
static HANDLE g_connect_event = NULL;
#else
static pthread_t g_thread;
static pthread_mutex_t g_connect_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_connect_cond = PTHREAD_COND_INITIALIZER;
#endif

// ============================================================================
// Embedded SHA-1 (RFC 3174) — needed for WebSocket handshake
// ============================================================================

static void sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476, h4 = 0xC3D2E1F0;

    // Pre-processing: pad message
    size_t new_len = len + 1;
    while (new_len % 64 != 56)
        new_len++;
    uint8_t* msg = (uint8_t*)calloc(new_len + 8, 1);
    if (!msg)
        return;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        msg[new_len + 7 - i] = (uint8_t)(bits >> (i * 8));

    // Process each 512-bit block
    for (size_t offset = 0; offset < new_len + 8; offset += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[offset + i * 4] << 24) |
                   ((uint32_t)msg[offset + i * 4 + 1] << 16) |
                   ((uint32_t)msg[offset + i * 4 + 2] << 8) | ((uint32_t)msg[offset + i * 4 + 3]);
        for (int i = 16; i < 80; i++) {
            uint32_t t = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (t << 1) | (t >> 31);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }
    free(msg);

    uint32_t h[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++) {
        out[i * 4] = (uint8_t)(h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(h[i]);
    }
}

// ============================================================================
// Embedded Base64 encoder
// ============================================================================

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t* in, int in_len, char* out, int out_size) {
    int i = 0, j = 0;
    while (i < in_len) {
        uint32_t a = i < in_len ? in[i++] : 0;
        uint32_t b = i < in_len ? in[i++] : 0;
        uint32_t c = i < in_len ? in[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        if (j + 4 >= out_size)
            return -1;
        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = (i - 2 > in_len) ? '=' : b64_table[(triple >> 6) & 0x3F];
        out[j++] = (i - 1 > in_len) ? '=' : b64_table[triple & 0x3F];
    }
    out[j] = '\0';
    return j;
}

// ============================================================================
// WebSocket frame operations (RFC 6455 §5)
// ============================================================================

// Send a WebSocket text frame (server->client, NOT masked)
static int ws_send_text(socket_t sock, const char* data, int len) {
    uint8_t header[10];
    int hlen = 0;

    header[0] = 0x81; // FIN + text opcode
    if (len < 126) {
        header[1] = (uint8_t)len;
        hlen = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (uint8_t)(len >> 8);
        header[3] = (uint8_t)(len & 0xFF);
        hlen = 4;
    } else {
        header[1] = 127;
        memset(header + 2, 0, 4);
        header[6] = (uint8_t)(len >> 24);
        header[7] = (uint8_t)(len >> 16);
        header[8] = (uint8_t)(len >> 8);
        header[9] = (uint8_t)(len);
        hlen = 10;
    }

    if (send(sock, (const char*)header, hlen, 0) != hlen)
        return -1;
    if (send(sock, data, len, 0) != len)
        return -1;
    return 0;
}

// Read a WebSocket frame (client->server, MASKED per RFC 6455)
// Returns payload length, -1 on error, 0 on close
static int ws_read_frame(socket_t sock, char* buf, int buf_size, int* opcode) {
    uint8_t h[2];
    int r = recv(sock, (char*)h, 2, 0);
    if (r <= 0)
        return -1;

    *opcode = h[0] & 0x0F;
    int masked = (h[1] >> 7) & 1;
    uint64_t payload_len = h[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        if (recv(sock, (char*)ext, 2, 0) != 2)
            return -1;
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (recv(sock, (char*)ext, 8, 0) != 8)
            return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | ext[i];
    }

    uint8_t mask_key[4] = {0};
    if (masked) {
        if (recv(sock, (char*)mask_key, 4, 0) != 4)
            return -1;
    }

    if ((int64_t)payload_len >= buf_size)
        return -1; // Too large

    int total = 0;
    while (total < (int)payload_len) {
        r = recv(sock, buf + total, (int)(payload_len - total), 0);
        if (r <= 0)
            return -1;
        total += r;
    }

    // Unmask
    if (masked) {
        for (int i = 0; i < total; i++)
            buf[i] ^= mask_key[i % 4];
    }
    buf[total] = '\0';

    // Handle control frames
    if (*opcode == 0x8)
        return 0;         // Close
    if (*opcode == 0x9) { // Ping -> Pong
        uint8_t pong[2] = {0x8A, 0x00};
        send(sock, (const char*)pong, 2, 0);
        return ws_read_frame(sock, buf, buf_size, opcode); // Read next frame
    }

    return total;
}

// ============================================================================
// WebSocket HTTP upgrade handshake (RFC 6455 §4.2)
// ============================================================================

static const char* WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static int ws_handshake(socket_t client) {
    char req[4096];
    int total = 0;
    // Read HTTP request until \r\n\r\n
    while (total < (int)sizeof(req) - 1) {
        int r = recv(client, req + total, 1, 0);
        if (r <= 0)
            return -1;
        total += r;
        if (total >= 4 && memcmp(req + total - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    req[total] = '\0';

    // Check for /json/version endpoint (Chrome DevTools discovery)
    if (strstr(req, "GET /json/version") || strstr(req, "GET /json HTTP")) {
        const char* json_resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n\r\n"
            "[{\"description\":\"TML Inspector\","
            "\"devtoolsFrontendUrl\":\"devtools://devtools/bundled/inspector.html\","
            "\"id\":\"tml-main\","
            "\"title\":\"TML Program\","
            "\"type\":\"node\","
            "\"webSocketDebuggerUrl\":\"ws://127.0.0.1:%d/tml\"}]";
        char buf[1024];
        snprintf(buf, sizeof(buf), json_resp, g_port);
        send(client, buf, (int)strlen(buf), 0);
        return -1; // Close after JSON response
    }

    // Extract Sec-WebSocket-Key
    const char* key_header = strstr(req, "Sec-WebSocket-Key: ");
    if (!key_header)
        return -1;
    key_header += 19;
    const char* key_end = strstr(key_header, "\r\n");
    if (!key_end)
        return -1;
    int key_len = (int)(key_end - key_header);

    // Compute accept: SHA1(key + magic) -> base64
    char concat[256];
    memcpy(concat, key_header, key_len);
    memcpy(concat + key_len, WS_MAGIC, 36);
    concat[key_len + 36] = '\0';

    uint8_t hash[20];
    sha1((const uint8_t*)concat, key_len + 36, hash);

    char accept[64];
    base64_encode(hash, 20, accept, sizeof(accept));

    // Send 101 Switching Protocols
    char resp[512];
    snprintf(resp, sizeof(resp),
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n",
             accept);
    send(client, resp, (int)strlen(resp), 0);
    return 0;
}

// ============================================================================
// CDP message handling (Chrome DevTools Protocol)
// ============================================================================

// Send a CDP result response
static void cdp_respond(socket_t client, int id, const char* result_json) {
    char buf[4096];
    if (result_json && result_json[0]) {
        snprintf(buf, sizeof(buf), "{\"id\":%d,\"result\":%s}", id, result_json);
    } else {
        snprintf(buf, sizeof(buf), "{\"id\":%d,\"result\":{}}", id);
    }
    ws_send_text(client, buf, (int)strlen(buf));
}

// Send a CDP event
static void cdp_event(socket_t client, const char* method, const char* params_json) {
    char buf[4096];
    if (params_json && params_json[0]) {
        snprintf(buf, sizeof(buf), "{\"method\":\"%s\",\"params\":%s}", method, params_json);
    } else {
        snprintf(buf, sizeof(buf), "{\"method\":\"%s\",\"params\":{}}", method);
    }
    ws_send_text(client, buf, (int)strlen(buf));
}

static void cdp_handle_message(socket_t client, const char* json, int len) {
    (void)len;

    // Extract "method":"..."
    const char* m = strstr(json, "\"method\":\"");
    if (!m)
        return;
    m += 10;
    const char* me = strchr(m, '"');
    if (!me)
        return;
    int mlen = (int)(me - m);

    // Extract "id":N
    int id = 0;
    const char* idp = strstr(json, "\"id\":");
    if (idp)
        id = atoi(idp + 5);

    // ---- Runtime domain ----
    if (mlen == 14 && strncmp(m, "Runtime.enable", 14) == 0) {
        cdp_respond(client, id, NULL);
        // Send executionContextCreated event
        cdp_event(client, "Runtime.executionContextCreated",
                  "{\"context\":{\"id\":1,\"origin\":\"\",\"name\":\"TML Main\","
                  "\"uniqueId\":\"tml-main-ctx\",\"auxData\":{\"isDefault\":true}}}");
    } else if (mlen == 15 && strncmp(m, "Runtime.disable", 15) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 20 && strncmp(m, "Runtime.getHeapUsage", 20) == 0) {
        cdp_respond(client, id, "{\"usedSize\":0,\"totalSize\":0}");
    } else if (mlen == 27 && strncmp(m, "Runtime.runIfWaitingForDebugger", 27) == 0) {
        cdp_respond(client, id, NULL);
    }
    // ---- Profiler domain ----
    else if (mlen == 15 && strncmp(m, "Profiler.enable", 15) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 16 && strncmp(m, "Profiler.disable", 16) == 0) {
        cdp_respond(client, id, NULL);
    }
    // ---- Debugger domain ----
    else if (mlen == 15 && strncmp(m, "Debugger.enable", 15) == 0) {
        cdp_respond(client, id, "{\"debuggerId\":\"tml-debugger-1\"}");
    } else if (mlen == 16 && strncmp(m, "Debugger.disable", 16) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 31 && strncmp(m, "Debugger.setAsyncCallStackDepth", 31) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 29 && strncmp(m, "Debugger.setPauseOnExceptions", 29) == 0) {
        cdp_respond(client, id, NULL);
    }
    // ---- HeapProfiler domain ----
    else if (mlen == 19 && strncmp(m, "HeapProfiler.enable", 19) == 0) {
        cdp_respond(client, id, NULL);
    }
    // ---- Console domain ----
    else if (mlen == 14 && strncmp(m, "Console.enable", 14) == 0) {
        cdp_respond(client, id, NULL);
    }
    // ---- Default: empty result for unknown methods ----
    else {
        cdp_respond(client, id, NULL);
    }
}

// ============================================================================
// Server accept loop (runs on background thread)
// ============================================================================

static void server_loop(void) {
    while (!g_shutdown_requested) {
        // Accept with timeout so we can check shutdown flag
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(g_server_sock, &fds);
        struct timeval tv = {1, 0}; // 1 second timeout

        int sel = select((int)g_server_sock + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0)
            continue;

        socket_t client = accept(g_server_sock, NULL, NULL);
        if (client == INVALID_SOCK)
            continue;

        // Perform WebSocket handshake
        if (ws_handshake(client) != 0) {
            sock_close(client);
            continue;
        }

        // Client connected
        g_client_sock = client;
        g_client_connected = 1;

        // Signal wait_for_debugger
#ifdef _WIN32
        if (g_connect_event)
            SetEvent(g_connect_event);
#else
        pthread_mutex_lock(&g_connect_mutex);
        pthread_cond_signal(&g_connect_cond);
        pthread_mutex_unlock(&g_connect_mutex);
#endif

        fprintf(stderr, "Debugger attached.\n");

        // Message loop
        char buf[65536];
        while (!g_shutdown_requested) {
            int opcode = 0;
            int n = ws_read_frame(client, buf, sizeof(buf) - 1, &opcode);
            if (n <= 0)
                break;           // Closed or error
            if (opcode == 0x1) { // Text frame
                cdp_handle_message(client, buf, n);
            }
        }

        // Client disconnected
        g_client_connected = 0;
        g_client_sock = INVALID_SOCK;
        sock_close(client);
        fprintf(stderr, "Debugger disconnected.\n");
    }
}

#ifdef _WIN32
static DWORD WINAPI thread_func(LPVOID param) {
    (void)param;
    server_loop();
    return 0;
}
#else
static void* thread_func(void* param) {
    (void)param;
    server_loop();
    return NULL;
}
#endif

// ============================================================================
// Public API
// ============================================================================

TML_EXPORT int64_t tml_inspector_init(int64_t port) {
    if (g_active)
        return 1; // Already running

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 0;
#endif

    g_port = (int)port;
    g_shutdown_requested = 0;

    // Create server socket
    g_server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_server_sock == INVALID_SOCK)
        return 0;

    // Allow port reuse
    int opt = 1;
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only
    addr.sin_port = htons((uint16_t)g_port);

    if (bind(g_server_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        sock_close(g_server_sock);
        g_server_sock = INVALID_SOCK;
        return 0;
    }

    if (listen(g_server_sock, 1) != 0) {
        sock_close(g_server_sock);
        g_server_sock = INVALID_SOCK;
        return 0;
    }

    snprintf(g_url, sizeof(g_url), "ws://127.0.0.1:%d/tml", g_port);
    g_active = 1;

    fprintf(stderr, "Debugger listening on %s\n", g_url);
    fprintf(stderr, "For help, see: https://nodejs.org/en/docs/inspector\n");

    // Start background thread
#ifdef _WIN32
    g_connect_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_thread = CreateThread(NULL, 0, thread_func, NULL, 0, NULL);
#else
    pthread_create(&g_thread, NULL, thread_func, NULL);
#endif

    return 1;
}

TML_EXPORT void tml_inspector_shutdown(void) {
    if (!g_active)
        return;

    g_shutdown_requested = 1;

    // Close client if connected
    if (g_client_sock != INVALID_SOCK) {
        sock_close(g_client_sock);
        g_client_sock = INVALID_SOCK;
    }

    // Close server socket to unblock accept
    if (g_server_sock != INVALID_SOCK) {
        sock_close(g_server_sock);
        g_server_sock = INVALID_SOCK;
    }

    // Wait for thread
#ifdef _WIN32
    if (g_thread) {
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    if (g_connect_event) {
        CloseHandle(g_connect_event);
        g_connect_event = NULL;
    }
    WSACleanup();
#else
    pthread_join(g_thread, NULL);
#endif

    g_active = 0;
    g_client_connected = 0;
    g_url[0] = '\0';
}

TML_EXPORT void tml_inspector_wait_for_debugger(void) {
    if (!g_active)
        return;
    fprintf(stderr, "Waiting for debugger to connect...\n");

#ifdef _WIN32
    if (g_connect_event)
        WaitForSingleObject(g_connect_event, INFINITE);
#else
    pthread_mutex_lock(&g_connect_mutex);
    while (!g_client_connected && !g_shutdown_requested)
        pthread_cond_wait(&g_connect_cond, &g_connect_mutex);
    pthread_mutex_unlock(&g_connect_mutex);
#endif
}

TML_EXPORT const char* tml_inspector_url(void) {
    return g_url;
}

TML_EXPORT int64_t tml_inspector_is_active(void) {
    return g_active ? 1 : 0;
}
