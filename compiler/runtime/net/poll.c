/**
 * @file poll.c
 * @brief TML Runtime - I/O Event Polling
 *
 * Cross-platform I/O multiplexing for the TML async event loop.
 * Uses epoll on Linux, WSAPoll on Windows.
 *
 * ## Functions
 *
 * - `tml_poll_create`  - Create a poller instance
 * - `tml_poll_destroy` - Destroy a poller instance
 * - `tml_poll_add`     - Register a socket for events
 * - `tml_poll_modify`  - Modify registered interests
 * - `tml_poll_remove`  - Remove a socket from the poller
 * - `tml_poll_wait`    - Wait for I/O events (blocking with timeout)
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Interest flags (match TML constants) */
#define POLL_READABLE 1
#define POLL_WRITABLE 2
#define POLL_ERROR 4
#define POLL_HUP 8

/* Output event struct — 8 bytes, matches TML PollEvent { token: U32, flags: U32 } */
typedef struct {
    uint32_t token;
    uint32_t flags;
} PollEvent;

/* ========================================================================== */
/* Windows: WSAPoll-based implementation                                      */
/* ========================================================================== */
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* Ensure Winsock is initialized (same pattern as net.c) */
extern int32_t sys_wsa_startup(void);
static int wsa_poll_initialized = 0;

/**
 * Poller state for Windows (dynamic array of WSAPOLLFD + token mapping).
 *
 * We maintain parallel arrays:
 *   fds[i]    — WSAPOLLFD for WSAPoll
 *   tokens[i] — user-assigned token for fds[i]
 */
typedef struct {
    WSAPOLLFD* fds;
    uint32_t* tokens;
    int32_t count;
    int32_t capacity;
} WinPoller;

static WinPoller* win_poller_new(void) {
    WinPoller* p = (WinPoller*)calloc(1, sizeof(WinPoller));
    if (!p)
        return NULL;
    p->capacity = 64;
    p->fds = (WSAPOLLFD*)calloc(p->capacity, sizeof(WSAPOLLFD));
    p->tokens = (uint32_t*)calloc(p->capacity, sizeof(uint32_t));
    p->count = 0;
    return p;
}

static void win_poller_destroy(WinPoller* p) {
    if (!p)
        return;
    free(p->fds);
    free(p->tokens);
    free(p);
}

static int win_poller_find(WinPoller* p, SOCKET fd) {
    for (int i = 0; i < p->count; i++) {
        if (p->fds[i].fd == fd)
            return i;
    }
    return -1;
}

static void win_poller_grow(WinPoller* p) {
    int32_t new_cap = p->capacity * 2;
    p->fds = (WSAPOLLFD*)realloc(p->fds, new_cap * sizeof(WSAPOLLFD));
    p->tokens = (uint32_t*)realloc(p->tokens, new_cap * sizeof(uint32_t));
    p->capacity = new_cap;
}

static short interests_to_pollflags(uint32_t interests) {
    short events = 0;
    if (interests & POLL_READABLE)
        events |= POLLIN;
    if (interests & POLL_WRITABLE)
        events |= POLLOUT;
    return events;
}

static uint32_t pollflags_to_interests(short revents) {
    uint32_t flags = 0;
    if (revents & POLLIN)
        flags |= POLL_READABLE;
    if (revents & POLLOUT)
        flags |= POLL_WRITABLE;
    if (revents & POLLERR)
        flags |= POLL_ERROR;
    if (revents & POLLHUP)
        flags |= POLL_HUP;
    return flags;
}

/* ── Public API (Windows) ─────────────────────────────────────────────────── */

int64_t tml_poll_create(void) {
    if (!wsa_poll_initialized) {
        sys_wsa_startup();
        wsa_poll_initialized = 1;
    }
    WinPoller* p = win_poller_new();
    return (int64_t)(uintptr_t)p;
}

void tml_poll_destroy(int64_t poller) {
    win_poller_destroy((WinPoller*)(uintptr_t)poller);
}

int32_t tml_poll_add(int64_t poller, int64_t socket_handle, uint32_t token, uint32_t interests) {
    WinPoller* p = (WinPoller*)(uintptr_t)poller;
    if (!p)
        return -1;

    SOCKET fd = (SOCKET)socket_handle;
    if (win_poller_find(p, fd) >= 0)
        return -2; /* already registered */

    if (p->count >= p->capacity)
        win_poller_grow(p);

    int idx = p->count++;
    memset(&p->fds[idx], 0, sizeof(WSAPOLLFD));
    p->fds[idx].fd = fd;
    p->fds[idx].events = interests_to_pollflags(interests);
    p->tokens[idx] = token;
    return 0;
}

