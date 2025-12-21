# TML v1.0 — System Call Interface

## 1. Overview

TML provides direct access to operating system system calls for:
- Building the standard library without external dependencies
- Bare-metal and OS development
- Maximum control and performance
- Custom platform abstraction layers

## 2. Raw System Calls

### 2.1 Linux System Calls

```tml
@cfg(target_os = "linux")
module sys.linux

/// Raw syscall with variable arguments
public lowlevel func syscall0(nr: U64) -> I64
public lowlevel func syscall1(nr: U64, a1: U64) -> I64
public lowlevel func syscall2(nr: U64, a1: U64, a2: U64) -> I64
public lowlevel func syscall3(nr: U64, a1: U64, a2: U64, a3: U64) -> I64
public lowlevel func syscall4(nr: U64, a1: U64, a2: U64, a3: U64, a4: U64) -> I64
public lowlevel func syscall5(nr: U64, a1: U64, a2: U64, a3: U64, a4: U64, a5: U64) -> I64
public lowlevel func syscall6(nr: U64, a1: U64, a2: U64, a3: U64, a4: U64, a5: U64, a6: U64) -> I64

// Syscall numbers (x86_64)
public const SYS_READ: U64 = 0
public const SYS_WRITE: U64 = 1
public const SYS_OPEN: U64 = 2
public const SYS_CLOSE: U64 = 3
public const SYS_STAT: U64 = 4
public const SYS_FSTAT: U64 = 5
public const SYS_LSTAT: U64 = 6
public const SYS_POLL: U64 = 7
public const SYS_LSEEK: U64 = 8
public const SYS_MMAP: U64 = 9
public const SYS_MPROTECT: U64 = 10
public const SYS_MUNMAP: U64 = 11
public const SYS_BRK: U64 = 12
public const SYS_IOCTL: U64 = 16
public const SYS_PIPE: U64 = 22
public const SYS_SELECT: U64 = 23
public const SYS_DUP: U64 = 32
public const SYS_DUP2: U64 = 33
public const SYS_NANOSLEEP: U64 = 35
public const SYS_GETPID: U64 = 39
public const SYS_SOCKET: U64 = 41
public const SYS_CONNECT: U64 = 42
public const SYS_ACCEPT: U64 = 43
public const SYS_SENDTO: U64 = 44
public const SYS_RECVFROM: U64 = 45
public const SYS_BIND: U64 = 49
public const SYS_LISTEN: U64 = 50
public const SYS_CLONE: U64 = 56
public const SYS_FORK: U64 = 57
public const SYS_EXECVE: U64 = 59
public const SYS_EXIT: U64 = 60
public const SYS_WAIT4: U64 = 61
public const SYS_KILL: U64 = 62
public const SYS_FCNTL: U64 = 72
public const SYS_FLOCK: U64 = 73
public const SYS_FSYNC: U64 = 74
public const SYS_GETCWD: U64 = 79
public const SYS_CHDIR: U64 = 80
public const SYS_MKDIR: U64 = 83
public const SYS_RMDIR: U64 = 84
public const SYS_UNLINK: U64 = 87
public const SYS_CHMOD: U64 = 90
public const SYS_CHOWN: U64 = 92
public const SYS_GETUID: U64 = 102
public const SYS_GETGID: U64 = 104
public const SYS_GETEUID: U64 = 107
public const SYS_GETEGID: U64 = 108
public const SYS_GETPPID: U64 = 110
public const SYS_SETPGID: U64 = 109
public const SYS_SETSID: U64 = 112
public const SYS_GETGROUPS: U64 = 115
public const SYS_SETGROUPS: U64 = 116
public const SYS_GETRESUID: U64 = 118
public const SYS_GETRESGID: U64 = 120
public const SYS_UNAME: U64 = 63
public const SYS_GETTIMEOFDAY: U64 = 96
public const SYS_GETRLIMIT: U64 = 97
public const SYS_GETRUSAGE: U64 = 98
public const SYS_CLOCK_GETTIME: U64 = 228
public const SYS_CLOCK_NANOSLEEP: U64 = 230
public const SYS_EPOLL_CREATE: U64 = 213
public const SYS_EPOLL_CTL: U64 = 233
public const SYS_EPOLL_WAIT: U64 = 232
public const SYS_EPOLL_CREATE1: U64 = 291
public const SYS_EVENTFD2: U64 = 290
public const SYS_SIGNALFD4: U64 = 289
public const SYS_TIMERFD_CREATE: U64 = 283
public const SYS_TIMERFD_SETTIME: U64 = 286
public const SYS_GETRANDOM: U64 = 318
public const SYS_MEMFD_CREATE: U64 = 319
public const SYS_OPENAT: U64 = 257
public const SYS_MKDIRAT: U64 = 258
public const SYS_FSTATAT: U64 = 262
public const SYS_UNLINKAT: U64 = 263
public const SYS_RENAMEAT: U64 = 264
public const SYS_FUTEX: U64 = 202
public const SYS_EXIT_GROUP: U64 = 231
```

