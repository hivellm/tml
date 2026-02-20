/**
 * @file sync.c
 * @brief TML Runtime - Synchronization Primitives
 *
 * Platform-specific implementation of synchronization primitives for the TML
 * sync library. These functions back the TML types: Mutex, RwLock, Condvar,
 * and Thread management.
 *
 * ## Components
 *
 * - **Mutex**: tml_mutex_* functions (SRWLOCK on Windows, pthread_mutex on Unix)
 * - **RwLock**: tml_rwlock_* functions (SRWLOCK on Windows, pthread_rwlock on Unix)
 * - **Condvar**: tml_condvar_* functions (CONDITION_VARIABLE on Windows, pthread_cond on Unix)
 * - **Thread**: tml_thread_* functions (CreateThread on Windows, pthread on Unix)
 *
 * ## Platform Support
 *
 * - **Windows**: Uses Win32 API (SRWLOCK, CONDITION_VARIABLE, CreateThread)
 * - **Unix/POSIX**: Uses pthreads (pthread_mutex, pthread_rwlock, pthread_cond, pthread_create)
 *
 * @note These functions use the `tml_` prefix to distinguish from the existing
 *       thread.c API which provides Go-style channels and different mutex API.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#endif

// ============================================================================
// Mutex Operations
// ============================================================================

#ifdef _WIN32

/**
 * @brief Initialize a mutex.
 * @param mutex Pointer to 64-byte RawMutex buffer
 * @return 0 on success
 */
TML_EXPORT int32_t tml_mutex_init(void* mutex) {
    SRWLOCK* lock = (SRWLOCK*)mutex;
    InitializeSRWLock(lock);
    return 0;
}

/**
 * @brief Destroy a mutex.
 * @param mutex Pointer to RawMutex buffer
 * @return 0 on success
 */
TML_EXPORT int32_t tml_mutex_destroy(void* mutex) {
    // SRWLOCK doesn't need explicit destruction on Windows
    (void)mutex;
    return 0;
}

/**
 * @brief Acquire a mutex (blocking).
 * @param mutex Pointer to RawMutex buffer
 * @return 0 on success
 */
TML_EXPORT int32_t tml_mutex_lock(void* mutex) {
    SRWLOCK* lock = (SRWLOCK*)mutex;
    AcquireSRWLockExclusive(lock);
    return 0;
}

/**
 * @brief Try to acquire a mutex (non-blocking).
 * @param mutex Pointer to RawMutex buffer
 * @return 0 if lock acquired, non-zero if lock was held
 */
TML_EXPORT int32_t tml_mutex_trylock(void* mutex) {
    SRWLOCK* lock = (SRWLOCK*)mutex;
    return TryAcquireSRWLockExclusive(lock) ? 0 : 1;
}

/**
 * @brief Release a mutex.
 * @param mutex Pointer to RawMutex buffer
 * @return 0 on success
 */
TML_EXPORT int32_t tml_mutex_unlock(void* mutex) {
    SRWLOCK* lock = (SRWLOCK*)mutex;
    ReleaseSRWLockExclusive(lock);
    return 0;
}

#else // Unix

TML_EXPORT int32_t tml_mutex_init(void* mutex) {
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    return pthread_mutex_init(mtx, NULL);
}

TML_EXPORT int32_t tml_mutex_destroy(void* mutex) {
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    return pthread_mutex_destroy(mtx);
}

TML_EXPORT int32_t tml_mutex_lock(void* mutex) {
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    return pthread_mutex_lock(mtx);
}

TML_EXPORT int32_t tml_mutex_trylock(void* mutex) {
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    return pthread_mutex_trylock(mtx);
}

TML_EXPORT int32_t tml_mutex_unlock(void* mutex) {
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    return pthread_mutex_unlock(mtx);
}

#endif // _WIN32

// ============================================================================
// RwLock Operations
// ============================================================================

#ifdef _WIN32

/**
 * @brief Initialize a read-write lock.
 */
TML_EXPORT int32_t tml_rwlock_init(void* rwlock) {
    SRWLOCK* lock = (SRWLOCK*)rwlock;
    InitializeSRWLock(lock);
    return 0;
}

/**
 * @brief Destroy a read-write lock.
 */
TML_EXPORT int32_t tml_rwlock_destroy(void* rwlock) {
    (void)rwlock;
    return 0;
}

/**
 * @brief Acquire read lock.
 */
TML_EXPORT int32_t tml_rwlock_read_lock(void* rwlock) {
    SRWLOCK* lock = (SRWLOCK*)rwlock;
    AcquireSRWLockShared(lock);
    return 0;
}

/**
 * @brief Try to acquire read lock.
 */
TML_EXPORT int32_t tml_rwlock_try_read_lock(void* rwlock) {
    SRWLOCK* lock = (SRWLOCK*)rwlock;
    return TryAcquireSRWLockShared(lock) ? 0 : 1;
}

