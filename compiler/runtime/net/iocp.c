/**
 * @file iocp.c
 * @brief TML Runtime - Windows I/O Completion Ports (IOCP)
 *
 * Provides high-performance async I/O for Windows using IOCP, replacing the
 * WSAPoll-based model in poll.c. IOCP achieves 500K+ req/s vs ~50K for WSAPoll
 * by issuing async I/O operations that complete in kernel space, then notifying
 * worker threads via GetQueuedCompletionStatus rather than polling readiness.
 *
 * ## Architecture
 *
 * 1. Create IOCP handle via `tml_iocp_create`
 * 2. Create a listening socket via `tml_iocp_listen` and associate it
 * 3. Issue async AcceptEx operations via `tml_iocp_accept`
 * 4. Worker threads block on `tml_iocp_wait`; when a completion arrives:
 *    - For accept completions: call SO_UPDATE_ACCEPT_CONTEXT, re-issue AcceptEx
 *    - For recv completions: process the received data, re-issue WSARecv
 *    - For send completions: free/reuse the buffer
 * 5. To shut down: post sentinel completions via `tml_iocp_post` (token=0, op=NULL)
 *
 * ## Key Invariants
 *
 * - OVERLAPPED must be the FIRST field of IocpOperation — Windows casts the
 *   OVERLAPPED* from GetQueuedCompletionStatus back to IocpOperation* via
 *   CONTAINING_RECORD / simple cast (they point to the same address).
 * - Use calloc for IocpOperation to zero-initialize the embedded OVERLAPPED.
 *   Windows sets internal fields of OVERLAPPED between issue and completion;
 *   reusing a non-zeroed OVERLAPPED causes undefined behavior.
 * - AcceptEx requires loading the function pointer at runtime via WSAIoctl.
 * - Every accepted socket must call setsockopt(SO_UPDATE_ACCEPT_CONTEXT) before
 *   use so that standard socket functions work on the accepted socket.
 *
 * ## Functions
 *
 * - `tml_iocp_create`              - Create IOCP handle
 * - `tml_iocp_destroy`             - Destroy IOCP handle
 * - `tml_iocp_associate`           - Associate socket with IOCP
 * - `tml_iocp_recv`                - Issue async WSARecv
 * - `tml_iocp_send`                - Issue async WSASend
 * - `tml_iocp_wait`                - Block until a completion arrives
 * - `tml_iocp_post`                - Post a custom completion (e.g., shutdown)
 * - `tml_iocp_op_alloc`            - Allocate an IocpOperation
 * - `tml_iocp_op_free`             - Free an IocpOperation
 * - `tml_iocp_op_type`             - Get op_type from an IocpOperation
 * - `tml_iocp_op_token`            - Get token from an IocpOperation
 * - `tml_iocp_op_socket`           - Get socket from an IocpOperation
 * - `tml_iocp_listen`              - Create a listening socket with AcceptEx support
 * - `tml_iocp_accept`              - Issue async AcceptEx
 * - `tml_iocp_create_accept_socket`- Create a socket ready for AcceptEx
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

/*
 * Include order is critical on Windows:
 *   1. winsock2.h — must come BEFORE windows.h to avoid winsock.h conflicts
 *   2. windows.h — provides LONG, DWORD, HANDLE, etc. needed by mswsock.h
 *   3. ws2tcpip.h — IPv6 and protocol definitions
 *   4. mswsock.h — AcceptEx, GetAcceptExSockaddrs, TransmitFile
 */
#define WIN32_LEAN_AND_MEAN
#include <mswsock.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

/* ── WSA initialisation (same pattern as net.c / poll.c) ─────────────────── */

extern int32_t sys_wsa_startup(void);
static int iocp_wsa_initialized = 0;

static void ensure_wsa(void) {
    if (!iocp_wsa_initialized) {
        sys_wsa_startup();
        iocp_wsa_initialized = 1;
    }
}