### 2.2 Typed Linux Wrappers

```tml
@cfg(target_os = "linux")
module sys.linux.io

/// Read from file descriptor
public lowlevel func read(fd: I32, buf: *mut U8, count: U64) -> I64 {
    return syscall3(SYS_READ, fd as U64, buf as U64, count)
}

/// Write to file descriptor
public lowlevel func write(fd: I32, buf: *const U8, count: U64) -> I64 {
    return syscall3(SYS_WRITE, fd as U64, buf as U64, count)
}

/// Open file
public lowlevel func open(path: *const U8, flags: I32, mode: U32) -> I32 {
    return syscall3(SYS_OPEN, path as U64, flags as U64, mode as U64) as I32
}

/// Close file descriptor
public lowlevel func close(fd: I32) -> I32 {
    return syscall1(SYS_CLOSE, fd as U64) as I32
}

/// Seek
public lowlevel func lseek(fd: I32, offset: I64, whence: I32) -> I64 {
    return syscall3(SYS_LSEEK, fd as U64, offset as U64, whence as U64)
}

// Open flags
public const O_RDONLY: I32 = 0
public const O_WRONLY: I32 = 1
public const O_RDWR: I32 = 2
public const O_CREAT: I32 = 0x40
public const O_EXCL: I32 = 0x80
public const O_TRUNC: I32 = 0x200
public const O_APPEND: I32 = 0x400
public const O_NONBLOCK: I32 = 0x800
public const O_CLOEXEC: I32 = 0x80000

// Seek origins
public const SEEK_SET: I32 = 0
public const SEEK_CUR: I32 = 1
public const SEEK_END: I32 = 2
```

### 2.3 Linux Memory Management

```tml
@cfg(target_os = "linux")
module sys.linux.mem

/// Memory map
public lowlevel func mmap(
    addr: *mut Void,
    length: U64,
    prot: I32,
    flags: I32,
    fd: I32,
    offset: I64
) -> *mut Void {
    return syscall6(
        SYS_MMAP,
        addr as U64,
        length,
        prot as U64,
        flags as U64,
        fd as U64,
        offset as U64
    ) as *mut Void
}

/// Memory unmap
public lowlevel func munmap(addr: *mut Void, length: U64) -> I32 {
    return syscall2(SYS_MUNMAP, addr as U64, length) as I32
}

/// Memory protect
public lowlevel func mprotect(addr: *mut Void, length: U64, prot: I32) -> I32 {
    return syscall3(SYS_MPROTECT, addr as U64, length, prot as U64) as I32
}

/// Program break
public lowlevel func brk(addr: *mut Void) -> *mut Void {
    return syscall1(SYS_BRK, addr as U64) as *mut Void
}

// Protection flags
public const PROT_NONE: I32 = 0
public const PROT_READ: I32 = 1
public const PROT_WRITE: I32 = 2
public const PROT_EXEC: I32 = 4

// Map flags
public const MAP_SHARED: I32 = 0x01
public const MAP_PRIVATE: I32 = 0x02
public const MAP_FIXED: I32 = 0x10
public const MAP_ANONYMOUS: I32 = 0x20
public const MAP_NORESERVE: I32 = 0x4000
public const MAP_POPULATE: I32 = 0x8000
public const MAP_HUGETLB: I32 = 0x40000

public const MAP_FAILED: *mut Void = -1 as *mut Void
```

### 2.4 Linux Process Management

