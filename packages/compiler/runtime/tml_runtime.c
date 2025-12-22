// TML Runtime - Threading and Channel Support
// Provides Go-style concurrency primitives

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// ============ THREAD PRIMITIVES ============

typedef void (*ThreadFunc)(void* arg);

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

void* tml_thread_spawn(ThreadFunc func, void* arg) {
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args->func = func;
    args->arg = arg;
    HANDLE handle = CreateThread(NULL, 0, thread_wrapper, args, 0, NULL);
    return (void*)handle;
}

void tml_thread_join(void* handle) {
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
}

void tml_thread_yield(void) {
    SwitchToThread();
}

void tml_thread_sleep(int32_t ms) {
    Sleep(ms);
}

int32_t tml_thread_id(void) {
    return (int32_t)GetCurrentThreadId();
}

#else
static void* thread_wrapper(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    args->func(args->arg);
    free(args);
    return NULL;
}

void* tml_thread_spawn(ThreadFunc func, void* arg) {
    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args->func = func;
    args->arg = arg;
    pthread_create(thread, NULL, thread_wrapper, args);
    return (void*)thread;
}

void tml_thread_join(void* handle) {
    pthread_join(*(pthread_t*)handle, NULL);
    free(handle);
}

void tml_thread_yield(void) {
    sched_yield();
}

void tml_thread_sleep(int32_t ms) {
    usleep(ms * 1000);
}

int32_t tml_thread_id(void) {
    return (int32_t)pthread_self();
}
#endif

// ============ CHANNEL (Go-style) ============

#define CHANNEL_BUFFER_SIZE 256

typedef struct {
    int32_t buffer[CHANNEL_BUFFER_SIZE];
    volatile int32_t head;
    volatile int32_t tail;
    volatile int32_t count;
    volatile int32_t closed;
    volatile int32_t lock;
} Channel;

void* tml_channel_create(void) {
    Channel* ch = (Channel*)malloc(sizeof(Channel));
    memset(ch, 0, sizeof(Channel));
    return ch;
}

void tml_channel_close(void* ch_ptr) {
    Channel* ch = (Channel*)ch_ptr;
    ch->closed = 1;
}

void tml_channel_destroy(void* ch_ptr) {
    free(ch_ptr);
}

// Spinlock helpers
static void channel_lock(Channel* ch) {
    while (__sync_lock_test_and_set(&ch->lock, 1)) {
        #ifdef _WIN32
        SwitchToThread();
        #else
        sched_yield();
        #endif
    }
}

static void channel_unlock(Channel* ch) {
    __sync_lock_release(&ch->lock);
}

// Send value to channel (blocks if full)
int32_t tml_channel_send(void* ch_ptr, int32_t value) {
    Channel* ch = (Channel*)ch_ptr;

    while (1) {
        if (ch->closed) return 0; // Channel closed

        channel_lock(ch);
        if (ch->count < CHANNEL_BUFFER_SIZE) {
            ch->buffer[ch->tail] = value;
            ch->tail = (ch->tail + 1) % CHANNEL_BUFFER_SIZE;
            ch->count++;
            channel_unlock(ch);
            return 1; // Success
        }
        channel_unlock(ch);

        // Buffer full, yield and retry
        #ifdef _WIN32
        SwitchToThread();
        #else
        sched_yield();
        #endif
    }
}

// Receive from channel (blocks if empty)
int32_t tml_channel_recv(void* ch_ptr, int32_t* out_value) {
    Channel* ch = (Channel*)ch_ptr;

    while (1) {
        channel_lock(ch);
        if (ch->count > 0) {
            *out_value = ch->buffer[ch->head];
            ch->head = (ch->head + 1) % CHANNEL_BUFFER_SIZE;
            ch->count--;
            channel_unlock(ch);
            return 1; // Success
        }
        channel_unlock(ch);

        if (ch->closed) return 0; // Channel closed and empty

        // Buffer empty, yield and retry
        #ifdef _WIN32
        SwitchToThread();
        #else
        sched_yield();
        #endif
    }
}