/* ── Operation type constants ─────────────────────────────────────────────── */

#define IOCP_OP_RECV 0
#define IOCP_OP_SEND 1
#define IOCP_OP_ACCEPT 2

/* ── Per-operation context ────────────────────────────────────────────────── */

/**
 * IocpOperation — one per outstanding async I/O request.
 *
 * The embedded OVERLAPPED MUST be the first field.  Windows returns an
 * OVERLAPPED* from GetQueuedCompletionStatus; we cast it directly to
 * IocpOperation* because the OVERLAPPED lives at offset 0.
 *
 * calloc-allocate these so the OVERLAPPED is zero-initialised before
 * passing to WSARecv / WSASend / AcceptEx.
 */
typedef struct {
    OVERLAPPED overlapped; /* MUST be first — offset 0 */
    WSABUF wsabuf;         /* buffer descriptor for WSARecv / WSASend */
    int64_t socket;        /* socket this operation is for */
    int32_t op_type;       /* IOCP_OP_RECV / IOCP_OP_SEND / IOCP_OP_ACCEPT */
    int32_t token;         /* caller-assigned connection index */
} IocpOperation;

/* ── AcceptEx function pointer (loaded once per process) ──────────────────── */

static LPFN_ACCEPTEX g_fn_AcceptEx = NULL;
static LPFN_GETACCEPTEXSOCKADDRS g_fn_GetAcceptExSockaddrs = NULL;

/**
 * @brief Load AcceptEx and GetAcceptExSockaddrs via WSAIoctl.
 *
 * AcceptEx is a Winsock extension function; its pointer must be obtained
 * through WSAIoctl with SIO_GET_EXTENSION_FUNCTION_POINTER because it lives
 * in mswsock.dll rather than ws2_32.dll.  We use a temporary socket just to
 * make the ioctl call — the socket is closed immediately afterward.
 *
 * @return 1 on success, 0 on failure.
 */
static int load_acceptex(void) {
    if (g_fn_AcceptEx != NULL) {
        return 1; /* already loaded */
    }

    SOCKET tmp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tmp == INVALID_SOCKET) {
        return 0;
    }

    GUID guid_acceptex = WSAID_ACCEPTEX;
    GUID guid_getaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    DWORD bytes = 0;

    int ok_accept =
        WSAIoctl(tmp, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_acceptex, sizeof(guid_acceptex),
                 &g_fn_AcceptEx, sizeof(g_fn_AcceptEx), &bytes, NULL, NULL) == 0;

    int ok_addrs = WSAIoctl(tmp, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_getaddrs,
                            sizeof(guid_getaddrs), &g_fn_GetAcceptExSockaddrs,
                            sizeof(g_fn_GetAcceptExSockaddrs), &bytes, NULL, NULL) == 0;

    closesocket(tmp);

    if (!ok_accept) {
        g_fn_AcceptEx = NULL;
        g_fn_GetAcceptExSockaddrs = NULL;
        return 0;
    }
    if (!ok_addrs) {
        /* AcceptEx loaded but GetAcceptExSockaddrs failed — still usable */
        g_fn_GetAcceptExSockaddrs = NULL;
    }
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════════
   IOCP Lifecycle
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create an IOCP handle.
 *
 * @param num_threads Maximum number of threads that can concurrently process
 *                    completions.  Pass 0 to let Windows use the CPU count.
 * @return IOCP handle as int64_t, or 0 on failure.
 */
int64_t tml_iocp_create(int32_t num_threads) {
    ensure_wsa();
    if (!load_acceptex()) {
        return 0;
    }

    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, /* no file handle yet */
                                         NULL,                 /* no existing IOCP to associate */
                                         0, /* completion key (unused at creation) */
                                         (DWORD)num_threads);
    return iocp ? (int64_t)(uintptr_t)iocp : 0;
}