/**
 * @brief Release read lock.
 */
TML_EXPORT int32_t tml_rwlock_read_unlock(void* rwlock) {
    SRWLOCK* lock = (SRWLOCK*)rwlock;
    ReleaseSRWLockShared(lock);
    return 0;
}

/**
 * @brief Acquire write lock.
 */
TML_EXPORT int32_t tml_rwlock_write_lock(void* rwlock) {
    SRWLOCK* lock = (SRWLOCK*)rwlock;
    AcquireSRWLockExclusive(lock);
    return 0;
}

/**
 * @brief Try to acquire write lock.
 */
TML_EXPORT int32_t tml_rwlock_try_write_lock(void* rwlock) {
    SRWLOCK* lock = (SRWLOCK*)rwlock;
    return TryAcquireSRWLockExclusive(lock) ? 0 : 1;
}

/**
 * @brief Release write lock.
 */
TML_EXPORT int32_t tml_rwlock_write_unlock(void* rwlock) {
    SRWLOCK* lock = (SRWLOCK*)rwlock;
    ReleaseSRWLockExclusive(lock);
    return 0;
}

#else // Unix

TML_EXPORT int32_t tml_rwlock_init(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_init(lock, NULL);
}

TML_EXPORT int32_t tml_rwlock_destroy(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_destroy(lock);
}

TML_EXPORT int32_t tml_rwlock_read_lock(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_rdlock(lock);
}

TML_EXPORT int32_t tml_rwlock_try_read_lock(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_tryrdlock(lock);
}

TML_EXPORT int32_t tml_rwlock_read_unlock(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_unlock(lock);
}

TML_EXPORT int32_t tml_rwlock_write_lock(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_wrlock(lock);
}

TML_EXPORT int32_t tml_rwlock_try_write_lock(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_trywrlock(lock);
}

TML_EXPORT int32_t tml_rwlock_write_unlock(void* rwlock) {
    pthread_rwlock_t* lock = (pthread_rwlock_t*)rwlock;
    return pthread_rwlock_unlock(lock);
}

#endif // _WIN32

// ============================================================================
// Condition Variable Operations
// ============================================================================

#ifdef _WIN32

/**
 * @brief Initialize a condition variable.
 */
TML_EXPORT int32_t tml_condvar_init(void* cvar) {
    CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)cvar;
    InitializeConditionVariable(cv);
    return 0;
}

/**
 * @brief Destroy a condition variable.
 */
TML_EXPORT int32_t tml_condvar_destroy(void* cvar) {
    // Windows CONDITION_VARIABLE doesn't need explicit destruction
    (void)cvar;
    return 0;
}

/**
 * @brief Wait on a condition variable.
 * @param cvar Pointer to RawCondvar buffer
 * @param mutex Pointer to RawMutex buffer (SRWLOCK)
 * @return 0 on success
 */
TML_EXPORT int32_t tml_condvar_wait(void* cvar, void* mutex) {
    CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)cvar;
    SRWLOCK* lock = (SRWLOCK*)mutex;
    SleepConditionVariableSRW(cv, lock, INFINITE, 0);
    return 0;
}

/**
 * @brief Wait on a condition variable with timeout.
 * @return 0 on signal, non-zero on timeout
 */
TML_EXPORT int32_t tml_condvar_wait_timeout_ms(void* cvar, void* mutex, uint64_t timeout_ms) {
    CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)cvar;
    SRWLOCK* lock = (SRWLOCK*)mutex;
    BOOL result = SleepConditionVariableSRW(cv, lock, (DWORD)timeout_ms, 0);
    return result ? 0 : 1; // 0 = signaled, 1 = timeout
}

/**
 * @brief Wake one waiting thread.
 */
TML_EXPORT int32_t tml_condvar_notify_one(void* cvar) {
    CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)cvar;
    WakeConditionVariable(cv);
    return 0;
}

/**
 * @brief Wake all waiting threads.
 */
TML_EXPORT int32_t tml_condvar_notify_all(void* cvar) {
    CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)cvar;
    WakeAllConditionVariable(cv);
    return 0;
}

#else // Unix

TML_EXPORT int32_t tml_condvar_init(void* cvar) {
    pthread_cond_t* cv = (pthread_cond_t*)cvar;
    return pthread_cond_init(cv, NULL);
}

TML_EXPORT int32_t tml_condvar_destroy(void* cvar) {
    pthread_cond_t* cv = (pthread_cond_t*)cvar;
    return pthread_cond_destroy(cv);
}

TML_EXPORT int32_t tml_condvar_wait(void* cvar, void* mutex) {
    pthread_cond_t* cv = (pthread_cond_t*)cvar;
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    return pthread_cond_wait(cv, mtx);
}

