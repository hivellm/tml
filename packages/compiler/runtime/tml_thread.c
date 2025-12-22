// TML Runtime - Threading
// Thread, Channel, Mutex, WaitGroup, Atomic

#include "tml_runtime.h"

// ============ THREAD PRIMITIVES ============

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

void tml_thread_sleep_ms(int64_t ms) {
    Sleep((DWORD)ms);
}
#else
static void* thread_wrapper(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    args->func(args->arg);
    free(args);
    return NULL;
}

void* tml_thread_spawn(ThreadFunc func, void* arg) {
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args->func = func;
    args->arg = arg;
    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));
    pthread_create(thread, NULL, thread_wrapper, args);
    return thread;
}

void tml_thread_join(void* handle) {
    pthread_join(*(pthread_t*)handle, NULL);
    free(handle);
}

void tml_thread_yield(void) {
    sched_yield();
}

void tml_thread_sleep_ms(int64_t ms) {
    usleep((useconds_t)(ms * 1000));
}
#endif

// ============ CHANNEL (Go-style) ============

struct Channel {
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
};

Channel* tml_channel_new(int64_t capacity) {
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

void tml_channel_send(Channel* ch, void* data) {
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

void* tml_channel_recv(Channel* ch) {
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

int tml_channel_try_send(Channel* ch, void* data) {
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

void* tml_channel_try_recv(Channel* ch) {
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

void tml_channel_close(Channel* ch) {
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

int tml_channel_is_closed(Channel* ch) {
    return ch->closed;
}

void tml_channel_free(Channel* ch) {
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

struct Mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mtx;
#endif
};

Mutex* tml_mutex_new(void) {
    Mutex* m = (Mutex*)malloc(sizeof(Mutex));
#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
#else
    pthread_mutex_init(&m->mtx, NULL);
#endif
    return m;
}

void tml_mutex_lock(Mutex* m) {
#ifdef _WIN32
    EnterCriticalSection(&m->cs);
#else
    pthread_mutex_lock(&m->mtx);
#endif
}

void tml_mutex_unlock(Mutex* m) {
#ifdef _WIN32
    LeaveCriticalSection(&m->cs);
#else
    pthread_mutex_unlock(&m->mtx);
#endif
}

int tml_mutex_try_lock(Mutex* m) {
#ifdef _WIN32
    return TryEnterCriticalSection(&m->cs);
#else
    return pthread_mutex_trylock(&m->mtx) == 0;
#endif
}

void tml_mutex_free(Mutex* m) {
#ifdef _WIN32
    DeleteCriticalSection(&m->cs);
#else
    pthread_mutex_destroy(&m->mtx);
#endif
    free(m);
}

// ============ WAIT GROUP ============

struct WaitGroup {
    atomic_int_fast64_t count;
#ifdef _WIN32
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE cond;
#else
    pthread_mutex_t lock;
    pthread_cond_t cond;
#endif
};

WaitGroup* tml_waitgroup_new(void) {
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

void tml_waitgroup_add(WaitGroup* wg, int64_t delta) {
    atomic_fetch_add(&wg->count, delta);
}

void tml_waitgroup_done(WaitGroup* wg) {
    int64_t old = atomic_fetch_sub(&wg->count, 1);
    if (old == 1) {
#ifdef _WIN32
        WakeAllConditionVariable(&wg->cond);
#else
        pthread_cond_broadcast(&wg->cond);
#endif
    }
}

void tml_waitgroup_wait(WaitGroup* wg) {
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

void tml_waitgroup_free(WaitGroup* wg) {
#ifdef _WIN32
    DeleteCriticalSection(&wg->lock);
#else
    pthread_mutex_destroy(&wg->lock);
    pthread_cond_destroy(&wg->cond);
#endif
    free(wg);
}

// ============ ATOMIC COUNTER ============

struct AtomicCounter {
    atomic_int_fast64_t value;
};

AtomicCounter* tml_atomic_new(int64_t initial) {
    AtomicCounter* a = (AtomicCounter*)malloc(sizeof(AtomicCounter));
    atomic_init(&a->value, initial);
    return a;
}

int64_t tml_atomic_load(AtomicCounter* a) {
    return atomic_load(&a->value);
}

void tml_atomic_store(AtomicCounter* a, int64_t value) {
    atomic_store(&a->value, value);
}

int64_t tml_atomic_add(AtomicCounter* a, int64_t delta) {
    return atomic_fetch_add(&a->value, delta);
}

int64_t tml_atomic_sub(AtomicCounter* a, int64_t delta) {
    return atomic_fetch_sub(&a->value, delta);
}

void tml_atomic_free(AtomicCounter* a) {
    free(a);
}