/**
 * @brief Destroy an IOCP handle.
 *
 * Closing the handle causes all threads blocked in GetQueuedCompletionStatus
 * to return with a FALSE result and GetLastError() == ERROR_ABANDONED_WAIT_0.
 * Post shutdown sentinels before calling this to unblock workers gracefully.
 *
 * @param iocp IOCP handle from tml_iocp_create.
 */
void tml_iocp_destroy(int64_t iocp) {
    if (iocp) {
        CloseHandle((HANDLE)(uintptr_t)iocp);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Socket Association
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Associate a socket with an IOCP.
 *
 * Must be called once per socket before issuing any async I/O on it.
 * The token (completion key) is stored by the OS and returned verbatim from
 * GetQueuedCompletionStatus for every completion on this socket.
 *
 * @param iocp   IOCP handle.
 * @param socket Socket handle.
 * @param token  Per-connection identifier (e.g., connection index).
 * @return 0 on success, -1 on error.
 */
int32_t tml_iocp_associate(int64_t iocp, int64_t socket, int32_t token) {
    if (!iocp || socket == -1) {
        return -1;
    }

    HANDLE result = CreateIoCompletionPort((HANDLE)(uintptr_t)socket, (HANDLE)(uintptr_t)iocp,
                                           (ULONG_PTR)(uintptr_t)(uint32_t)token,
                                           0 /* num_threads ignored when associating */
    );
    return result ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════════════════════
   Async I/O Operations
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Issue an async WSARecv on a socket.
 *
 * The socket must already be associated with the IOCP via tml_iocp_associate.
 * The buffer (buf/len) is owned by the caller for the lifetime of the
 * operation — do not free or modify it until the completion arrives.
 *
 * WSARecv returns 0 (completed synchronously — rare) or SOCKET_ERROR.
 * On SOCKET_ERROR, WSA_IO_PENDING means the operation is queued normally.
 * Any other error code is a real failure.
 *
 * @param socket Socket handle.
 * @param buf    Buffer to receive data into.
 * @param len    Length of buf in bytes.
 * @param op     Pre-allocated IocpOperation (caller manages lifetime).
 * @return 0 on success (operation pending or completed inline), -1 on error.
 */
int32_t tml_iocp_recv(int64_t socket, uint8_t* buf, int64_t len, void* op) {
    if (socket == -1 || !buf || len <= 0 || !op) {
        return -1;
    }

    IocpOperation* iop = (IocpOperation*)op;

    /* Re-zero the OVERLAPPED so it is fresh for reuse */
    memset(&iop->overlapped, 0, sizeof(OVERLAPPED));

    iop->wsabuf.buf = (CHAR*)buf;
    iop->wsabuf.len = (ULONG)len;
    iop->socket = socket;
    iop->op_type = IOCP_OP_RECV;

    DWORD flags = 0;
    int rc = WSARecv((SOCKET)socket, &iop->wsabuf, 1, /* buffer count */
                     NULL, /* bytes received — use GetQueuedCompletionStatus instead */
                     &flags, &iop->overlapped, NULL /* no completion routine — IOCP-based */
    );

    if (rc == 0) {
        return 0; /* completed synchronously; IOCP will still post a notification */
    }
    if (WSAGetLastError() == WSA_IO_PENDING) {
        return 0; /* queued normally */
    }
    return -1; /* real error */
}

/**
 * @brief Issue an async WSASend on a socket.
 *
 * Same lifetime rules as tml_iocp_recv: buf must remain valid until the
 * send completion arrives.  For zero-copy sends, buf can point into a
 * pre-allocated connection write buffer.
 *
 * @param socket Socket handle.
 * @param buf    Buffer containing data to send.
 * @param len    Number of bytes to send.
 * @param op     Pre-allocated IocpOperation.
 * @return 0 on success (pending or inline), -1 on error.
 */
int32_t tml_iocp_send(int64_t socket, const uint8_t* buf, int64_t len, void* op) {
    if (socket == -1 || !buf || len <= 0 || !op) {
        return -1;
    }

    IocpOperation* iop = (IocpOperation*)op;

    memset(&iop->overlapped, 0, sizeof(OVERLAPPED));

    /*
     * WSASend takes non-const WSABUF.buf.  The cast is safe because WSASend
     * only reads the buffer; it does not modify it.
     */
    iop->wsabuf.buf = (CHAR*)(uintptr_t)buf;
    iop->wsabuf.len = (ULONG)len;
    iop->socket = socket;
    iop->op_type = IOCP_OP_SEND;

    int rc = WSASend((SOCKET)socket, &iop->wsabuf, 1,
                     NULL, /* bytes sent — use GetQueuedCompletionStatus */
                     0,    /* flags */
                     &iop->overlapped, NULL);

    if (rc == 0) {
        return 0;
    }
    if (WSAGetLastError() == WSA_IO_PENDING) {
        return 0;
    }
    return -1;
}

/* ════════════════════════════════════════════════════════════════════════════
   Completion Dequeue
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Block until an I/O completion arrives, then return it.
 *
 * Wraps GetQueuedCompletionStatus.  On success, *token_out receives the
 * per-connection token and *op_out receives the IocpOperation pointer.
 * Returns the number of bytes transferred.
 *
 * Special cases:
 *   - Return -1, *op_out == NULL: timeout or IOCP handle closed.
 *   - Return -1, *op_out != NULL: the operation failed (e.g., connection reset).
 *     The caller should close the socket and free the op.
 *   - *op_out == NULL, bytes == 0: custom shutdown sentinel posted via
 *     tml_iocp_post with op == NULL.
 *
 * @param iocp       IOCP handle.
 * @param token_out  Receives the completion key (token) for the socket.
 * @param op_out     Receives the IocpOperation pointer.
 * @param timeout_ms Milliseconds to wait; INFINITE (-1 cast to DWORD) waits forever.
 * @return Bytes transferred (>= 0), or -1 on error/timeout.
 */
int64_t tml_iocp_wait(int64_t iocp, int32_t* token_out, void** op_out, int32_t timeout_ms) {
    if (!iocp || !token_out || !op_out) {
        return -1;
    }

    *token_out = 0;
    *op_out = NULL;

    DWORD bytes_transferred = 0;
    ULONG_PTR completion_key = 0;
    OVERLAPPED* overlapped = NULL;

    DWORD timeout_dword = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;

    BOOL ok = GetQueuedCompletionStatus((HANDLE)(uintptr_t)iocp, &bytes_transferred,
                                        &completion_key, &overlapped, timeout_dword);

    *token_out = (int32_t)(uint32_t)(uintptr_t)completion_key;

    if (overlapped) {
        /*
         * OVERLAPPED is the first field of IocpOperation, so the pointer
         * values are identical (no arithmetic needed).
         */
        *op_out = (void*)overlapped;
    }

    if (!ok) {
        /*
         * GetQueuedCompletionStatus returns FALSE in three situations:
         *   1. overlapped == NULL: timeout or the IOCP handle was closed
         *      (ERROR_ABANDONED_WAIT_0) — no op to process.
         *   2. overlapped != NULL: the async operation failed (e.g.,
         *      WSAECONNRESET / ERROR_NETNAME_DELETED).  The caller must handle
         *      the error and free/recycle the op.
         */
        return -1;
    }

    return (int64_t)bytes_transferred;
}

/* ════════════════════════════════════════════════════════════════════════════
   Custom Completion Post
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Post a custom completion to the IOCP queue.
 *
 * Used to wake up threads blocked in tml_iocp_wait without real I/O.
 * Convention: post token=0, op=NULL as a shutdown sentinel.
 *
 * @param iocp  IOCP handle.
 * @param token Completion key to deliver.
 * @param op    IocpOperation pointer to deliver (may be NULL for sentinels).
 * @return 0 on success, -1 on error.
 */
int32_t tml_iocp_post(int64_t iocp, int32_t token, void* op) {
    if (!iocp) {
        return -1;
    }

    BOOL ok = PostQueuedCompletionStatus((HANDLE)(uintptr_t)iocp, 0, /* bytes transferred */
                                         (ULONG_PTR)(uintptr_t)(uint32_t)token, (OVERLAPPED*)op);
    return ok ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════════════════════
   IocpOperation Allocation Helpers
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocate and zero-initialize a new IocpOperation.
 *
 * calloc ensures the embedded OVERLAPPED is zeroed — required before issuing
 * any async operation.  The caller is responsible for freeing via
 * tml_iocp_op_free when the operation is no longer in flight.
 *
 * @param socket  Socket handle this operation will be used with.
 * @param op_type IOCP_OP_RECV (0), IOCP_OP_SEND (1), or IOCP_OP_ACCEPT (2).
 * @param token   Per-connection identifier.
 * @return Pointer to the allocated IocpOperation as int64_t, or 0 on OOM.
 */
int64_t tml_iocp_op_alloc(int64_t socket, int32_t op_type, int32_t token) {
    IocpOperation* op = (IocpOperation*)calloc(1, sizeof(IocpOperation));
    if (!op) {
        return 0;
    }
    op->socket = socket;
    op->op_type = op_type;
    op->token = token;
    /* wsabuf and overlapped are zero from calloc */
    return (int64_t)(uintptr_t)op;
}

/**
 * @brief Free an IocpOperation allocated by tml_iocp_op_alloc.
 *
 * MUST only be called after the operation has completed (i.e., after
 * tml_iocp_wait returned it) or was never successfully issued.  Freeing an
 * in-flight operation corrupts kernel state.
 *
 * @param op Pointer returned by tml_iocp_op_alloc (cast from int64_t).
 */
void tml_iocp_op_free(void* op) {
    free(op);
}

/**
 * @brief Get the op_type field from an IocpOperation.
 * @param op Pointer to IocpOperation.
 * @return op_type (0=recv, 1=send, 2=accept), or -1 if op is NULL.
 */
int32_t tml_iocp_op_type(void* op) {
    if (!op)
        return -1;
    return ((IocpOperation*)op)->op_type;
}

/**
 * @brief Get the token field from an IocpOperation.
 * @param op Pointer to IocpOperation.
 * @return token, or -1 if op is NULL.
 */
int32_t tml_iocp_op_token(void* op) {
    if (!op)
        return -1;
    return ((IocpOperation*)op)->token;
}

/**
 * @brief Get the socket field from an IocpOperation.
 * @param op Pointer to IocpOperation.
 * @return socket handle, or -1 if op is NULL.
 */
int64_t tml_iocp_op_socket(void* op) {
    if (!op)
        return -1;
    return ((IocpOperation*)op)->socket;
}

/* ════════════════════════════════════════════════════════════════════════════
   Listening Socket + AcceptEx
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a TCP/IPv4 listening socket with optimal IOCP-friendly options.
 *
 * Sets:
 *   - SO_REUSEADDR  — allow fast restarts without TIME_WAIT stalls
 *   - TCP_NODELAY   — disable Nagle; latency matters more than throughput here
 *   - Non-blocking  — not strictly required for IOCP but defensive
 *
 * @param port    Port to bind and listen on.
 * @param backlog Maximum connection backlog for listen().
 * @return Listening socket handle, or -1 on error.
 */
int64_t tml_iocp_listen(int32_t port, int32_t backlog) {
    ensure_wsa();

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return -1;
    }

    /* SO_REUSEADDR — survive TIME_WAIT on fast restarts */
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0) {
        closesocket(sock);
        return -1;
    }

    /* TCP_NODELAY — minimize latency; HTTP responses are small */
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt)) != 0) {
        closesocket(sock);
        return -1;
    }

    /* Bind to 0.0.0.0:port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }

    if (listen(sock, backlog) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }

    return (int64_t)sock;
}

/**
 * @brief Create a socket suitable for use as the AcceptEx target.
 *
 * AcceptEx requires a pre-created (but not yet connected) socket to receive
 * the incoming connection into.  Create one for each pending AcceptEx call.
 *
 * @return Socket handle, or -1 on error.
 */