```tml
@cfg(target_os = "linux")
module sys.linux.process

/// Exit process
public lowlevel func exit(code: I32) -> ! {
    syscall1(SYS_EXIT, code as U64)
    unreachable()
}

/// Exit thread group
public lowlevel func exit_group(code: I32) -> ! {
    syscall1(SYS_EXIT_GROUP, code as U64)
    unreachable()
}

/// Get process ID
public lowlevel func getpid() -> I32 {
    return syscall0(SYS_GETPID) as I32
}

/// Get parent process ID
public lowlevel func getppid() -> I32 {
    return syscall0(SYS_GETPPID) as I32
}

/// Fork process
public lowlevel func fork() -> I32 {
    return syscall0(SYS_FORK) as I32
}

/// Clone (create thread/process)
public lowlevel func clone(
    flags: U64,
    stack: *mut Void,
    parent_tid: *mut I32,
    child_tid: *mut I32,
    tls: U64
) -> I64 {
    return syscall5(SYS_CLONE, flags, stack as U64, parent_tid as U64, child_tid as U64, tls)
}

/// Execute program
public lowlevel func execve(
    path: *const U8,
    argv: *const *const U8,
    envp: *const *const U8
) -> I32 {
    return syscall3(SYS_EXECVE, path as U64, argv as U64, envp as U64) as I32
}

/// Wait for child process
public lowlevel func wait4(
    pid: I32,
    status: *mut I32,
    options: I32,
    rusage: *mut Void
) -> I32 {
    return syscall4(SYS_WAIT4, pid as U64, status as U64, options as U64, rusage as U64) as I32
}

/// Send signal
public lowlevel func kill(pid: I32, sig: I32) -> I32 {
    return syscall2(SYS_KILL, pid as U64, sig as U64) as I32
}

// Clone flags
public const CLONE_VM: U64 = 0x00000100
public const CLONE_FS: U64 = 0x00000200
public const CLONE_FILES: U64 = 0x00000400
public const CLONE_SIGHAND: U64 = 0x00000800
public const CLONE_THREAD: U64 = 0x00010000
public const CLONE_SETTLS: U64 = 0x00080000
public const CLONE_PARENT_SETTID: U64 = 0x00100000
public const CLONE_CHILD_CLEARTID: U64 = 0x00200000
```

### 2.5 Linux Futex

```tml
@cfg(target_os = "linux")
module sys.linux.futex

public lowlevel func futex(
    uaddr: *mut U32,
    futex_op: I32,
    val: U32,
    timeout: *const Timespec,
    uaddr2: *mut U32,
    val3: U32
) -> I64 {
    return syscall6(
        SYS_FUTEX,
        uaddr as U64,
        futex_op as U64,
        val as U64,
        timeout as U64,
        uaddr2 as U64,
        val3 as U64
    )
}

/// Wait if *uaddr == val
public lowlevel func futex_wait(uaddr: *mut U32, val: U32, timeout: Maybe[ref Timespec]) -> I64 {
    let timeout_ptr = when timeout {
        Just(t) -> t as *const Timespec,
        Nothing -> null,
    }
    return futex(uaddr, FUTEX_WAIT, val, timeout_ptr, null, 0)
}

/// Wake up to n waiters
public lowlevel func futex_wake(uaddr: *mut U32, n: U32) -> I64 {
    return futex(uaddr, FUTEX_WAKE, n, null, null, 0)
}

public const FUTEX_WAIT: I32 = 0
public const FUTEX_WAKE: I32 = 1
public const FUTEX_PRIVATE_FLAG: I32 = 128
public const FUTEX_WAIT_PRIVATE: I32 = FUTEX_WAIT | FUTEX_PRIVATE_FLAG
public const FUTEX_WAKE_PRIVATE: I32 = FUTEX_WAKE | FUTEX_PRIVATE_FLAG

public type Timespec {
    tv_sec: I64,
    tv_nsec: I64,
}
```

## 3. Windows System Calls

### 3.1 Windows NT Syscalls

```tml
@cfg(target_os = "windows")
module sys.windows

/// NT system call
public lowlevel func syscall(nr: U32, ...) -> I32

// Note: Windows syscall numbers vary between versions
// Typically access through ntdll.dll
```

### 3.2 Windows Kernel32 Functions