TML_EXPORT int32_t tml_condvar_wait_timeout_ms(void* cvar, void* mutex, uint64_t timeout_ms) {
    pthread_cond_t* cv = (pthread_cond_t*)cvar;
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    int result = pthread_cond_timedwait(cv, mtx, &ts);
    return (result == 0) ? 0 : 1; // 0 = signaled, 1 = timeout
}

TML_EXPORT int32_t tml_condvar_notify_one(void* cvar) {
    pthread_cond_t* cv = (pthread_cond_t*)cvar;
    return pthread_cond_signal(cv);
}

TML_EXPORT int32_t tml_condvar_notify_all(void* cvar) {
    pthread_cond_t* cv = (pthread_cond_t*)cvar;
    return pthread_cond_broadcast(cv);
}

#endif // _WIN32

// ============================================================================
// Thread Management
// ============================================================================

/**
 * @brief Thread trampoline data structure.
 */
typedef struct TmlThreadData {
    void* func_ptr; // Pointer to the thread function
    void* arg;      // Argument to pass to the function
} TmlThreadData;

#ifdef _WIN32

/**
 * @brief Windows thread entry point.
 */
static DWORD WINAPI tml_thread_entry(LPVOID lpParam) {
    TmlThreadData* data = (TmlThreadData*)lpParam;
    void (*func)(void*) = (void (*)(void*))data->func_ptr;
    void* arg = data->arg;
    free(data); // Free the trampoline data

    if (func) {
        func(arg);
    }
    return 0;
}

/**
 * @brief Spawns a new thread.
 * @param func_ptr Function pointer to execute
 * @param arg Argument to pass to the function
 * @param stack_size Desired stack size (0 for default)
 * @return Raw thread handle (HANDLE on Windows)
 */
TML_EXPORT uint64_t tml_thread_spawn(void* func_ptr, void* arg, uint64_t stack_size) {
    TmlThreadData* data = (TmlThreadData*)malloc(sizeof(TmlThreadData));
    if (!data)
        return 0;

    data->func_ptr = func_ptr;
    data->arg = arg;

    SIZE_T ss = (SIZE_T)stack_size;
    HANDLE thread = CreateThread(NULL,             // Security attributes
                                 ss,               // Stack size (0 = default)
                                 tml_thread_entry, // Thread function
                                 data,             // Argument
                                 0,                // Creation flags
                                 NULL              // Thread ID (not needed)
    );

    if (!thread) {
        free(data);
        return 0;
    }

    return (uint64_t)(uintptr_t)thread;
}

/**
 * @brief Joins (waits for) a thread to complete.
 * @param thread_handle Raw thread handle
 * @return 0 on success, non-zero on error
 */
TML_EXPORT int32_t tml_thread_join(uint64_t thread_handle) {
    HANDLE thread = (HANDLE)(uintptr_t)thread_handle;
    if (!thread)
        return -1;

    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    return (result == WAIT_OBJECT_0) ? 0 : -1;
}

/**
 * @brief Detaches a thread (allows it to run independently).
 * @param thread_handle Raw thread handle
 * @return 0 on success
 */
TML_EXPORT int32_t tml_thread_detach(uint64_t thread_handle) {
    HANDLE thread = (HANDLE)(uintptr_t)thread_handle;
    if (thread) {
        CloseHandle(thread);
    }
    return 0;
}

/**
 * @brief Gets the current thread's ID.
 * @return Thread ID as uint64_t
 */
TML_EXPORT uint64_t tml_thread_current_id(void) {
    return (uint64_t)GetCurrentThreadId();
}

/**
 * @brief Sleeps for the specified number of milliseconds.
 * @param milliseconds Duration to sleep
 */
TML_EXPORT void tml_thread_sleep_ms(uint64_t milliseconds) {
    Sleep((DWORD)milliseconds);
}

/**
 * @brief Yields the current thread's time slice.
 */
TML_EXPORT void tml_thread_yield(void) {
    SwitchToThread();
}

/**
 * @brief Returns the number of logical processors.
 * @return Number of CPUs, or 0 on error
 */
TML_EXPORT uint32_t tml_thread_available_parallelism(void) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (uint32_t)sysinfo.dwNumberOfProcessors;
}

#else // Unix

static void* tml_thread_entry(void* param) {
    TmlThreadData* data = (TmlThreadData*)param;
    void (*func)(void*) = (void (*)(void*))data->func_ptr;
    void* arg = data->arg;
    free(data);

    if (func) {
        func(arg);
    }
    return NULL;
}