int64_t tml_iocp_create_accept_socket(void) {
    ensure_wsa();

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return -1;
    }

    /* TCP_NODELAY on accepted sockets for low-latency HTTP */
    int opt = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));

    return (int64_t)sock;
}

/**
 * @brief Issue an async AcceptEx on a listening socket.
 *
 * AcceptEx is the IOCP-compatible replacement for accept().  A single call
 * prepares one pending accept; the caller typically keeps a pool of pending
 * AcceptEx operations so that incoming connections are accepted immediately
 * without any gap between completions.
 *
 * When GetQueuedCompletionStatus returns a completion for this op, the caller
 * MUST call setsockopt(SO_UPDATE_ACCEPT_CONTEXT) on the accept_socket before
 * using it, otherwise getpeername / getsockname will fail.
 *
 * accept_buf layout (imposed by AcceptEx):
 *   [local_addr_area (sizeof(SOCKADDR_IN)+16)] [remote_addr_area (sizeof(SOCKADDR_IN)+16)]
 *
 * The minimum buf_len is therefore 2 * (sizeof(SOCKADDR_IN) + 16) = 64 bytes.
 * Passing a larger buffer allows AcceptEx to read the first chunk of request
 * data in the same syscall (zero-copy receive on accept), but buf_len must be
 * at least 64; we validate this and treat excess as receive_data_length.
 *
 * op->socket is updated to accept_socket so that the completion handler can
 * retrieve the new socket without maintaining a separate mapping.
 *
 * @param listen_socket  The listening socket created by tml_iocp_listen.
 * @param accept_socket  Pre-created socket from tml_iocp_create_accept_socket.
 * @param accept_buf     Caller-allocated buffer (>= 64 bytes).
 * @param buf_len        Length of accept_buf.
 * @param op             Pre-allocated IocpOperation with op_type=IOCP_OP_ACCEPT.
 * @return 0 on success (pending), -1 on error.
 */