```tml
@cfg(target_os = "windows")
module sys.windows.kernel32

// These are FFI declarations, not raw syscalls
// Windows encourages using documented APIs

extern "system" from "kernel32" {
    // File operations
    func CreateFileW(
        lpFileName: *const U16,
        dwDesiredAccess: U32,
        dwShareMode: U32,
        lpSecurityAttributes: *mut Void,
        dwCreationDisposition: U32,
        dwFlagsAndAttributes: U32,
        hTemplateFile: Handle
    ) -> Handle

    func ReadFile(
        hFile: Handle,
        lpBuffer: *mut U8,
        nNumberOfBytesToRead: U32,
        lpNumberOfBytesRead: *mut U32,
        lpOverlapped: *mut Void
    ) -> Bool

    func WriteFile(
        hFile: Handle,
        lpBuffer: *const U8,
        nNumberOfBytesToWrite: U32,
        lpNumberOfBytesWritten: *mut U32,
        lpOverlapped: *mut Void
    ) -> Bool

    func CloseHandle(hObject: Handle) -> Bool

    // Memory management
    func VirtualAlloc(
        lpAddress: *mut Void,
        dwSize: U64,
        flAllocationType: U32,
        flProtect: U32
    ) -> *mut Void

    func VirtualFree(
        lpAddress: *mut Void,
        dwSize: U64,
        dwFreeType: U32
    ) -> Bool

    func VirtualProtect(
        lpAddress: *mut Void,
        dwSize: U64,
        flNewProtect: U32,
        lpflOldProtect: *mut U32
    ) -> Bool

    // Process/Thread
    func GetCurrentProcess() -> Handle
    func GetCurrentThread() -> Handle
    func GetCurrentProcessId() -> U32
    func GetCurrentThreadId() -> U32
    func ExitProcess(uExitCode: U32) -> !
    func ExitThread(dwExitCode: U32) -> !

    func CreateThread(
        lpThreadAttributes: *mut Void,
        dwStackSize: U64,
        lpStartAddress: *func(*mut Void) -> U32,
        lpParameter: *mut Void,
        dwCreationFlags: U32,
        lpThreadId: *mut U32
    ) -> Handle

    func WaitForSingleObject(hHandle: Handle, dwMilliseconds: U32) -> U32

    // Synchronization
    func InitializeCriticalSection(lpCriticalSection: *mut CriticalSection)
    func EnterCriticalSection(lpCriticalSection: *mut CriticalSection)
    func LeaveCriticalSection(lpCriticalSection: *mut CriticalSection)
    func DeleteCriticalSection(lpCriticalSection: *mut CriticalSection)

    func CreateEventW(
        lpEventAttributes: *mut Void,
        bManualReset: Bool,
        bInitialState: Bool,
        lpName: *const U16
    ) -> Handle

    func SetEvent(hEvent: Handle) -> Bool
    func ResetEvent(hEvent: Handle) -> Bool

    // Time
    func GetSystemTimeAsFileTime(lpSystemTimeAsFileTime: *mut FileTime)
    func QueryPerformanceCounter(lpPerformanceCount: *mut I64) -> Bool
    func QueryPerformanceFrequency(lpFrequency: *mut I64) -> Bool
    func Sleep(dwMilliseconds: U32)

    // Console
    func GetStdHandle(nStdHandle: U32) -> Handle
    func WriteConsoleW(
        hConsoleOutput: Handle,
        lpBuffer: *const U16,
        nNumberOfCharsToWrite: U32,
        lpNumberOfCharsWritten: *mut U32,
        lpReserved: *mut Void
    ) -> Bool

    // Error handling
    func GetLastError() -> U32
    func SetLastError(dwErrCode: U32)
}

public type Handle = *mut Void
public const INVALID_HANDLE_VALUE: Handle = -1 as Handle

// Standard handles
public const STD_INPUT_HANDLE: U32 = 0xFFFFFFF6
public const STD_OUTPUT_HANDLE: U32 = 0xFFFFFFF5
public const STD_ERROR_HANDLE: U32 = 0xFFFFFFF4

// File access
public const GENERIC_READ: U32 = 0x80000000
public const GENERIC_WRITE: U32 = 0x40000000
public const GENERIC_EXECUTE: U32 = 0x20000000
public const GENERIC_ALL: U32 = 0x10000000

// Creation disposition
public const CREATE_NEW: U32 = 1
public const CREATE_ALWAYS: U32 = 2
public const OPEN_EXISTING: U32 = 3
public const OPEN_ALWAYS: U32 = 4
public const TRUNCATE_EXISTING: U32 = 5

// Memory allocation
public const MEM_COMMIT: U32 = 0x1000
public const MEM_RESERVE: U32 = 0x2000
public const MEM_RELEASE: U32 = 0x8000

// Memory protection
public const PAGE_NOACCESS: U32 = 0x01
public const PAGE_READONLY: U32 = 0x02
public const PAGE_READWRITE: U32 = 0x04
public const PAGE_EXECUTE: U32 = 0x10
public const PAGE_EXECUTE_READ: U32 = 0x20
public const PAGE_EXECUTE_READWRITE: U32 = 0x40

// Wait
public const INFINITE: U32 = 0xFFFFFFFF
public const WAIT_OBJECT_0: U32 = 0
public const WAIT_TIMEOUT: U32 = 0x102
public const WAIT_FAILED: U32 = 0xFFFFFFFF
```

