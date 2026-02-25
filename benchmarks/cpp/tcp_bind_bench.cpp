// C++ TCP Socket Bind Benchmark (Winsock2)
// Windows native socket API

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <windows.h>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

int main() {
    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    printf("\n================================================================\n");
    printf("  C++ TCP Benchmarks: Bind (Winsock2)\n");
    printf("================================================================\n\n");

    printf("=== SYNC TCP (WSASocket) ===\n");
    printf("  Binding to 127.0.0.1:0 (50 iterations)\n\n");

    const int n = 50;
    auto start = std::chrono::high_resolution_clock::now();
    int success = 0;

    for (int i = 0; i < n; ++i) {
        // Create socket
        SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
        if (sock == INVALID_SOCKET) {
            continue;
        }

        // Bind to 127.0.0.1:0 (OS assigns free port)
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(0);  // 0 = let OS choose port
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
            success++;
        }

        closesocket(sock);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    int64_t ns_elapsed = elapsed.count();

    printf("    Iterations: %d\n", n);
    printf("    Total time: %lld ms\n", ns_elapsed / 1000000);
    printf("    Per op:     %lld ns\n", ns_elapsed > 0 ? ns_elapsed / n : 0);
    printf("    Ops/sec:    %lld\n", ns_elapsed > 0 ? (int64_t)n * 1000000000 / ns_elapsed : 0);
    printf("    Successful: %d/%d\n\n", success, n);

    printf("================================================================\n\n");

    // Cleanup
    WSACleanup();
    return 0;
}
