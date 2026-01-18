# Reactor Specification (mio-like I/O Abstraction)

## Overview

The reactor is the lowest layer of the async runtime, responsible for interfacing with OS-level event notification systems. It provides a unified API across platforms.

## Platform Backends

### Linux: epoll

```c
// System calls used
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

**Configuration:**
- Use `EPOLL_CLOEXEC` flag on creation
- Default to edge-triggered mode (`EPOLLET`) for performance
- Support level-triggered for compatibility

### macOS/BSD: kqueue

```c
// System calls used
int kqueue(void);
int kevent(int kq, const struct kevent *changelist, int nchanges,
           struct kevent *eventlist, int nevents, const struct timespec *timeout);
```

**Configuration:**
- Use `EV_CLEAR` for edge-triggered behavior
- Support multiple filters: `EVFILT_READ`, `EVFILT_WRITE`, `EVFILT_TIMER`

### Windows: IOCP

```c
// System calls used
HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort,
                               ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads);
BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
                                PULONG_PTR lpCompletionKey, LPOVERLAPPED *lpOverlapped,
                                DWORD dwMilliseconds);
BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
                                 ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);
```

**Note:** IOCP is completion-based (events fire when I/O completes) vs readiness-based (events fire when I/O is possible). The abstraction layer must handle this difference.

## Core Types

### Token
```tml
/// Identifies a registered I/O source
pub type Token {
    value: U64,
}

impl Token {
    pub func new(value: U64) -> Token {
        Token { value }
    }

    pub func as_u64(this) -> U64 {
        this.value
    }
}
```

### Interest
```tml
/// I/O interests to monitor
pub type Interest {
    bits: U8,
}

impl Interest {
    const NONE: U8 = 0b00
    const READ: U8 = 0b01
    const WRITE: U8 = 0b10

    pub func readable() -> Interest { Interest { bits: Interest::READ } }
    pub func writable() -> Interest { Interest { bits: Interest::WRITE } }

    pub func add(this, other: Interest) -> Interest {
        Interest { bits: this.bits | other.bits }
    }

    pub func is_readable(this) -> Bool { (this.bits & Interest::READ) != 0 }
    pub func is_writable(this) -> Bool { (this.bits & Interest::WRITE) != 0 }
}
```

### Event
```tml
/// A readiness event from the OS
pub type Event {
    token: Token,
    readiness: Interest,
    is_error: Bool,
    is_read_closed: Bool,
    is_write_closed: Bool,
}

impl Event {
    pub func token(this) -> Token { this.token }
    pub func is_readable(this) -> Bool { this.readiness.is_readable() }
    pub func is_writable(this) -> Bool { this.readiness.is_writable() }
    pub func is_error(this) -> Bool { this.is_error }
    pub func is_read_closed(this) -> Bool { this.is_read_closed }
    pub func is_write_closed(this) -> Bool { this.is_write_closed }
}
```

### Events (buffer)
```tml
/// Reusable buffer for collecting events
pub type Events {
    inner: Vec[Event],
    capacity: U32,
}

impl Events {
    pub func with_capacity(capacity: U32) -> Events {
        Events {
            inner: Vec::with_capacity(capacity as U64),
            capacity,
        }
    }

    pub func clear(mut this) {
        this.inner.clear()
    }

    pub func iter(this) -> impl Iterator[Item = ref Event] {
        this.inner.iter()
    }

    pub func len(this) -> U64 {
        this.inner.len()
    }
}
```

## Selector (Poll)

```tml
/// The main event notification interface
pub type Selector {
    #if LINUX
    epoll_fd: I32,
    #elif MACOS
    kqueue_fd: I32,
    #elif WINDOWS
    iocp: Handle,
    #endif
}