TML_EXPORT uint64_t tml_thread_spawn(void* func_ptr, void* arg, uint64_t stack_size) {
    TmlThreadData* data = (TmlThreadData*)malloc(sizeof(TmlThreadData));
    if (!data)
        return 0;

    data->func_ptr = func_ptr;
    data->arg = arg;

    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    if (stack_size > 0) {
        pthread_attr_setstacksize(&attr, (size_t)stack_size);
    }

    int result = pthread_create(&thread, &attr, tml_thread_entry, data);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        free(data);
        return 0;
    }

    return (uint64_t)thread;
}

TML_EXPORT int32_t tml_thread_join(uint64_t thread_handle) {
    pthread_t thread = (pthread_t)thread_handle;
    return pthread_join(thread, NULL);
}

TML_EXPORT int32_t tml_thread_detach(uint64_t thread_handle) {
    pthread_t thread = (pthread_t)thread_handle;
    return pthread_detach(thread);
}

TML_EXPORT uint64_t tml_thread_current_id(void) {
    return (uint64_t)pthread_self();
}

TML_EXPORT void tml_thread_sleep_ms(uint64_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

TML_EXPORT void tml_thread_yield(void) {
    sched_yield();
}

TML_EXPORT uint32_t tml_thread_available_parallelism(void) {
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return (nprocs > 0) ? (uint32_t)nprocs : 1;
}

#endif // _WIN32

// ============================================================================
// Atomic Operations
// ============================================================================

/**
 * @brief Atomically add to an I32 and return the previous value.
 * @param ptr Pointer to the I32 variable
 * @param val Value to add
 * @return Previous value before addition
 */
TML_EXPORT int32_t atomic_fetch_add_i32(int32_t* ptr, int32_t val) {
#ifdef _WIN32
    return (int32_t)InterlockedExchangeAdd((LONG*)ptr, (LONG)val);
#else
    return __sync_fetch_and_add(ptr, val);
#endif
}

/**
 * @brief Atomically subtract from an I32 and return the previous value.
 * @param ptr Pointer to the I32 variable
 * @param val Value to subtract
 * @return Previous value before subtraction
 */
TML_EXPORT int32_t atomic_fetch_sub_i32(int32_t* ptr, int32_t val) {
#ifdef _WIN32
    return (int32_t)InterlockedExchangeAdd((LONG*)ptr, -(LONG)val);
#else
    return __sync_fetch_and_sub(ptr, val);
#endif
}

/**
 * @brief Atomically load an I32 value.
 * @param ptr Pointer to the I32 variable
 * @return Current value
 */
TML_EXPORT int32_t atomic_load_i32(const int32_t* ptr) {
#ifdef _WIN32
    return (int32_t)InterlockedCompareExchange((LONG*)ptr, 0, 0);
#else
    return __sync_fetch_and_add((int32_t*)ptr, 0);
#endif
}

/**
 * @brief Atomically store an I32 value.
 * @param ptr Pointer to the I32 variable
 * @param val Value to store
 */
TML_EXPORT void atomic_store_i32(int32_t* ptr, int32_t val) {
#ifdef _WIN32
    InterlockedExchange((LONG*)ptr, (LONG)val);
#else
    __sync_lock_test_and_set(ptr, val);
#endif
}

/**
 * @brief Atomically compare and swap an I32 value.
 * @param ptr Pointer to the I32 variable
 * @param expected Expected current value
 * @param desired Value to set if current == expected
 * @return Previous value (equal to expected if swap succeeded)
 */
TML_EXPORT int32_t atomic_compare_exchange_i32(int32_t* ptr, int32_t expected, int32_t desired) {
#ifdef _WIN32
    return (int32_t)InterlockedCompareExchange((LONG*)ptr, (LONG)desired, (LONG)expected);
#else
    return __sync_val_compare_and_swap(ptr, expected, desired);
#endif
}

/**
 * @brief Atomically swap an I32 value.
 * @param ptr Pointer to the I32 variable
 * @param val New value
 * @return Previous value
 */
TML_EXPORT int32_t atomic_swap_i32(int32_t* ptr, int32_t val) {
#ifdef _WIN32
    return (int32_t)InterlockedExchange((LONG*)ptr, (LONG)val);
#else
    return __sync_lock_test_and_set(ptr, val);
#endif
}

// 64-bit atomic operations — REMOVED (Phase 41)
// No declare in runtime.cpp, 0 TML callers. Only i32 atomics are used.
// Codegen uses LLVM atomicrmw/cmpxchg instructions for typed atomics.

// Atomic fence operations

/**
 * @brief Full memory barrier (acquire + release).
 */
TML_EXPORT void atomic_fence(void) {
#ifdef _WIN32
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

/**
 * @brief Acquire barrier (prevents reordering of loads).
 */
TML_EXPORT void atomic_fence_acquire(void) {
#ifdef _WIN32
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

/**
 * @brief Release barrier (prevents reordering of stores).
 */
TML_EXPORT void atomic_fence_release(void) {
#ifdef _WIN32
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}