## 4. macOS System Calls

### 4.1 macOS/Darwin Syscalls

```tml
@cfg(target_os = "macos")
module sys.macos

// macOS syscall interface (similar to Linux)
public lowlevel func syscall(nr: I32, ...) -> I64

// Syscall numbers (different from Linux!)
public const SYS_READ: I32 = 3
public const SYS_WRITE: I32 = 4
public const SYS_OPEN: I32 = 5
public const SYS_CLOSE: I32 = 6
public const SYS_MMAP: I32 = 197
public const SYS_MUNMAP: I32 = 73
public const SYS_MPROTECT: I32 = 74
public const SYS_EXIT: I32 = 1
public const SYS_FORK: I32 = 2
public const SYS_EXECVE: I32 = 59
public const SYS_GETPID: I32 = 20
public const SYS_KILL: I32 = 37
public const SYS_SOCKET: I32 = 97
public const SYS_CONNECT: I32 = 98
public const SYS_ACCEPT: I32 = 30
public const SYS_BIND: I32 = 104
public const SYS_LISTEN: I32 = 106
```

### 4.2 macOS libSystem Functions

```tml
@cfg(target_os = "macos")
module sys.macos.libsystem

extern "C" from "libSystem" {
    // pthread
    func pthread_create(
        thread: *mut pthread_t,
        attr: *const pthread_attr_t,
        start_routine: *func(*mut Void) -> *mut Void,
        arg: *mut Void
    ) -> I32

    func pthread_join(thread: pthread_t, retval: *mut *mut Void) -> I32
    func pthread_self() -> pthread_t
    func pthread_mutex_init(mutex: *mut pthread_mutex_t, attr: *const pthread_mutexattr_t) -> I32
    func pthread_mutex_lock(mutex: *mut pthread_mutex_t) -> I32
    func pthread_mutex_unlock(mutex: *mut pthread_mutex_t) -> I32
    func pthread_mutex_destroy(mutex: *mut pthread_mutex_t) -> I32

    // mach
    func mach_absolute_time() -> U64
    func mach_timebase_info(info: *mut mach_timebase_info_t) -> I32

    // random
    func arc4random() -> U32
    func arc4random_buf(buf: *mut Void, nbytes: U64)
}

public type pthread_t = U64
public type pthread_attr_t = [U8; 64]
public type pthread_mutex_t = [U8; 64]
public type pthread_mutexattr_t = [U8; 16]

public type mach_timebase_info_t {
    numer: U32,
    denom: U32,
}
```

## 5. Error Handling

### 5.1 Unix Errno

```tml
@cfg(unix)
module sys.errno

/// Get errno value
public func errno() -> I32 {
    unsafe { *__errno_location() }
}

/// Set errno value
public func set_errno(val: I32) {
    unsafe { *__errno_location() = val }
}

extern "C" {
    func __errno_location() -> *mut I32
}

// Common errno values
public const EPERM: I32 = 1
public const ENOENT: I32 = 2
public const ESRCH: I32 = 3
public const EINTR: I32 = 4
public const EIO: I32 = 5
public const ENXIO: I32 = 6
public const E2BIG: I32 = 7
public const ENOEXEC: I32 = 8
public const EBADF: I32 = 9
public const ECHILD: I32 = 10
public const EAGAIN: I32 = 11
public const ENOMEM: I32 = 12
public const EACCES: I32 = 13
public const EFAULT: I32 = 14
public const EBUSY: I32 = 16
public const EEXIST: I32 = 17
public const EXDEV: I32 = 18
public const ENODEV: I32 = 19
public const ENOTDIR: I32 = 20
public const EISDIR: I32 = 21
public const EINVAL: I32 = 22
public const ENFILE: I32 = 23
public const EMFILE: I32 = 24
public const ENOTTY: I32 = 25
public const ETXTBSY: I32 = 26
public const EFBIG: I32 = 27
public const ENOSPC: I32 = 28
public const ESPIPE: I32 = 29
public const EROFS: I32 = 30
public const EMLINK: I32 = 31
public const EPIPE: I32 = 32
public const EDOM: I32 = 33
public const ERANGE: I32 = 34
public const EDEADLK: I32 = 35
public const ENAMETOOLONG: I32 = 36
public const ENOSYS: I32 = 38
public const ENOTEMPTY: I32 = 39
public const ELOOP: I32 = 40
public const EWOULDBLOCK: I32 = EAGAIN
public const ECONNREFUSED: I32 = 111
public const ETIMEDOUT: I32 = 110
```

