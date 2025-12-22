# TML v1.0 — Runtime System

## 1. Overview

The TML runtime provides essential services for program execution:
- Memory allocation and deallocation
- Reference counting for Shared/Sync
- Panic handling and unwinding
- Thread management
- Global initialization

## 2. Runtime Initialization

### 2.1 Entry Point

```cpp
// Platform entry (provided by runtime)
extern "C" int main(int argc, char** argv) {
    // 1. Initialize runtime
    tml_runtime_init();

    // 2. Set up panic handler
    tml_set_panic_hook(default_panic_hook);

    // 3. Initialize global allocator
    tml_allocator_init();

    // 4. Call TML main with args
    TmlArgs args = tml_args_from_c(argc, argv);
    int result = tml_main(args);

    // 5. Run global destructors
    tml_run_global_dtors();

    // 6. Cleanup runtime
    tml_runtime_shutdown();

    return result;
}
```

### 2.2 Static Initialization

```cpp
// Module initializers run in dependency order
struct ModuleInit {
    const char* name;
    void (*init_fn)(void);
    void (*fini_fn)(void);
    const char** deps;
    size_t dep_count;
};

// Global registry
extern ModuleInit __tml_module_inits[] __attribute__((section(".tml_init")));

void tml_run_module_inits() {
    // Topological sort by dependencies
    // Call init_fn for each module
}
```

## 3. Memory Allocator

### 3.1 Allocator Interface

```cpp
// Core allocation functions
extern "C" {
    // Allocate memory with given size and alignment
    void* tml_alloc(size_t size, size_t align);

    // Reallocate memory
    void* tml_realloc(void* ptr, size_t old_size, size_t old_align,
                      size_t new_size, size_t new_align);

    // Deallocate memory
    void tml_dealloc(void* ptr, size_t size, size_t align);

    // Allocate zeroed memory
    void* tml_alloc_zeroed(size_t size, size_t align);
}
```

### 3.2 Default Allocator

```cpp
// Default: system allocator wrapper
void* tml_alloc(size_t size, size_t align) {
    if (size == 0) return nullptr;

    void* ptr;
    #if defined(_WIN32)
        ptr = _aligned_malloc(size, align);
    #else
        if (posix_memalign(&ptr, align, size) != 0) {
            ptr = nullptr;
        }
    #endif

    if (ptr == nullptr) {
        tml_alloc_error(size, align);
    }

    return ptr;
}

void tml_dealloc(void* ptr, size_t size, size_t align) {
    if (ptr == nullptr) return;

    #if defined(_WIN32)
        _aligned_free(ptr);
    #else
        free(ptr);
    #endif
}
```

### 3.3 Allocation Error Handling

```cpp
// Called when allocation fails
[[noreturn]]
void tml_alloc_error(size_t size, size_t align) {
    // Try to run OOM hook if set
    if (tml_oom_hook != nullptr) {
        tml_oom_hook(size, align);
    }

    // Abort with message
    tml_abort("memory allocation failed");
}

// User can set custom OOM handler
void tml_set_oom_hook(void (*hook)(size_t, size_t));
```

### 3.4 Custom Allocators

```tml
// TML interface for custom allocators
public behavior Allocator {
    lowlevel func alloc(this, size: U64, align: U64) -> *mut U8
    lowlevel func dealloc(this, ptr: *mut U8, size: U64, align: U64)
    lowlevel func realloc(this, ptr: *mut U8, old_size: U64, old_align: U64,
                        new_size: U64, new_align: U64) -> *mut U8
}

// Global allocator (can be overridden)
@global_allocator
var GLOBAL: SystemAllocator = SystemAllocator.new()

// Arena allocator example
public type Arena {
    buffer: *mut U8,
    capacity: U64,
    offset: U64,
}

extend Arena with Allocator {
    lowlevel func alloc(this, size: U64, align: U64) -> *mut U8 {
        let aligned_offset: U64 = align_up(this.offset, align)
        if aligned_offset + size > this.capacity {
            return null
        }
        let ptr: ptr U8 = this.buffer.add(aligned_offset)
        this.offset = aligned_offset + size
        return ptr
    }

    lowlevel func dealloc(this, ptr: *mut U8, size: U64, align: U64) {
        // Arena doesn't free individual allocations
    }
}
```

## 4. Reference Counting

### 4.1 Shared (Single-threaded)