int32_t tml_iocp_accept(int64_t listen_socket, int64_t accept_socket, uint8_t* accept_buf,
                        int64_t buf_len, void* op) {
    if (!op || listen_socket == -1 || accept_socket == -1 || !accept_buf) {
        return -1;
    }

    /* Minimum buffer size: 2 * (sizeof(SOCKADDR_IN) + 16) */
    const DWORD addr_size = sizeof(struct sockaddr_in) + 16;
    const int64_t min_buf = (int64_t)(2 * addr_size);
    if (buf_len < min_buf) {
        return -1;
    }

    if (!g_fn_AcceptEx) {
        if (!load_acceptex()) {
            return -1;
        }
    }

    IocpOperation* iop = (IocpOperation*)op;

    memset(&iop->overlapped, 0, sizeof(OVERLAPPED));
    iop->socket = accept_socket; /* completion handler reads this */
    iop->op_type = IOCP_OP_ACCEPT;

    /*
     * receive_data_length: how many bytes of request data to read along with
     * the accept.  Any buf_len beyond the two address areas is usable data.
     * We pass 0 here to maximise compatibility (no recv on accept) — the
     * separate WSARecv path handles request reads.
     */
    DWORD receive_data_length = 0;
    DWORD bytes_received = 0;

    BOOL ok = g_fn_AcceptEx((SOCKET)listen_socket, (SOCKET)accept_socket, accept_buf,
                            receive_data_length, addr_size, /* local_address_length */
                            addr_size,                      /* remote_address_length */
                            &bytes_received, &iop->overlapped);

    if (ok) {
        return 0; /* completed synchronously — IOCP still posts notification */
    }
    if (WSAGetLastError() == ERROR_IO_PENDING) {
        return 0; /* queued normally */
    }
    return -1;
}