int32_t tml_poll_modify(int64_t poller, int64_t socket_handle, uint32_t token, uint32_t interests) {
    WinPoller* p = (WinPoller*)(uintptr_t)poller;
    if (!p)
        return -1;

    SOCKET fd = (SOCKET)socket_handle;
    int idx = win_poller_find(p, fd);
    if (idx < 0)
        return -2; /* not found */

    p->fds[idx].events = interests_to_pollflags(interests);
    p->tokens[idx] = token;
    return 0;
}

int32_t tml_poll_remove(int64_t poller, int64_t socket_handle) {
    WinPoller* p = (WinPoller*)(uintptr_t)poller;
    if (!p)
        return -1;

    SOCKET fd = (SOCKET)socket_handle;
    int idx = win_poller_find(p, fd);
    if (idx < 0)
        return -2; /* not found */

    /* Swap with last element for O(1) removal */
    p->count--;
    if (idx < p->count) {
        p->fds[idx] = p->fds[p->count];
        p->tokens[idx] = p->tokens[p->count];
    }
    return 0;
}

int32_t tml_poll_wait(int64_t poller, void* events_out, int32_t max_events, int32_t timeout_ms) {
    WinPoller* p = (WinPoller*)(uintptr_t)poller;
    if (!p || p->count == 0)
        return 0;

    int ret = WSAPoll(p->fds, (ULONG)p->count, timeout_ms);
    if (ret <= 0)
        return ret; /* 0 = timeout, -1 = error */

    PollEvent* out = (PollEvent*)events_out;
    int32_t n = 0;

    for (int i = 0; i < p->count && n < max_events; i++) {
        if (p->fds[i].revents != 0) {
            out[n].token = p->tokens[i];
            out[n].flags = pollflags_to_interests(p->fds[i].revents);
            p->fds[i].revents = 0; /* clear for next poll */
            n++;
        }
    }
    return n;
}

/* ========================================================================== */
/* Linux: epoll-based implementation                                          */
/* ========================================================================== */
#else

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

static uint32_t interests_to_epoll(uint32_t interests) {
    uint32_t events = 0;
    if (interests & POLL_READABLE)
        events |= EPOLLIN;
    if (interests & POLL_WRITABLE)
        events |= EPOLLOUT;
    return events;
}

static uint32_t epoll_to_interests(uint32_t events) {
    uint32_t flags = 0;
    if (events & EPOLLIN)
        flags |= POLL_READABLE;
    if (events & EPOLLOUT)
        flags |= POLL_WRITABLE;
    if (events & EPOLLERR)
        flags |= POLL_ERROR;
    if (events & EPOLLHUP)
        flags |= POLL_HUP;
    return flags;
}

/* ── Public API (Linux) ───────────────────────────────────────────────────── */

int64_t tml_poll_create(void) {
    int fd = epoll_create1(0);
    if (fd < 0)
        return -1;
    return (int64_t)fd;
}

void tml_poll_destroy(int64_t poller) {
    if (poller >= 0)
        close((int)poller);
}

int32_t tml_poll_add(int64_t poller, int64_t socket_handle, uint32_t token, uint32_t interests) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = interests_to_epoll(interests);
    ev.data.u32 = token;
    return epoll_ctl((int)poller, EPOLL_CTL_ADD, (int)socket_handle, &ev) == 0 ? 0 : -1;
}

int32_t tml_poll_modify(int64_t poller, int64_t socket_handle, uint32_t token, uint32_t interests) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = interests_to_epoll(interests);
    ev.data.u32 = token;
    return epoll_ctl((int)poller, EPOLL_CTL_MOD, (int)socket_handle, &ev) == 0 ? 0 : -1;
}

int32_t tml_poll_remove(int64_t poller, int64_t socket_handle) {
    return epoll_ctl((int)poller, EPOLL_CTL_DEL, (int)socket_handle, NULL) == 0 ? 0 : -1;
}

int32_t tml_poll_wait(int64_t poller, void* events_out, int32_t max_events, int32_t timeout_ms) {
    /* Stack-allocate epoll_events, copy to our output format */
    struct epoll_event ep_events[256];
    int32_t limit = max_events < 256 ? max_events : 256;

    int ret = epoll_wait((int)poller, ep_events, limit, timeout_ms);
    if (ret <= 0)
        return ret;

    PollEvent* out = (PollEvent*)events_out;
    for (int i = 0; i < ret; i++) {
        out[i].token = ep_events[i].data.u32;
        out[i].flags = epoll_to_interests(ep_events[i].events);
    }
    return ret;
}

#endif
