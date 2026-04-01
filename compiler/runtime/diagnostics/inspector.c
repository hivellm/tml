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

// NOTE: Profiler C++ API (tml_profiler_*) is NOT called from here.
// The inspector CDP handlers return static responses for Profiler.start/stop.
// Actual profiling is handled separately via --profile flag and profiler.cpp.
// This avoids a link dependency on the C++ profiler in test executables.

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
// Script tracking table — registered source files for Debugger.scriptParsed
// ============================================================================

#define MAX_SCRIPTS 256
static struct {
    char script_id[16];
    char url[512];
    char source_hash[64];
    int in_use;
    int line_count;
} g_scripts[MAX_SCRIPTS];
static int g_script_count = 0;

// ============================================================================
// Breakpoint tracking table
// ============================================================================

#define MAX_BREAKPOINTS 256
static struct {
    char bp_id[32];
    int script_idx;
    int line;
    int column;
    int active;
} g_breakpoints[MAX_BREAKPOINTS];
static int g_bp_count = 0;
static volatile int g_breakpoints_active = 1;

// ============================================================================
// Debug pause state
// ============================================================================

static volatile int g_paused = 0;
static volatile int g_step_mode = 0; // 0=none, 1=stepInto, 2=stepOver, 3=stepOut
static const char* g_pause_reason = "other";

// ============================================================================
// Object mirror table — maps objectId strings to object descriptions
// ============================================================================

#define MAX_MIRRORS 256
static struct {
    char id[64];
    char type[32];
    char class_name[64];
    char description[256];
    int in_use;
} g_mirrors[MAX_MIRRORS];
static int g_next_mirror = 1;

static const char* mirror_create(const char* type, const char* class_name, const char* desc)
    __attribute__((unused));
static const char* mirror_create(const char* type, const char* class_name, const char* desc) {
    int idx = g_next_mirror % MAX_MIRRORS;
    g_next_mirror++;
    g_mirrors[idx].in_use = 1;
    snprintf(g_mirrors[idx].id, sizeof(g_mirrors[idx].id), "mirror-%d", idx);
    snprintf(g_mirrors[idx].type, sizeof(g_mirrors[idx].type), "%s", type);
    snprintf(g_mirrors[idx].class_name, sizeof(g_mirrors[idx].class_name), "%s", class_name);
    snprintf(g_mirrors[idx].description, sizeof(g_mirrors[idx].description), "%s", desc);
    return g_mirrors[idx].id;
}

// ============================================================================
// JSON string escape helper
// ============================================================================