```cpp
struct RcInner {
    size_t strong_count;
    size_t weak_count;
    // value follows
};

extern "C" void tml_rc_inc(void* ptr) {
    RcInner* inner = (RcInner*)ptr;
    inner->strong_count++;
}

extern "C" bool tml_rc_dec(void* ptr, void (*drop_fn)(void*), size_t size, size_t align) {
    RcInner* inner = (RcInner*)ptr;
    inner->strong_count--;

    if (inner->strong_count == 0) {
        // Drop the value
        void* value = (char*)ptr + sizeof(RcInner);
        if (drop_fn) drop_fn(value);

        if (inner->weak_count == 0) {
            // Deallocate
            tml_dealloc(ptr, sizeof(RcInner) + size, align);
        }
        return true;
    }
    return false;
}
```

### 4.2 Sync (Thread-safe)

```cpp
struct SyncInner {
    std::atomic<size_t> strong_count;
    std::atomic<size_t> weak_count;
    // value follows
};

extern "C" void tml_sync_inc(void* ptr) {
    SyncInner* inner = (SyncInner*)ptr;
    inner->strong_count.fetch_add(1, std::memory_order_relaxed);
}

extern "C" bool tml_sync_dec(void* ptr, void (*drop_fn)(void*), size_t size, size_t align) {
    SyncInner* inner = (SyncInner*)ptr;

    if (inner->strong_count.fetch_sub(1, std::memory_order_release) == 1) {
        std::atomic_thread_fence(std::memory_order_acquire);

        // Drop the value
        void* value = (char*)ptr + sizeof(ArcInner);
        if (drop_fn) drop_fn(value);

        if (inner->weak_count.load(std::memory_order_relaxed) == 0) {
            tml_dealloc(ptr, sizeof(ArcInner) + size, align);
        }
        return true;
    }
    return false;
}
```

### 4.3 Weak References

```cpp
extern "C" void* tml_weak_upgrade(void* weak_ptr) {
    SyncInner* inner = (SyncInner*)weak_ptr;

    while (true) {
        size_t strong = inner->strong_count.load(std::memory_order_relaxed);
        if (strong == 0) {
            return nullptr;  // Already dropped
        }

        if (inner->strong_count.compare_exchange_weak(
                strong, strong + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return weak_ptr;
        }
    }
}
```

## 5. Panic Handling

### 5.1 Panic Runtime

```cpp
struct PanicInfo {
    const char* message;
    size_t message_len;
    const char* file;
    size_t line;
    size_t column;
};

// Current panic hook
static void (*tml_panic_hook)(const PanicInfo*) = nullptr;

extern "C" [[noreturn]] void tml_panic(const char* msg, size_t len) {
    PanicInfo info = {
        .message = msg,
        .message_len = len,
        .file = __builtin_FILE(),
        .line = __builtin_LINE(),
        .column = 0
    };

    tml_panic_with_info(&info);
}

[[noreturn]] void tml_panic_with_info(const PanicInfo* info) {
    // Call panic hook if set
    if (tml_panic_hook) {
        tml_panic_hook(info);
    }

    // Print default message
    fprintf(stderr, "panic at %s:%zu: %.*s\n",
            info->file, info->line,
            (int)info->message_len, info->message);

    // Begin unwinding
    tml_begin_unwind(info);
}
```

### 5.2 Stack Unwinding

```cpp
// Platform-specific unwinding

#if defined(__linux__) || defined(__APPLE__)
#include <unwind.h>

struct TmlException {
    _Unwind_Exception header;
    PanicInfo info;
};

[[noreturn]] void tml_begin_unwind(const PanicInfo* info) {
    TmlException* ex = (TmlException*)malloc(sizeof(TmlException));
    ex->header.exception_class = TML_EXCEPTION_CLASS;
    ex->info = *info;

    _Unwind_RaiseException(&ex->header);

    // If we get here, unwinding failed
    tml_abort("unwinding failed");
}

#elif defined(_WIN32)
#include <windows.h>

[[noreturn]] void tml_begin_unwind(const PanicInfo* info) {
    ULONG_PTR args[2] = {
        (ULONG_PTR)info->message,
        (ULONG_PTR)info->message_len
    };

    RaiseException(TML_EXCEPTION_CODE, 0, 2, args);

    // If we get here, unwinding failed
    tml_abort("unwinding failed");
}
#endif
```

### 5.3 Panic Hook

```tml
import std.panic

// Set custom panic handler
public func main() {
    panic.set_hook(do(info) {
        // Log to file
        log_error("PANIC: " + info.message())

        // Print backtrace
        let bt: Backtrace = Backtrace.capture()
        eprintln(bt.to_string())
    })
}
```

## 6. Thread Support

### 6.1 Thread Creation

