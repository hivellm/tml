/**
 * @file thread.c
 * @brief TML Runtime - Threading Primitives
 *
 * Implements threading and synchronization primitives for the TML language.
 * Provides cross-platform thread creation, channels, mutexes, wait groups,
 * and atomic operations.
 *
 * ## Components
 *
 * - **Thread primitives**: `thread_spawn`, `thread_join`, `thread_yield`, `thread_sleep`
 * - **Channel (Go-style)**: Bounded MPMC channel with blocking operations
 * - **Mutex**: Simple mutual exclusion lock
 * - **WaitGroup**: Wait for multiple operations to complete
 * - **AtomicCounter**: Thread-safe counter with atomic operations
 *
 * ## Platform Support
 *
 * - **Windows**: Uses Win32 API (CreateThread, CriticalSection, ConditionVariable)
 * - **POSIX**: Uses pthreads (pthread_create, pthread_mutex, pthread_cond)
 *
 * ## Channel Semantics
 *
 * Channels follow Go-style semantics:
 * - `channel_send` blocks until space is available
 * - `channel_recv` blocks until data is available
 * - `channel_try_send`/`channel_try_recv` are non-blocking variants
 * - Closing a channel wakes all waiting senders/receivers
 *
 * ## Thread Safety
 *
 * All primitives in this file are designed for concurrent use from multiple
 * threads. Internal synchronization is handled automatically.
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

// ============================================================================
// Thread Primitives
// ============================================================================

typedef void (*ThreadFunc)(void*);

typedef struct {
    ThreadFunc func;
    void* arg;
} ThreadArgs;

#ifdef _WIN32
static DWORD WINAPI thread_wrapper(LPVOID arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    args->func(args->arg);
    free(args);
    return 0;
}

void* thread_spawn(ThreadFunc func, void* arg) {
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args->func = func;
    args->arg = arg;
    HANDLE handle = CreateThread(NULL, 0, thread_wrapper, args, 0, NULL);
    return (void*)handle;
}

void thread_join(void* handle) {
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
}

void thread_yield(void) {
    SwitchToThread();
}

void thread_sleep_ms(int64_t ms) {
    Sleep((DWORD)ms);
}
#else
static void* thread_wrapper(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    args->func(args->arg);
    free(args);
    return NULL;
}

void* thread_spawn(ThreadFunc func, void* arg) {
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args->func = func;
    args->arg = arg;
    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));
    pthread_create(thread, NULL, thread_wrapper, args);
    return thread;
}

void thread_join(void* handle) {
    pthread_join(*(pthread_t*)handle, NULL);
    free(handle);
}

void thread_yield(void) {
    sched_yield();
}

void thread_sleep_ms(int64_t ms) {
    usleep((useconds_t)(ms * 1000));
}
#endif

// ============ CHANNEL (Go-style) ============

typedef struct Channel {
    void** buffer;
    int64_t capacity;
    int64_t head;
    int64_t tail;
    int64_t count;
    int closed;
#ifdef _WIN32
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE not_empty;
    CONDITION_VARIABLE not_full;
#else
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif
} Channel;

Channel* channel_new(int64_t capacity) {
    Channel* ch = (Channel*)malloc(sizeof(Channel));
    ch->capacity = capacity > 0 ? capacity : 1;
    ch->buffer = (void**)malloc(sizeof(void*) * (size_t)ch->capacity);
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = 0;
#ifdef _WIN32
    InitializeCriticalSection(&ch->lock);
    InitializeConditionVariable(&ch->not_empty);
    InitializeConditionVariable(&ch->not_full);
#else
    pthread_mutex_init(&ch->lock, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    pthread_cond_init(&ch->not_full, NULL);
#endif
    return ch;
}

void channel_send(Channel* ch, void* data) {
#ifdef _WIN32
    EnterCriticalSection(&ch->lock);
    while (ch->count == ch->capacity && !ch->closed) {
        SleepConditionVariableCS(&ch->not_full, &ch->lock, INFINITE);
    }
    if (!ch->closed) {
        ch->buffer[ch->tail] = data;
        ch->tail = (ch->tail + 1) % ch->capacity;
        ch->count++;
        WakeConditionVariable(&ch->not_empty);
    }
    LeaveCriticalSection(&ch->lock);
#else
    pthread_mutex_lock(&ch->lock);
    while (ch->count == ch->capacity && !ch->closed) {
        pthread_cond_wait(&ch->not_full, &ch->lock);
    }
    if (!ch->closed) {
        ch->buffer[ch->tail] = data;
        ch->tail = (ch->tail + 1) % ch->capacity;
        ch->count++;
        pthread_cond_signal(&ch->not_empty);
    }
    pthread_mutex_unlock(&ch->lock);
#endif
}

void* channel_recv(Channel* ch) {
    void* data = NULL;
#ifdef _WIN32
    EnterCriticalSection(&ch->lock);
    while (ch->count == 0 && !ch->closed) {
        SleepConditionVariableCS(&ch->not_empty, &ch->lock, INFINITE);
    }
    if (ch->count > 0) {
        data = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % ch->capacity;
        ch->count--;
        WakeConditionVariable(&ch->not_full);
    }
    LeaveCriticalSection(&ch->lock);
#else
    pthread_mutex_lock(&ch->lock);
    while (ch->count == 0 && !ch->closed) {
        pthread_cond_wait(&ch->not_empty, &ch->lock);
    }
    if (ch->count > 0) {
        data = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % ch->capacity;
        ch->count--;
        pthread_cond_signal(&ch->not_full);
    }
    pthread_mutex_unlock(&ch->lock);
#endif
    return data;
}

int channel_try_send(Channel* ch, void* data) {
    int success = 0;
#ifdef _WIN32
    EnterCriticalSection(&ch->lock);
    if (ch->count < ch->capacity && !ch->closed) {
        ch->buffer[ch->tail] = data;
        ch->tail = (ch->tail + 1) % ch->capacity;
        ch->count++;
        WakeConditionVariable(&ch->not_empty);
        success = 1;
    }
    LeaveCriticalSection(&ch->lock);
#else
    pthread_mutex_lock(&ch->lock);
    if (ch->count < ch->capacity && !ch->closed) {
        ch->buffer[ch->tail] = data;
        ch->tail = (ch->tail + 1) % ch->capacity;
        ch->count++;
        pthread_cond_signal(&ch->not_empty);
        success = 1;
    }
    pthread_mutex_unlock(&ch->lock);
#endif
    return success;
}

void* channel_try_recv(Channel* ch) {
    void* data = NULL;
#ifdef _WIN32
    EnterCriticalSection(&ch->lock);
    if (ch->count > 0) {
        data = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % ch->capacity;
        ch->count--;
        WakeConditionVariable(&ch->not_full);
    }
    LeaveCriticalSection(&ch->lock);
#else
    pthread_mutex_lock(&ch->lock);
    if (ch->count > 0) {
        data = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % ch->capacity;
        ch->count--;
        pthread_cond_signal(&ch->not_full);
    }
    pthread_mutex_unlock(&ch->lock);
#endif
    return data;
}

void channel_close(Channel* ch) {
#ifdef _WIN32
    EnterCriticalSection(&ch->lock);
    ch->closed = 1;
    WakeAllConditionVariable(&ch->not_empty);
    WakeAllConditionVariable(&ch->not_full);
    LeaveCriticalSection(&ch->lock);
#else
    pthread_mutex_lock(&ch->lock);
    ch->closed = 1;
    pthread_cond_broadcast(&ch->not_empty);
    pthread_cond_broadcast(&ch->not_full);
    pthread_mutex_unlock(&ch->lock);
#endif
}

int channel_is_closed(Channel* ch) {
    return ch->closed;
}

void channel_free(Channel* ch) {
#ifdef _WIN32
    DeleteCriticalSection(&ch->lock);
#else
    pthread_mutex_destroy(&ch->lock);
    pthread_cond_destroy(&ch->not_empty);
    pthread_cond_destroy(&ch->not_full);
#endif
    free(ch->buffer);
    free(ch);
}

// ============ MUTEX ============

typedef struct Mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mtx;
#endif
} Mutex;

Mutex* mutex_new(void) {
    Mutex* m = (Mutex*)malloc(sizeof(Mutex));
#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
#else
    pthread_mutex_init(&m->mtx, NULL);
#endif
    return m;
}

void mutex_lock(Mutex* m) {
#ifdef _WIN32
    EnterCriticalSection(&m->cs);
#else
    pthread_mutex_lock(&m->mtx);
#endif
}

void mutex_unlock(Mutex* m) {
#ifdef _WIN32
    LeaveCriticalSection(&m->cs);
#else
    pthread_mutex_unlock(&m->mtx);
#endif
}

int mutex_try_lock(Mutex* m) {
#ifdef _WIN32
    return TryEnterCriticalSection(&m->cs);
#else
    return pthread_mutex_trylock(&m->mtx) == 0;
#endif
}

void mutex_free(Mutex* m) {
#ifdef _WIN32
    DeleteCriticalSection(&m->cs);
#else
    pthread_mutex_destroy(&m->mtx);
#endif
    free(m);
}

// ============ WAIT GROUP ============

typedef struct WaitGroup {
    atomic_int_fast64_t count;
#ifdef _WIN32
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE cond;
#else
    pthread_mutex_t lock;
    pthread_cond_t cond;
#endif
} WaitGroup;

WaitGroup* waitgroup_new(void) {
    WaitGroup* wg = (WaitGroup*)malloc(sizeof(WaitGroup));
    atomic_init(&wg->count, 0);
#ifdef _WIN32
    InitializeCriticalSection(&wg->lock);
    InitializeConditionVariable(&wg->cond);
#else
    pthread_mutex_init(&wg->lock, NULL);
    pthread_cond_init(&wg->cond, NULL);
#endif
    return wg;
}

void waitgroup_add(WaitGroup* wg, int64_t delta) {
    atomic_fetch_add(&wg->count, delta);
}

void waitgroup_done(WaitGroup* wg) {
    int64_t old = atomic_fetch_sub(&wg->count, 1);
    if (old == 1) {
#ifdef _WIN32
        WakeAllConditionVariable(&wg->cond);
#else
        pthread_cond_broadcast(&wg->cond);
#endif
    }
}

void waitgroup_wait(WaitGroup* wg) {
#ifdef _WIN32
    EnterCriticalSection(&wg->lock);
    while (atomic_load(&wg->count) > 0) {
        SleepConditionVariableCS(&wg->cond, &wg->lock, INFINITE);
    }
    LeaveCriticalSection(&wg->lock);
#else
    pthread_mutex_lock(&wg->lock);
    while (atomic_load(&wg->count) > 0) {
        pthread_cond_wait(&wg->cond, &wg->lock);
    }
    pthread_mutex_unlock(&wg->lock);
#endif
}

void waitgroup_free(WaitGroup* wg) {
#ifdef _WIN32
    DeleteCriticalSection(&wg->lock);
#else
    pthread_mutex_destroy(&wg->lock);
    pthread_cond_destroy(&wg->cond);
#endif
    free(wg);
}

// ============ ATOMIC COUNTER ============

typedef struct AtomicCounter {
    atomic_int_fast64_t value;
} AtomicCounter;

AtomicCounter* atomic_new(int64_t initial) {
    AtomicCounter* a = (AtomicCounter*)malloc(sizeof(AtomicCounter));
    atomic_init(&a->value, initial);
    return a;
}

int64_t atomic_load(AtomicCounter* a) {
    return atomic_load(&a->value);
}

void atomic_store(AtomicCounter* a, int64_t value) {
    atomic_store(&a->value, value);
}

int64_t atomic_add(AtomicCounter* a, int64_t delta) {
    return atomic_fetch_add(&a->value, delta);
}

int64_t atomic_sub(AtomicCounter* a, int64_t delta) {
    return atomic_fetch_sub(&a->value, delta);
}

void atomic_free(AtomicCounter* a) {
    free(a);
}

// ============ WRAPPER FUNCTIONS (for codegen compatibility) ============

// thread_sleep(ms: I32) -> Unit - Wrapper for thread_sleep_ms
void thread_sleep(int32_t ms) {
    thread_sleep_ms((int64_t)ms);
}

// thread_id() -> I32 - Get current thread ID
#ifdef _WIN32
int32_t thread_id(void) {
    return (int32_t)GetCurrentThreadId();
}
#else
#include <sys/types.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
int32_t thread_id(void) {
#if defined(__linux__) && defined(SYS_gettid)
    return (int32_t)syscall(SYS_gettid);
#else
    return (int32_t)(intptr_t)pthread_self();
#endif
}
#endif

// ============ CHANNEL WRAPPERS ============

// channel_create() -> Channel* - Wrapper for channel_new with default capacity
Channel* channel_create(void) {
    return channel_new(16);
}

// channel_destroy(ch) -> Unit - Wrapper for channel_free
void channel_destroy(Channel* ch) {
    channel_free(ch);
}

// channel_len(ch) -> I32 - Get number of items in channel
int32_t channel_len(Channel* ch) {
    if (!ch)
        return 0;
    return (int32_t)ch->count;
}

// ============ MUTEX WRAPPERS ============

// mutex_create() -> Mutex* - Wrapper for mutex_new
Mutex* mutex_create(void) {
    return mutex_new();
}

// mutex_destroy(m) -> Unit - Wrapper for mutex_free
void mutex_destroy(Mutex* m) {
    mutex_free(m);
}

// ============ WAITGROUP WRAPPERS ============

// waitgroup_create() -> WaitGroup* - Wrapper for waitgroup_new
WaitGroup* waitgroup_create(void) {
    return waitgroup_new();
}

// waitgroup_destroy(wg) -> Unit - Wrapper for waitgroup_free
void waitgroup_destroy(WaitGroup* wg) {
    waitgroup_free(wg);
}