impl Selector {
    /// Creates a new selector
    pub func new() -> Outcome[Selector, IoError] {
        #if LINUX
        let fd = lowlevel { epoll_create1(EPOLL_CLOEXEC) }
        if fd < 0 {
            return Err(IoError::last_os_error())
        }
        Ok(Selector { epoll_fd: fd })
        #elif MACOS
        let fd = lowlevel { kqueue() }
        if fd < 0 {
            return Err(IoError::last_os_error())
        }
        Ok(Selector { kqueue_fd: fd })
        #elif WINDOWS
        let handle = lowlevel { CreateIoCompletionPort(INVALID_HANDLE_VALUE, null, 0, 0) }
        if handle == null {
            return Err(IoError::last_os_error())
        }
        Ok(Selector { iocp: handle })
        #endif
    }

    /// Registers a file descriptor for monitoring
    pub func register(
        mut this,
        fd: RawFd,
        token: Token,
        interests: Interest
    ) -> Outcome[(), IoError] {
        #if LINUX
        var event = epoll_event {
            events: interests_to_epoll(interests) | EPOLLET,
            data: epoll_data { u64: token.value },
        }
        let result = lowlevel { epoll_ctl(this.epoll_fd, EPOLL_CTL_ADD, fd, ref event) }
        if result < 0 {
            return Err(IoError::last_os_error())
        }
        Ok(())
        #elif MACOS
        // ... kqueue implementation
        #elif WINDOWS
        // ... IOCP implementation
        #endif
    }

    /// Modifies the interests for a registered fd
    pub func reregister(
        mut this,
        fd: RawFd,
        token: Token,
        interests: Interest
    ) -> Outcome[(), IoError] {
        // Similar to register but uses EPOLL_CTL_MOD
    }

    /// Removes a registered fd
    pub func deregister(mut this, fd: RawFd) -> Outcome[(), IoError] {
        #if LINUX
        let result = lowlevel { epoll_ctl(this.epoll_fd, EPOLL_CTL_DEL, fd, null) }
        if result < 0 {
            return Err(IoError::last_os_error())
        }
        Ok(())
        #endif
    }

    /// Waits for events, blocking up to timeout
    pub func select(
        mut this,
        events: mut ref Events,
        timeout: Maybe[Duration]
    ) -> Outcome[(), IoError] {
        events.clear()

        let timeout_ms: I32 = when timeout {
            Just(d) => d.as_millis() as I32
            Nothing => -1  // Block indefinitely
        }

        #if LINUX
        // Temporary buffer for raw events
        var raw_events: [epoll_event; 256] = zeroed()

        let n = lowlevel {
            epoll_wait(this.epoll_fd, raw_events.as_mut_ptr(), 256, timeout_ms)
        }

        if n < 0 {
            let err = IoError::last_os_error()
            if err.kind() == IoErrorKind::Interrupted {
                return Ok(())  // Spurious wakeup, try again
            }
            return Err(err)
        }

        for i in 0 to n {
            let raw = raw_events[i]
            events.inner.push(Event {
                token: Token { value: raw.data.u64 },
                readiness: epoll_to_interests(raw.events),
                is_error: (raw.events & EPOLLERR) != 0,
                is_read_closed: (raw.events & (EPOLLHUP | EPOLLRDHUP)) != 0,
                is_write_closed: (raw.events & EPOLLHUP) != 0,
            })
        }

        Ok(())
        #endif
    }
}

impl Drop for Selector {
    func drop(mut this) {
        #if LINUX
        lowlevel { close(this.epoll_fd) }
        #elif MACOS
        lowlevel { close(this.kqueue_fd) }
        #elif WINDOWS
        lowlevel { CloseHandle(this.iocp) }
        #endif
    }
}
```

## Waker

The waker allows cross-thread notification to wake up a blocked selector.

```tml
pub type Waker {
    #if LINUX
    eventfd: I32,
    #elif MACOS
    write_fd: I32,
    read_fd: I32,
    #elif WINDOWS
    event: Handle,
    #endif
}