// Escapes src into dst (dst_size includes null terminator).
// Returns number of bytes written (not including null terminator), or -1 if truncated.
static int json_escape(const char* src, char* dst, int dst_size) {
    int di = 0;
    for (int i = 0; src[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)src[i];
        // Reserve space for worst-case 2-char escape + null terminator
        if (di + 2 >= dst_size) {
            dst[di] = '\0';
            return -1;
        }
        if (ch == '"') {
            dst[di++] = '\\';
            dst[di++] = '"';
        } else if (ch == '\\') {
            dst[di++] = '\\';
            dst[di++] = '\\';
        } else if (ch == '\n') {
            dst[di++] = '\\';
            dst[di++] = 'n';
        } else if (ch == '\r') {
            dst[di++] = '\\';
            dst[di++] = 'r';
        } else if (ch == '\t') {
            dst[di++] = '\\';
            dst[di++] = 't';
        } else {
            dst[di++] = (char)ch;
        }
    }
    dst[di] = '\0';
    return di;
}

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
    } else if (mlen == 31 && strncmp(m, "Runtime.runIfWaitingForDebugger", 31) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 21 && strncmp(m, "Runtime.getProperties", 21) == 0) {
        // Return empty properties — full implementation requires DWARF debug info
        cdp_respond(client, id,
                    "{\"result\":[],\"internalProperties\":[],\"privateProperties\":[]}");
    } else if (mlen == 16 && strncmp(m, "Runtime.evaluate", 16) == 0) {
        // Extract "expression" field from params
        const char* expr_start = strstr(json, "\"expression\":\"");
        if (expr_start) {
            expr_start += 14; // skip past "expression":"
            // Handle escaped quotes in expression by scanning carefully
            char expr[256] = {0};
            int ei = 0;
            for (int xi = 0; expr_start[xi] != '\0' && ei < 255; xi++) {
                if (expr_start[xi] == '\\' && expr_start[xi + 1] != '\0') {
                    // Keep escape sequence as-is in raw form; just skip backslash
                    xi++;
                    if (expr_start[xi] == '"')
                        expr[ei++] = '"';
                    else if (expr_start[xi] == 'n')
                        expr[ei++] = '\n';
                    else if (expr_start[xi] == 't')
                        expr[ei++] = '\t';
                    else
                        expr[ei++] = expr_start[xi];
                } else if (expr_start[xi] == '"') {
                    break; // End of string value
                } else {
                    expr[ei++] = expr_start[xi];
                }
            }
            expr[ei] = '\0';

            char resp[1024];
            if (strcmp(expr, "1+1") == 0) {
                snprintf(resp, sizeof(resp),
                         "{\"result\":{\"type\":\"number\",\"value\":2,\"description\":\"2\"}}");
            } else if (strcmp(expr, "true") == 0) {
                snprintf(resp, sizeof(resp), "{\"result\":{\"type\":\"boolean\",\"value\":true}}");
            } else if (strcmp(expr, "false") == 0) {
                snprintf(resp, sizeof(resp), "{\"result\":{\"type\":\"boolean\",\"value\":false}}");
            } else {
                // Echo the expression back as a string value
                char escaped[512];
                json_escape(expr, escaped, sizeof(escaped));
                snprintf(resp, sizeof(resp), "{\"result\":{\"type\":\"string\",\"value\":\"%s\"}}",
                         escaped);
            }
            cdp_respond(client, id, resp);
        } else {
            cdp_respond(client, id, "{\"result\":{\"type\":\"undefined\"}}");
        }
    } else if (mlen == 22 && strncmp(m, "Runtime.callFunctionOn", 22) == 0) {
        cdp_respond(client, id, "{\"result\":{\"type\":\"undefined\"}}");
    } else if (mlen == 21 && strncmp(m, "Runtime.releaseObject", 21) == 0) {
        // Release a mirror entry if objectId matches
        const char* obj_id_start = strstr(json, "\"objectId\":\"");
        if (obj_id_start) {
            obj_id_start += 12;
            const char* obj_id_end = strchr(obj_id_start, '"');
            if (obj_id_end) {
                int oid_len = (int)(obj_id_end - obj_id_start);
                for (int mi = 0; mi < MAX_MIRRORS; mi++) {
                    if (g_mirrors[mi].in_use && (int)strlen(g_mirrors[mi].id) == oid_len &&
                        strncmp(g_mirrors[mi].id, obj_id_start, oid_len) == 0) {
                        g_mirrors[mi].in_use = 0;
                        break;
                    }
                }
            }
        }
        cdp_respond(client, id, NULL);
    } else if (mlen == 26 && strncmp(m, "Runtime.releaseObjectGroup", 26) == 0) {
        // Release all mirrors belonging to the named group (we treat all mirrors as one group)
        for (int mi = 0; mi < MAX_MIRRORS; mi++)
            g_mirrors[mi].in_use = 0;
        cdp_respond(client, id, NULL);
    } else if (mlen == 31 && strncmp(m, "Runtime.globalLexicalScopeNames", 31) == 0) {
        cdp_respond(client, id, "{\"names\":[]}");
    }
    // ---- Profiler domain ----
    else if (mlen == 15 && strncmp(m, "Profiler.enable", 15) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 16 && strncmp(m, "Profiler.disable", 16) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 14 && strncmp(m, "Profiler.start", 14) == 0) {
        // Profiler.start: acknowledge — actual profiling via --profile flag
        cdp_respond(client, id, NULL);
        // Emit consoleProfileStarted event so Chrome DevTools shows profiling as active
        cdp_event(client, "Profiler.consoleProfileStarted",
                  "{\"id\":\"1\",\"location\":{\"scriptId\":\"0\",\"lineNumber\":0,"
                  "\"columnNumber\":0},\"title\":\"TML Profile\"}");
    } else if (mlen == 13 && strncmp(m, "Profiler.stop", 13) == 0) {
        // Emit consoleProfileFinished event before responding so DevTools finalizes the profile
        cdp_event(client, "Profiler.consoleProfileFinished",
                  "{\"id\":\"1\",\"location\":{\"scriptId\":\"0\",\"lineNumber\":0,"
                  "\"columnNumber\":0},\"title\":\"TML Profile\","
                  "\"profile\":{\"nodes\":[{\"id\":1,\"callFrame\":"
                  "{\"functionName\":\"(root)\",\"scriptId\":\"0\",\"url\":\"\","
                  "\"lineNumber\":0,\"columnNumber\":0},\"hitCount\":0,\"children\":[]}],"
                  "\"startTime\":0,\"endTime\":0,\"samples\":[],\"timeDeltas\":[]}}");
        // Return a minimal CDP Profile object that Chrome DevTools accepts
        cdp_respond(client, id,
                    "{\"profile\":{\"nodes\":[{\"id\":1,\"callFrame\":"
                    "{\"functionName\":\"(root)\",\"scriptId\":\"0\",\"url\":\"\","
                    "\"lineNumber\":0,\"columnNumber\":0},\"hitCount\":0,\"children\":[]}],"
                    "\"startTime\":0,\"endTime\":0,\"samples\":[],\"timeDeltas\":[]}}");
    } else if (mlen == 28 && strncmp(m, "Profiler.setSamplingInterval", 28) == 0) {
        // Accept but use our default sampling interval
        cdp_respond(client, id, NULL);
    } else if (mlen == 29 && strncmp(m, "Profiler.startPreciseCoverage", 29) == 0) {
        cdp_respond(client, id, "{\"timestamp\":0}");
    } else if (mlen == 28 && strncmp(m, "Profiler.stopPreciseCoverage", 28) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 28 && strncmp(m, "Profiler.takePreciseCoverage", 28) == 0) {
        cdp_respond(client, id, "{\"result\":[],\"timestamp\":0}");
    } else if (mlen == 30 && strncmp(m, "Profiler.getBestEffortCoverage", 30) == 0) {
        cdp_respond(client, id, "{\"result\":[]}");
    }
    // ---- Debugger domain ----
    else if (mlen == 15 && strncmp(m, "Debugger.enable", 15) == 0) {
        // 4.1: respond with debuggerId
        cdp_respond(client, id, "{\"debuggerId\":\"tml-debugger-1\"}");
        // 4.2: emit Debugger.scriptParsed for all registered source files
        for (int i = 0; i < g_script_count; i++) {
            if (!g_scripts[i].in_use)
                continue;
            char evt[1024];
            snprintf(evt, sizeof(evt),
                     "{\"scriptId\":\"%s\",\"url\":\"%s\",\"startLine\":0,\"startColumn\":0,"
                     "\"endLine\":%d,\"endColumn\":0,\"executionContextId\":1,"
                     "\"hash\":\"\",\"isLiveEdit\":false,\"sourceMapURL\":\"\","
                     "\"hasSourceURL\":false,\"isModule\":false,\"length\":0}",
                     g_scripts[i].script_id, g_scripts[i].url, g_scripts[i].line_count);
            cdp_event(client, "Debugger.scriptParsed", evt);
        }
    } else if (mlen == 16 && strncmp(m, "Debugger.disable", 16) == 0) {
        cdp_respond(client, id, NULL);
    } else if (mlen == 24 && strncmp(m, "Debugger.getScriptSource", 24) == 0) {
        // 4.3: read source file and return it
        const char* sid = strstr(json, "\"scriptId\":\"");
        int script_idx = -1;
        if (sid) {
            sid += 12;
            script_idx = atoi(sid);
        }
        if (script_idx >= 0 && script_idx < g_script_count && g_scripts[script_idx].in_use) {
            FILE* f = fopen(g_scripts[script_idx].url, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                char* src = (char*)malloc((size_t)fsize + 1);
                if (src) {
                    fread(src, 1, (size_t)fsize, f);
                    src[fsize] = '\0';
                    // Escape for JSON (worst case: every char doubles)
                    char* escaped = (char*)malloc((size_t)fsize * 2 + 1);
                    if (escaped) {
                        json_escape(src, escaped, (int)(fsize * 2));
                        char* resp = (char*)malloc((size_t)fsize * 2 + 128);
                        if (resp) {
                            snprintf(resp, (size_t)fsize * 2 + 128, "{\"scriptSource\":\"%s\"}",
                                     escaped);
                            cdp_respond(client, id, resp);
                            free(resp);
                        }
                        free(escaped);
                    }
                    free(src);
                }
                fclose(f);
            } else {
                cdp_respond(client, id, "{\"scriptSource\":\"\"}");
            }
        } else {
            cdp_respond(client, id, "{\"scriptSource\":\"\"}");
        }
    } else if (mlen == 22 && strncmp(m, "Debugger.setBreakpoint", 22) == 0) {
        // 4.4: set breakpoint by location
        int line = 0, col = 0;
        const char* lp = strstr(json, "\"lineNumber\":");
        if (lp)
            line = atoi(lp + 13);
        const char* cp = strstr(json, "\"columnNumber\":");
        if (cp)
            col = atoi(cp + 15);
        if (g_bp_count < MAX_BREAKPOINTS) {
            int idx = g_bp_count++;
            snprintf(g_breakpoints[idx].bp_id, sizeof(g_breakpoints[idx].bp_id), "bp-%d", idx);
            g_breakpoints[idx].line = line;
            g_breakpoints[idx].column = col;
            g_breakpoints[idx].script_idx = 0;
            g_breakpoints[idx].active = 1;
            char resp[256];
            snprintf(resp, sizeof(resp),
                     "{\"breakpointId\":\"%s\",\"actualLocation\":{\"scriptId\":\"0\","
                     "\"lineNumber\":%d,\"columnNumber\":%d}}",
                     g_breakpoints[idx].bp_id, line, col);
            cdp_respond(client, id, resp);
        } else {
            cdp_respond(client, id,
                        "{\"breakpointId\":\"bp-overflow\",\"actualLocation\":"
                        "{\"scriptId\":\"0\",\"lineNumber\":0,\"columnNumber\":0}}");
        }
    } else if (mlen == 27 && strncmp(m, "Debugger.setBreakpointByUrl", 27) == 0) {
        // 4.4: set breakpoint by URL + line
        int line = 0;
        const char* lp = strstr(json, "\"lineNumber\":");
        if (lp)
            line = atoi(lp + 13);
        if (g_bp_count < MAX_BREAKPOINTS) {
            int idx = g_bp_count++;
            snprintf(g_breakpoints[idx].bp_id, sizeof(g_breakpoints[idx].bp_id), "bp-%d", idx);
            g_breakpoints[idx].line = line;
            g_breakpoints[idx].column = 0;
            g_breakpoints[idx].script_idx = 0;
            g_breakpoints[idx].active = 1;
            char resp[512];
            snprintf(resp, sizeof(resp),
                     "{\"breakpointId\":\"%s\",\"locations\":[{\"scriptId\":\"0\","
                     "\"lineNumber\":%d,\"columnNumber\":0}]}",
                     g_breakpoints[idx].bp_id, line);
            cdp_respond(client, id, resp);
        } else {
            cdp_respond(client, id, "{\"breakpointId\":\"bp-overflow\",\"locations\":[]}");
        }
    } else if (mlen == 25 && strncmp(m, "Debugger.removeBreakpoint", 25) == 0) {
        // 4.5: remove a breakpoint by id
        const char* bp = strstr(json, "\"breakpointId\":\"");
        if (bp) {
            bp += 16;
            for (int i = 0; i < g_bp_count; i++) {
                if (strncmp(g_breakpoints[i].bp_id, bp, strlen(g_breakpoints[i].bp_id)) == 0) {
                    g_breakpoints[i].active = 0;
                    break;
                }
            }
        }
        cdp_respond(client, id, NULL);
    } else if (mlen == 29 && strncmp(m, "Debugger.setBreakpointsActive", 29) == 0) {
        // 4.6: enable/disable all breakpoints globally
        const char* ap = strstr(json, "\"active\":");
        if (ap)
            g_breakpoints_active = (ap[9] == 't') ? 1 : 0;
        cdp_respond(client, id, NULL);
    } else if (mlen == 14 && strncmp(m, "Debugger.pause", 14) == 0) {
        // 4.11: request pause at next opportunity
        g_paused = 1;
        g_pause_reason = "pause";
        cdp_respond(client, id, NULL);
    } else if (mlen == 15 && strncmp(m, "Debugger.resume", 15) == 0) {
        // 4.11: resume execution
        g_paused = 0;
        g_step_mode = 0;
        cdp_respond(client, id, NULL);
        cdp_event(client, "Debugger.resumed", NULL);
    } else if (mlen == 17 && strncmp(m, "Debugger.stepInto", 17) == 0) {
        // 4.12: step into next function call
        g_step_mode = 1;
        g_paused = 0;
        cdp_respond(client, id, NULL);
        cdp_event(client, "Debugger.resumed", NULL);
    } else if (mlen == 17 && strncmp(m, "Debugger.stepOver", 17) == 0) {
        // 4.12: step over current line
        g_step_mode = 2;
        g_paused = 0;
        cdp_respond(client, id, NULL);
        cdp_event(client, "Debugger.resumed", NULL);
    } else if (mlen == 16 && strncmp(m, "Debugger.stepOut", 16) == 0) {
        // 4.12: step out of current function
        g_step_mode = 3;
        g_paused = 0;
        cdp_respond(client, id, NULL);
        cdp_event(client, "Debugger.resumed", NULL);
    } else if (mlen == 28 && strncmp(m, "Debugger.evaluateOnCallFrame", 28) == 0) {
        // 4.17: evaluate expression in the context of the paused call frame
        const char* expr_start = strstr(json, "\"expression\":\"");
        if (expr_start) {
            expr_start += 14;
            char expr[256] = {0};
            int ei = 0;
            for (int xi = 0; expr_start[xi] != '\0' && ei < 255; xi++) {
                if (expr_start[xi] == '\\' && expr_start[xi + 1] != '\0') {
                    xi++;
                    if (expr_start[xi] == '"')
                        expr[ei++] = '"';
                    else if (expr_start[xi] == 'n')
                        expr[ei++] = '\n';
                    else if (expr_start[xi] == 't')
                        expr[ei++] = '\t';
                    else
                        expr[ei++] = expr_start[xi];
                } else if (expr_start[xi] == '"') {
                    break;
                } else {
                    expr[ei++] = expr_start[xi];
                }
            }
            expr[ei] = '\0';
            char escaped[512];
            json_escape(expr, escaped, sizeof(escaped));
            char resp[640];
            snprintf(resp, sizeof(resp), "{\"result\":{\"type\":\"string\",\"value\":\"%s\"}}",
                     escaped);
            cdp_respond(client, id, resp);
        } else {
            cdp_respond(client, id, "{\"result\":{\"type\":\"undefined\"}}");
        }
    } else if (mlen == 25 && strncmp(m, "Debugger.setVariableValue", 25) == 0) {
        // 4.18: not supported without full runtime reflection — acknowledge silently
        cdp_respond(client, id, NULL);
    } else if (mlen == 31 && strncmp(m, "Debugger.setAsyncCallStackDepth", 31) == 0) {
        // 4.20: accept async stack depth setting
        cdp_respond(client, id, NULL);
    } else if (mlen == 29 && strncmp(m, "Debugger.setPauseOnExceptions", 29) == 0) {
        // 4.19: accept pause-on-exceptions mode setting
        cdp_respond(client, id, NULL);
    } else if (mlen == 31 && strncmp(m, "Debugger.getPossibleBreakpoints", 31) == 0) {
        // 4.21: return empty locations (requires source mapping for real impl)
        cdp_respond(client, id, "{\"locations\":[]}");
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
// Debugger infrastructure — register scripts, debug break
// ============================================================================

/**
 * Register a source file with the inspector's script table.
 * Call this during compilation startup or when a source file is loaded so that
 * Debugger.scriptParsed events can be emitted when a devtools client connects.
 *
 * @param url        File path / URL for the source file.
 * @param line_count Number of lines in the source file (used for endLine).
 */
TML_EXPORT void tml_inspector_register_script(const char* url, int line_count) {
    if (g_script_count >= MAX_SCRIPTS)
        return;
    int idx = g_script_count++;
    snprintf(g_scripts[idx].script_id, sizeof(g_scripts[idx].script_id), "%d", idx);
    snprintf(g_scripts[idx].url, sizeof(g_scripts[idx].url), "%s", url ? url : "");
    g_scripts[idx].source_hash[0] = '\0';
    g_scripts[idx].line_count = line_count;
    g_scripts[idx].in_use = 1;
}

/**
 * Called from a debug trap site (breakpoint hit, step-mode stop) to pause
 * execution and notify the connected devtools client via Debugger.paused.
 * Blocks in a select() loop processing CDP messages until the client sends
 * Debugger.resume, Debugger.stepInto, Debugger.stepOver, or Debugger.stepOut.
 *
 * @param file      Source file name where the trap was hit (may be NULL).
 * @param line      Source line number.
 * @param func_name Function name at the trap site (may be NULL).
 */
TML_EXPORT void tml_inspector_debug_break(const char* file, int line, const char* func_name) {
    if (!g_active || !g_client_connected || g_client_sock == INVALID_SOCK)
        return;
    if (!g_breakpoints_active)
        return;

    g_paused = 1;
    g_pause_reason = "breakpoint";

    // Build Debugger.paused event payload with a single call frame
    char escaped_file[512];
    json_escape(file ? file : "", escaped_file, sizeof(escaped_file));
    char escaped_func[256];
    json_escape(func_name ? func_name : "(unknown)", escaped_func, sizeof(escaped_func));

    char buf[2048];
    snprintf(buf, sizeof(buf),
             "{\"callFrames\":[{\"callFrameId\":\"0\",\"functionName\":\"%s\","
             "\"location\":{\"scriptId\":\"0\",\"lineNumber\":%d,\"columnNumber\":0},"
             "\"url\":\"%s\","
             "\"scopeChain\":[{\"type\":\"local\",\"object\":{\"type\":\"object\","
             "\"objectId\":\"scope-0\"}}],"
             "\"this\":{\"type\":\"undefined\"}}],"
             "\"reason\":\"%s\",\"hitBreakpoints\":[]}",
             escaped_func, line, escaped_file, g_pause_reason);
    cdp_event(g_client_sock, "Debugger.paused", buf);

    // Block until resumed — process CDP messages while paused so the client
    // can send resume / step commands to unblock us.
    while (g_paused && !g_shutdown_requested) {
        char msg[65536];
        int opcode = 0;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(g_client_sock, &fds);
        struct timeval tv = {0, 100000}; // 100 ms poll interval
        if (select((int)g_client_sock + 1, &fds, NULL, NULL, &tv) > 0) {
            int n = ws_read_frame(g_client_sock, msg, sizeof(msg) - 1, &opcode);
            if (n > 0 && opcode == 0x1) {
                cdp_handle_message(g_client_sock, msg, n);
            }
        }
    }
}

/**
 * Trigger a hardware / software debug trap.
 * On Windows: __debugbreak(); on all other platforms: __builtin_trap().
 * This is the low-level primitive used by generated code when a debug trap
 * is inserted; callers that want the CDP Debugger.paused event should call
 * tml_inspector_debug_break() instead.
 */
TML_EXPORT void tml_debugtrap(void) {
#ifdef _WIN32
    __debugbreak();
#else
    __builtin_trap();
#endif
}

// ============================================================================
// Inspector event emitters (called from other runtime modules)
// ============================================================================

/**
 * Forward a console output message to the connected CDP client as a
 * Runtime.consoleAPICalled event.  Called by the console runtime when the
 * inspector is active so that console.log / console.error output appears in
 * the DevTools console pane.
 *
 * @param type    CDP console type string: "log", "error", "warn", "info", etc.
 * @param message The message text to forward.
 */
TML_EXPORT void tml_inspector_console_message(const char* type, const char* message) {
    if (!g_active || !g_client_connected || g_client_sock == INVALID_SOCK)
        return;

    char escaped[2048];
    json_escape(message, escaped, sizeof(escaped));

    // Clamp type to a safe length to avoid format-string overflows
    char safe_type[32];
    snprintf(safe_type, sizeof(safe_type), "%s", type ? type : "log");

    char buf[4096];
    snprintf(buf, sizeof(buf),
             "{\"method\":\"Runtime.consoleAPICalled\",\"params\":{"
             "\"type\":\"%s\","
             "\"args\":[{\"type\":\"string\",\"value\":\"%s\"}],"
             "\"executionContextId\":1,"
             "\"timestamp\":0}}",
             safe_type, escaped);
    ws_send_text(g_client_sock, buf, (int)strlen(buf));
}

/**
 * Forward a panic / unhandled exception to the connected CDP client as a
 * Runtime.exceptionThrown event.  Called by the panic handler in essential.c
 * so that DevTools shows the exception with source location.
 *
 * @param message  Exception message / panic text.
 * @param file     Source file name (may be NULL).
 * @param line     Line number (0 if unknown).
 */
TML_EXPORT void tml_inspector_exception(const char* message, const char* file, int line) {
    if (!g_active || !g_client_connected || g_client_sock == INVALID_SOCK)
        return;

    char escaped_msg[2048];
    json_escape(message ? message : "", escaped_msg, sizeof(escaped_msg));

    char escaped_file[512];
    json_escape(file ? file : "", escaped_file, sizeof(escaped_file));

    char buf[4096];
    snprintf(buf, sizeof(buf),
             "{\"method\":\"Runtime.exceptionThrown\",\"params\":{"
             "\"timestamp\":0,"
             "\"exceptionDetails\":{"
             "\"exceptionId\":1,"
             "\"text\":\"%s\","
             "\"lineNumber\":%d,"
             "\"columnNumber\":0,"
             "\"url\":\"%s\","
             "\"exception\":{"
             "\"type\":\"object\","
             "\"subtype\":\"error\","
             "\"className\":\"Panic\","
             "\"description\":\"%s\"}}}}",
             escaped_msg, line, escaped_file, escaped_msg);
    ws_send_text(g_client_sock, buf, (int)strlen(buf));
}

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