### 5.2 Result Conversion

```tml
@cfg(unix)
module sys.result

/// Check syscall result and convert to Outcome
public func check(result: I64) -> Outcome[U64, Errno] {
    if result < 0 {
        return Failure(Errno(-result as I32))
    }
    return Success(result as U64)
}

/// Check syscall result for I32 return
public func check_i32(result: I32) -> Outcome[I32, Errno] {
    if result < 0 {
        return Failure(Errno(errno.errno()))
    }
    return Success(result)
}

public type Errno(I32)

extend Errno {
    public func code(this) -> I32 { this.0 }

    public func message(this) -> ref static str {
        when this.0 {
            EPERM -> "Operation not permitted",
            ENOENT -> "No such file or directory",
            ESRCH -> "No such process",
            EINTR -> "Interrupted system call",
            EIO -> "I/O error",
            // ...
            _ -> "Unknown error",
        }
    }
}
```

## 6. Examples

### 6.1 Simple File I/O (Linux)

```tml
@cfg(target_os = "linux")
module example.file

use sys.linux.io.*
use sys.result.*

public func read_file(path: ref str) -> Outcome[List[U8], Errno] {
    lowlevel {
        // Open file
        let fd = check_i32(open(
            path.as_ptr(),
            O_RDONLY,
            0
        ))!

        // Read contents
        var buffer = List.with_capacity(4096)
        var buf: [U8; 4096] = [0; 4096]

        loop {
            let n = check(read(fd, buf.as_mut_ptr(), buf.len()))!
            if n == 0 {
                break
            }
            buffer.extend_from_slice(ref buf[0 to n as U64])
        }

        // Close file
        close(fd)

        return Success(buffer)
    }
}
```

### 6.2 Memory Allocator (Linux)

```tml
@cfg(target_os = "linux")
module example.alloc

use sys.linux.mem.*

/// Simple bump allocator using mmap
type BumpAllocator {
    base: *mut U8,
    size: U64,
    offset: U64,
}

extend BumpAllocator {
    public lowlevel func new(size: U64) -> Outcome[This, Errno] {
        let ptr = mmap(
            null,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        )

        if ptr == MAP_FAILED {
            return Failure(Errno(errno.errno()))
        }

        return Success(This {
            base: ptr as *mut U8,
            size: size,
            offset: 0,
        })
    }

    public lowlevel func alloc(this, size: U64, align: U64) -> *mut U8 {
        let aligned_offset = (this.offset + align - 1) & ~(align - 1)

        if aligned_offset + size > this.size {
            return null
        }

        let ptr = this.base.add(aligned_offset)
        this.offset = aligned_offset + size
        return ptr
    }

    public lowlevel func reset(this) {
        this.offset = 0
    }

    public lowlevel func drop(this) {
        munmap(this.base as *mut Void, this.size)
    }
}
```

### 6.3 Thread Creation (Linux)

```tml
@cfg(target_os = "linux")
module example.thread

use sys.linux.process.*
use sys.linux.mem.*
use sys.linux.futex.*

const STACK_SIZE: U64 = 1024 * 1024  // 1 MB

public lowlevel func spawn_thread(func: *func(*mut Void) -> *mut Void, arg: *mut Void) -> Outcome[I32, Errno] {
    // Allocate stack
    let stack = mmap(
        null,
        STACK_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
        -1,
        0
    )

    if stack == MAP_FAILED {
        return Failure(Errno(errno.errno()))
    }

    // Stack grows down, so start at top
    let stack_top = (stack as *mut U8).add(STACK_SIZE) as *mut Void

    // Clone with thread flags
    let flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                CLONE_THREAD | CLONE_SYSVSEM

    let tid = clone(
        flags,
        stack_top,
        null,
        null,
        0
    )

    if tid < 0 {
        munmap(stack, STACK_SIZE)
        return Failure(Errno(-tid as I32))
    }

    return Success(tid as I32)
}
```

---

*Previous: [23-INTRINSICS.md](./23-INTRINSICS.md)*
*Next: [INDEX.md](./INDEX.md) — Specification Index*