impl Waker {
    pub func new(selector: mut ref Selector, token: Token) -> Outcome[Waker, IoError] {
        #if LINUX
        let fd = lowlevel { eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK) }
        if fd < 0 {
            return Err(IoError::last_os_error())
        }

        selector.register(fd, token, Interest::readable())?

        Ok(Waker { eventfd: fd })
        #elif MACOS
        var fds: [I32; 2] = zeroed()
        let result = lowlevel { pipe(fds.as_mut_ptr()) }
        if result < 0 {
            return Err(IoError::last_os_error())
        }

        // Set non-blocking
        lowlevel { fcntl(fds[0], F_SETFL, O_NONBLOCK) }
        lowlevel { fcntl(fds[1], F_SETFL, O_NONBLOCK) }

        selector.register(fds[0], token, Interest::readable())?

        Ok(Waker { read_fd: fds[0], write_fd: fds[1] })
        #endif
    }

    pub func wake(this) -> Outcome[(), IoError] {
        #if LINUX
        let val: U64 = 1
        let result = lowlevel { write(this.eventfd, ref val as *U8, 8) }
        if result < 0 {
            return Err(IoError::last_os_error())
        }
        Ok(())
        #elif MACOS
        let byte: U8 = 1
        lowlevel { write(this.write_fd, ref byte as *U8, 1) }
        Ok(())
        #endif
    }

    /// Called after waking to reset the waker
    pub func reset(this) -> Outcome[(), IoError] {
        #if LINUX
        var buf: U64 = 0
        lowlevel { read(this.eventfd, ref buf as *U8, 8) }
        Ok(())
        #elif MACOS
        var buf: [U8; 64] = zeroed()
        loop {
            let n = lowlevel { read(this.read_fd, buf.as_mut_ptr(), 64) }
            if n <= 0 { break }
        }
        Ok(())
        #endif
    }
}
```

## Source Registration

Wrapper for I/O sources that auto-registers with selector.

```tml
pub type IoSource[T] {
    inner: T,
    registration: IoRegistration,
}

pub type IoRegistration {
    token: Token,
    selector: Weak[Selector],
    interests: Interest,
}

impl IoRegistration {
    pub func readable(this) -> ReadyFuture {
        ReadyFuture {
            registration: ref this,
            interest: Interest::readable(),
        }
    }

    pub func writable(this) -> ReadyFuture {
        ReadyFuture {
            registration: ref this,
            interest: Interest::writable(),
        }
    }
}

/// Future that resolves when I/O is ready
pub type ReadyFuture {
    registration: ref IoRegistration,
    interest: Interest,
}

impl Future for ReadyFuture {
    type Output = Outcome[(), IoError]

    func poll(mut this, cx: mut ref Context) -> Poll[Outcome[(), IoError]] {
        // Check if already ready
        // If not, register waker and return Pending
        // The selector will wake us when ready
    }
}
```

## Thread Safety

- `Selector` is not `Send` or `Sync` - must be used from single thread
- `Waker` is `Send + Sync` - can be cloned and sent to other threads
- Each worker thread has its own selector
- Cross-thread waking uses the `Waker` mechanism

## Performance Considerations

1. **Batch registration changes** - Avoid syscalls for each change
2. **Reuse event buffers** - Allocate once, clear and reuse
3. **Edge-triggered by default** - Fewer syscalls for high-throughput
4. **Vectored I/O** - Support `readv`/`writev` for efficiency
5. **Memory-map large transfers** - For file serving

## Error Handling

All operations return `Outcome[T, IoError]`. The `IoError` type wraps platform-specific error codes:

```tml
pub type IoError {
    kind: IoErrorKind,
    os_error: I32,
}

pub type IoErrorKind {
    WouldBlock,
    Interrupted,
    ConnectionRefused,
    ConnectionReset,
    NotConnected,
    AddrInUse,
    AddrNotAvailable,
    BrokenPipe,
    AlreadyExists,
    NotFound,
    PermissionDenied,
    TimedOut,
    Other,
}
```