```cpp
struct TmlThread {
    #if defined(_WIN32)
        HANDLE handle;
    #else
        pthread_t handle;
    #endif
    void* result;
    bool panicked;
};

extern "C" TmlThread* tml_thread_spawn(
    void* (*start_routine)(void*),
    void* arg,
    size_t stack_size
) {
    TmlThread* thread = (TmlThread*)tml_alloc(sizeof(TmlThread), alignof(TmlThread));

    #if defined(_WIN32)
        thread->handle = CreateThread(
            nullptr,
            stack_size,
            (LPTHREAD_START_ROUTINE)start_routine,
            arg,
            0,
            nullptr
        );
    #else
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        if (stack_size > 0) {
            pthread_attr_setstacksize(&attr, stack_size);
        }
        pthread_create(&thread->handle, &attr, start_routine, arg);
        pthread_attr_destroy(&attr);
    #endif

    return thread;
}
```

### 6.2 Thread-Local Storage

```cpp
// Thread-local key management
#if defined(_WIN32)
    typedef DWORD TmlTlsKey;
    #define tml_tls_create() TlsAlloc()
    #define tml_tls_get(key) TlsGetValue(key)
    #define tml_tls_set(key, val) TlsSetValue(key, val)
#else
    typedef pthread_key_t TmlTlsKey;
    #define tml_tls_create() ({ pthread_key_t k; pthread_key_create(&k, nullptr); k; })
    #define tml_tls_get(key) pthread_getspecific(key)
    #define tml_tls_set(key, val) pthread_setspecific(key, val)
#endif

// TLS destructor registration
void tml_tls_register_dtor(TmlTlsKey key, void (*dtor)(void*));
```

### 6.3 Thread Synchronization

```cpp
// Mutex
struct TmlMutex {
    #if defined(_WIN32)
        SRWLOCK lock;
    #else
        pthread_mutex_t lock;
    #endif
};

extern "C" void tml_mutex_lock(TmlMutex* mutex);
extern "C" bool tml_mutex_try_lock(TmlMutex* mutex);
extern "C" void tml_mutex_unlock(TmlMutex* mutex);

// Condition variable
struct TmlCondVar {
    #if defined(_WIN32)
        CONDITION_VARIABLE cond;
    #else
        pthread_cond_t cond;
    #endif
};

extern "C" void tml_condvar_wait(TmlCondVar* cond, TmlMutex* mutex);
extern "C" void tml_condvar_notify_one(TmlCondVar* cond);
extern "C" void tml_condvar_notify_all(TmlCondVar* cond);
```

## 7. I/O Runtime

### 7.1 Standard Streams

```cpp
// Standard stream handles
extern "C" {
    void* tml_stdin();
    void* tml_stdout();
    void* tml_stderr();
}

// Buffered I/O
extern "C" {
    size_t tml_write(void* handle, const uint8_t* buf, size_t len);
    size_t tml_read(void* handle, uint8_t* buf, size_t len);
    void tml_flush(void* handle);
}
```

### 7.2 Print Functions

```cpp
extern "C" void tml_print(const char* s, size_t len) {
    tml_write(tml_stdout(), (const uint8_t*)s, len);
}

extern "C" void tml_println(const char* s, size_t len) {
    tml_print(s, len);
    tml_print("\n", 1);
}

extern "C" void tml_eprint(const char* s, size_t len) {
    tml_write(tml_stderr(), (const uint8_t*)s, len);
}
```

## 8. Intrinsics

### 8.1 Compiler Intrinsics

```cpp
// Memory operations
extern "C" void tml_memcpy(void* dst, const void* src, size_t n);
extern "C" void tml_memmove(void* dst, const void* src, size_t n);
extern "C" void tml_memset(void* dst, int c, size_t n);
extern "C" int tml_memcmp(const void* a, const void* b, size_t n);

// Overflow-checked arithmetic
extern "C" bool tml_add_overflow_i32(int32_t a, int32_t b, int32_t* result);
extern "C" bool tml_sub_overflow_i32(int32_t a, int32_t b, int32_t* result);
extern "C" bool tml_mul_overflow_i32(int32_t a, int32_t b, int32_t* result);

// Bit manipulation
extern "C" uint32_t tml_ctlz_u32(uint32_t x);  // count leading zeros
extern "C" uint32_t tml_cttz_u32(uint32_t x);  // count trailing zeros
extern "C" uint32_t tml_popcnt_u32(uint32_t x); // population count
extern "C" uint32_t tml_bswap_u32(uint32_t x);  // byte swap
```

### 8.2 Math Intrinsics