// Try to receive (non-blocking)
int32_t tml_channel_try_recv(void* ch_ptr, int32_t* out_value) {
    Channel* ch = (Channel*)ch_ptr;

    channel_lock(ch);
    if (ch->count > 0) {
        *out_value = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % CHANNEL_BUFFER_SIZE;
        ch->count--;
        channel_unlock(ch);
        return 1; // Success
    }
    channel_unlock(ch);
    return 0; // No data available
}

// Try to send (non-blocking)
int32_t tml_channel_try_send(void* ch_ptr, int32_t value) {
    Channel* ch = (Channel*)ch_ptr;

    if (ch->closed) return 0;

    channel_lock(ch);
    if (ch->count < CHANNEL_BUFFER_SIZE) {
        ch->buffer[ch->tail] = value;
        ch->tail = (ch->tail + 1) % CHANNEL_BUFFER_SIZE;
        ch->count++;
        channel_unlock(ch);
        return 1; // Success
    }
    channel_unlock(ch);
    return 0; // Buffer full
}

// Get channel length
int32_t tml_channel_len(void* ch_ptr) {
    Channel* ch = (Channel*)ch_ptr;
    return ch->count;
}

// ============ MUTEX ============

typedef struct {
    volatile int32_t locked;
} Mutex;

void* tml_mutex_create(void) {
    Mutex* m = (Mutex*)malloc(sizeof(Mutex));
    m->locked = 0;
    return m;
}

void tml_mutex_lock(void* m_ptr) {
    Mutex* m = (Mutex*)m_ptr;
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        #ifdef _WIN32
        SwitchToThread();
        #else
        sched_yield();
        #endif
    }
}

void tml_mutex_unlock(void* m_ptr) {
    Mutex* m = (Mutex*)m_ptr;
    __sync_lock_release(&m->locked);
}

int32_t tml_mutex_try_lock(void* m_ptr) {
    Mutex* m = (Mutex*)m_ptr;
    return !__sync_lock_test_and_set(&m->locked, 1);
}

void tml_mutex_destroy(void* m_ptr) {
    free(m_ptr);
}

// ============ WAIT GROUP (Go-style) ============

typedef struct {
    volatile int32_t count;
    volatile int32_t lock;
} WaitGroup;

void* tml_waitgroup_create(void) {
    WaitGroup* wg = (WaitGroup*)malloc(sizeof(WaitGroup));
    wg->count = 0;
    wg->lock = 0;
    return wg;
}

void tml_waitgroup_add(void* wg_ptr, int32_t delta) {
    WaitGroup* wg = (WaitGroup*)wg_ptr;
    __sync_fetch_and_add(&wg->count, delta);
}

void tml_waitgroup_done(void* wg_ptr) {
    WaitGroup* wg = (WaitGroup*)wg_ptr;
    __sync_fetch_and_sub(&wg->count, 1);
}

void tml_waitgroup_wait(void* wg_ptr) {
    WaitGroup* wg = (WaitGroup*)wg_ptr;
    while (__sync_fetch_and_add(&wg->count, 0) > 0) {
        #ifdef _WIN32
        SwitchToThread();
        #else
        sched_yield();
        #endif
    }
}

void tml_waitgroup_destroy(void* wg_ptr) {
    free(wg_ptr);
}

// ============ ATOMIC COUNTER ============

void* tml_atomic_counter_create(int32_t initial) {
    int32_t* counter = (int32_t*)malloc(sizeof(int32_t));
    *counter = initial;
    return counter;
}

int32_t tml_atomic_counter_inc(void* counter) {
    return __sync_fetch_and_add((int32_t*)counter, 1);
}

int32_t tml_atomic_counter_dec(void* counter) {
    return __sync_fetch_and_sub((int32_t*)counter, 1);
}

int32_t tml_atomic_counter_get(void* counter) {
    return __sync_fetch_and_add((int32_t*)counter, 0);
}

void tml_atomic_counter_set(void* counter, int32_t value) {
    __sync_lock_test_and_set((int32_t*)counter, value);
}

void tml_atomic_counter_destroy(void* counter) {
    free(counter);
}