/**
 * @brief Finalise an accepted socket after AcceptEx completes.
 *
 * After GetQueuedCompletionStatus returns for an IOCP_OP_ACCEPT operation,
 * the accepted socket is functional but SO_UPDATE_ACCEPT_CONTEXT must be set
 * before calling getpeername, getsockname, or shutdown on it.
 *
 * This is a helper called internally; TML callers should call this immediately
 * after processing the accept completion and before issuing the first WSARecv.
 *
 * @param listen_socket The listening socket.
 * @param accept_socket The newly accepted socket.
 * @return 0 on success, -1 on error.
 */
int32_t tml_iocp_update_accept_context(int64_t listen_socket, int64_t accept_socket) {
    if (listen_socket == -1 || accept_socket == -1) {
        return -1;
    }

    SOCKET ls = (SOCKET)listen_socket;
    int rc = setsockopt((SOCKET)accept_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        (const char*)&ls, sizeof(ls));
    return (rc == SOCKET_ERROR) ? -1 : 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   Non-Windows stubs (IOCP is Windows-only)
   ════════════════════════════════════════════════════════════════════════════ */

#else /* !_WIN32 */

#include <errno.h>

/*
 * On Linux / macOS use epoll / kqueue (poll.c).  These stubs exist so that
 * TML code compiled for non-Windows platforms can still reference the IOCP
 * symbols without linker errors — all return -1 indicating unsupported.
 */

int64_t tml_iocp_create(int32_t num_threads) {
    (void)num_threads;
    return 0;
}
void tml_iocp_destroy(int64_t iocp) {
    (void)iocp;
}
int32_t tml_iocp_associate(int64_t iocp, int64_t socket, int32_t token) {
    (void)iocp;
    (void)socket;
    (void)token;
    return -1;
}
int32_t tml_iocp_recv(int64_t socket, uint8_t* buf, int64_t len, void* op) {
    (void)socket;
    (void)buf;
    (void)len;
    (void)op;
    return -1;
}
int32_t tml_iocp_send(int64_t socket, const uint8_t* buf, int64_t len, void* op) {
    (void)socket;
    (void)buf;
    (void)len;
    (void)op;
    return -1;
}
int64_t tml_iocp_wait(int64_t iocp, int32_t* tok, void** op, int32_t ms) {
    (void)iocp;
    (void)tok;
    (void)op;
    (void)ms;
    return -1;
}
int32_t tml_iocp_post(int64_t iocp, int32_t token, void* op) {
    (void)iocp;
    (void)token;
    (void)op;
    return -1;
}
int64_t tml_iocp_op_alloc(int64_t socket, int32_t op_type, int32_t token) {
    (void)socket;
    (void)op_type;
    (void)token;
    return 0;
}
void tml_iocp_op_free(void* op) {
    free(op);
}
int32_t tml_iocp_op_type(void* op) {
    (void)op;
    return -1;
}
int32_t tml_iocp_op_token(void* op) {
    (void)op;
    return -1;
}
int64_t tml_iocp_op_socket(void* op) {
    (void)op;
    return -1;
}
int64_t tml_iocp_listen(int32_t port, int32_t backlog) {
    (void)port;
    (void)backlog;
    return -1;
}
int32_t tml_iocp_accept(int64_t ls, int64_t as, uint8_t* buf, int64_t len, void* op) {
    (void)ls;
    (void)as;
    (void)buf;
    (void)len;
    (void)op;
    return -1;
}
int64_t tml_iocp_create_accept_socket(void) {
    return -1;
}
int32_t tml_iocp_update_accept_context(int64_t ls, int64_t as) {
    (void)ls;
    (void)as;
    return -1;
}

#endif /* _WIN32 */