```cpp
// Fast math (may use hardware instructions)
extern "C" float tml_sqrtf(float x);
extern "C" double tml_sqrt(double x);
extern "C" float tml_sinf(float x);
extern "C" double tml_sin(double x);
extern "C" float tml_cosf(float x);
extern "C" double tml_cos(double x);
extern "C" float tml_floorf(float x);
extern "C" float tml_ceilf(float x);
extern "C" float tml_truncf(float x);
extern "C" float tml_roundf(float x);
```

## 9. Debug Support

### 9.1 Backtrace

```cpp
struct TmlBacktrace {
    void** frames;
    size_t count;
    size_t capacity;
};

extern "C" TmlBacktrace* tml_backtrace_capture() {
    TmlBacktrace* bt = (TmlBacktrace*)tml_alloc(sizeof(TmlBacktrace), alignof(TmlBacktrace));
    bt->capacity = 64;
    bt->frames = (void**)tml_alloc(bt->capacity * sizeof(void*), alignof(void*));

    #if defined(__linux__) || defined(__APPLE__)
        bt->count = backtrace(bt->frames, bt->capacity);
    #elif defined(_WIN32)
        bt->count = CaptureStackBackTrace(0, bt->capacity, bt->frames, nullptr);
    #endif

    return bt;
}

extern "C" void tml_backtrace_symbolize(TmlBacktrace* bt, TmlBacktraceSymbols* symbols);
```

### 9.2 Debug Assertions

```cpp
extern "C" void tml_debug_assert(bool cond, const char* msg, size_t len,
                                  const char* file, size_t line) {
    #ifdef TML_DEBUG
        if (!cond) {
            fprintf(stderr, "assertion failed at %s:%zu: %.*s\n",
                    file, line, (int)len, msg);
            tml_abort("assertion failed");
        }
    #endif
}
```

## 10. Platform Abstraction

### 10.1 OS Functions

```cpp
// Time
extern "C" uint64_t tml_time_now_nanos();
extern "C" void tml_sleep_nanos(uint64_t nanos);

// Environment
extern "C" const char* tml_getenv(const char* name);
extern "C" void tml_setenv(const char* name, const char* value);

// File system
extern "C" int tml_open(const char* path, int flags, int mode);
extern "C" ssize_t tml_read_fd(int fd, void* buf, size_t count);
extern "C" ssize_t tml_write_fd(int fd, const void* buf, size_t count);
extern "C" int tml_close(int fd);

// Process
extern "C" void tml_exit(int code);
extern "C" [[noreturn]] void tml_abort(const char* msg);
extern "C" int tml_getpid();
```

### 10.2 Random Numbers

```cpp
// Cryptographically secure random
extern "C" void tml_random_bytes(uint8_t* buf, size_t len) {
    #if defined(__linux__)
        getrandom(buf, len, 0);
    #elif defined(__APPLE__)
        arc4random_buf(buf, len);
    #elif defined(_WIN32)
        BCryptGenRandom(nullptr, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    #endif
}
```

## 11. Build Configuration

### 11.1 Runtime Library Variants

| Variant | Use Case |
|---------|----------|
| `libtml_rt.a` | Static release |
| `libtml_rt_debug.a` | Static debug |
| `libtml_rt.so/.dll` | Dynamic release |
| `libtml_rt_debug.so/.dll` | Dynamic debug |

### 11.2 Configuration Flags

```cpp
// Compile-time configuration
#define TML_DEBUG           // Enable debug assertions
#define TML_SANITIZE        // Enable sanitizer hooks
#define TML_PROFILE         // Enable profiling hooks
#define TML_CUSTOM_ALLOC    // Use custom allocator

// Runtime configuration (via environment)
TML_BACKTRACE=1             // Enable backtraces
TML_LOG=debug               // Log level
TML_THREADS=4               // Thread pool size
```

## 12. Runtime Size

### 12.1 Minimal Runtime

For embedded/minimal builds:

```cpp
// Minimal runtime (~10KB)
- tml_alloc / tml_dealloc (system malloc wrapper)
- tml_panic (simple abort)
- tml_print / tml_println
- Basic intrinsics
```

### 12.2 Full Runtime

Full-featured runtime:

```cpp
// Full runtime (~200KB)
- Complete allocator
- Reference counting (Shared/Sync)
- Full panic + unwinding
- Thread support
- I/O runtime
- Debug support
- Platform abstraction
```

---

*Previous: [18-ABI.md](./18-ABI.md)*
*Next: [20-STDLIB.md](./20-STDLIB.md) — Standard Library*
