# TML Standard Library: Synchronization

> `std::sync` — Channels, synchronization primitives, and concurrent data structures.

## Overview

The sync package provides high-level synchronization primitives for communication between threads. It builds on the low-level atomics and locks from the core library.

## Import

```tml
use std::sync
use std::sync.{channel, mpsc, broadcast, Barrier, Once}
```

---

## Channels

### Basic Channel (SPSC)

Single-producer, single-consumer channel.

```tml
/// Creates a bounded SPSC channel
pub func channel[T](capacity: U64) -> (Sender[T], Receiver[T]) {
    let inner = Arc.new(ChannelInner[T].new(capacity))
    return (
        Sender { inner: inner.duplicate() },
        Receiver { inner: inner },
    )
}

/// Creates an unbounded SPSC channel
pub func unbounded[T]() -> (Sender[T], Receiver[T]) {
    let inner = Arc.new(ChannelInner[T].unbounded())
    return (
        Sender { inner: inner.duplicate() },
        Receiver { inner: inner },
    )
}
```

### Sender

```tml
/// Sending half of a channel
pub type Sender[T] {
    inner: Arc[ChannelInner[T]],
}

extend Sender[T] {
    /// Sends a value, blocking if the channel is full
    pub func send(this, value: T) -> Outcome[Unit, SendError[T]] {
        this.inner.send(value)
    }

    /// Tries to send without blocking
    pub func try_send(this, value: T) -> Outcome[Unit, TrySendError[T]] {
        this.inner.try_send(value)
    }

    /// Sends with timeout
    pub func send_timeout(this, value: T, timeout: Duration) -> Outcome[Unit, SendTimeoutError[T]] {
        this.inner.send_timeout(value, timeout)
    }

    /// Returns true if the receiver is still connected
    pub func is_connected(this) -> Bool {
        Arc.strong_count(ref this.inner) > 1
    }

    /// Returns the number of messages in the channel
    pub func len(this) -> U64 {
        this.inner.len()
    }

    /// Returns true if the channel is empty
    pub func is_empty(this) -> Bool {
        this.inner.is_empty()
    }

    /// Returns true if the channel is full
    pub func is_full(this) -> Bool {
        this.inner.is_full()
    }
}

extend Sender[T] with Duplicate {
    func clone(this) -> Sender[T] {
        return Sender { inner: this.inner.duplicate() }
    }
}
```

### Receiver

```tml
/// Receiving half of a channel
pub type Receiver[T] {
    inner: Arc[ChannelInner[T]],
}

extend Receiver[T] {
    /// Receives a value, blocking if the channel is empty
    pub func recv(this) -> Outcome[T, RecvError] {
        this.inner.recv()
    }

    /// Tries to receive without blocking
    pub func try_recv(this) -> Outcome[T, TryRecvError] {
        this.inner.try_recv()
    }

    /// Receives with timeout
    pub func recv_timeout(this, timeout: Duration) -> Outcome[T, RecvTimeoutError] {
        this.inner.recv_timeout(timeout)
    }

    /// Returns an iterator over received values
    pub func iter(this) -> RecvIter[T] {
        return RecvIter { receiver: this }
    }

    /// Tries to receive all available values
    pub func try_recv_all(this) -> Vec[T] {
        var result = Vec.new()
        loop {
            when this.try_recv() {
                Ok(value) -> result.push(value),
                Err(_) -> break,
            }
        }
        return result
    }
}

extend Receiver[T] with IntoIterator {
    type Item = T
    type Iter = RecvIter[T]

    func into_iter(this) -> RecvIter[T] {
        return RecvIter { receiver: this }
    }
}

/// Iterator over received values
pub type RecvIter[T] {
    receiver: Receiver[T],
}

extend RecvIter[T] with Iterator {
    type Item = T

    func next(mut this) -> Maybe[T] {
        this.receiver.recv().ok()
    }
}
```

### Errors

```tml
/// Error when sending to a disconnected channel
pub type SendError[T] {
    value: T,
}

/// Error when try_send fails
pub type TrySendError[T] = Full(T) | Disconnected(T)

/// Error when send_timeout fails
pub type SendTimeoutError[T] = Timeout(T) | Disconnected(T)

/// Error when receiving from a disconnected channel
pub type RecvError = Disconnected

/// Error when try_recv fails
pub type TryRecvError = Empty | Disconnected

/// Error when recv_timeout fails
pub type RecvTimeoutError = Timeout | Disconnected
```

---

## MPSC (Multi-Producer, Single-Consumer)

```tml
/// Creates a bounded MPSC channel
pub func mpsc[T](capacity: U64) -> (MpscSender[T], MpscReceiver[T]) {
    let inner = Arc.new(MpscInner[T].new(capacity))
    return (
        MpscSender { inner: inner.duplicate() },
        MpscReceiver { inner: inner },
    )
}

/// Creates an unbounded MPSC channel
pub func mpsc_unbounded[T]() -> (MpscSender[T], MpscReceiver[T]) {
    let inner = Arc.new(MpscInner[T].unbounded())
    return (
        MpscSender { inner: inner.duplicate() },
        MpscReceiver { inner: inner },
    )
}
```

### MpscSender

```tml
/// Duplicateable sender for MPSC channel
pub type MpscSender[T] {
    inner: Arc[MpscInner[T]],
}

extend MpscSender[T] {
    /// Sends a value
    pub func send(this, value: T) -> Outcome[Unit, SendError[T]]

    /// Tries to send without blocking
    pub func try_send(this, value: T) -> Outcome[Unit, TrySendError[T]]

    /// Creates a permit to send
    pub async func reserve(this) -> Outcome[Permit[T], SendError[Unit]]

    /// Blocks until there's capacity
    pub func blocking_send(this, value: T) -> Outcome[Unit, SendError[T]]
}

/// A permit to send one value
pub type Permit[T] {
    sender: MpscSender[T],
}

extend Permit[T] {
    /// Sends a value using this permit
    pub func send(this, value: T)
}

extend MpscSender[T] with Duplicate {
    func clone(this) -> MpscSender[T] {
        this.inner.sender_count.fetch_add(1, Ordering.Relaxed)
        return MpscSender { inner: this.inner.duplicate() }
    }
}

extend MpscSender[T] with Disposable {
    func drop(mut this) {
        if this.inner.sender_count.fetch_sub(1, Ordering.AcqRel) == 1 then {
            // Last sender, notify receiver
            this.inner.notify_receiver()
        }
    }
}
```

### MpscReceiver

```tml
/// Receiver for MPSC channel
pub type MpscReceiver[T] {
    inner: Arc[MpscInner[T]],
}

extend MpscReceiver[T] {
    /// Receives a value
    pub func recv(this) -> Maybe[T]

    /// Tries to receive without blocking
    pub func try_recv(this) -> Outcome[T, TryRecvError]

    /// Receives with timeout
    pub func recv_timeout(this, timeout: Duration) -> Outcome[T, RecvTimeoutError]

    /// Async receive
    pub async func recv_async(this) -> Maybe[T]

    /// Closes the channel
    pub func close(this)
}
```

---

## MPMC (Multi-Producer, Multi-Consumer)

```tml
/// Creates a bounded MPMC channel
pub func mpmc[T](capacity: U64) -> (MpmcSender[T], MpmcReceiver[T]) {
    let inner = Arc.new(MpmcInner[T].new(capacity))
    return (
        MpmcSender { inner: inner.duplicate() },
        MpmcReceiver { inner: inner },
    )
}

/// MPMC sender (cloneable)
pub type MpmcSender[T] {
    inner: Arc[MpmcInner[T]],
}

extend MpmcSender[T] {
    pub func send(this, value: T) -> Outcome[Unit, SendError[T]]
    pub func try_send(this, value: T) -> Outcome[Unit, TrySendError[T]]
}

extend MpmcSender[T] with Duplicate {
    func clone(this) -> MpmcSender[T] {
        return MpmcSender { inner: this.inner.duplicate() }
    }
}

/// MPMC receiver (cloneable)
pub type MpmcReceiver[T] {
    inner: Arc[MpmcInner[T]],
}

extend MpmcReceiver[T] {
    pub func recv(this) -> Outcome[T, RecvError]
    pub func try_recv(this) -> Outcome[T, TryRecvError]
}

extend MpmcReceiver[T] with Duplicate {
    func clone(this) -> MpmcReceiver[T] {
        return MpmcReceiver { inner: this.inner.duplicate() }
    }
}
```

---

## Broadcast Channel

Sends to multiple receivers, each receiving all messages.

```tml
/// Creates a broadcast channel
pub func broadcast[T](capacity: U64) -> (BroadcastSender[T], BroadcastReceiver[T])
    where T: Duplicate
{
    let inner = Arc.new(BroadcastInner[T].new(capacity))
    return (
        BroadcastSender { inner: inner.duplicate() },
        BroadcastReceiver { inner: inner, pos: 0 },
    )
}

/// Broadcast sender
pub type BroadcastSender[T] {
    inner: Arc[BroadcastInner[T]],
}

extend BroadcastSender[T] where T: Duplicate {
    /// Sends a value to all receivers
    pub func send(this, value: T) -> Outcome[U64, SendError[T]] {
        this.inner.send(value)
    }

    /// Returns the number of active receivers
    pub func receiver_count(this) -> U64 {
        this.inner.receiver_count.load(Ordering.Relaxed)
    }

    /// Creates a new receiver
    pub func subscribe(this) -> BroadcastReceiver[T] {
        let pos = this.inner.tail.load(Ordering.Relaxed)
        this.inner.receiver_count.fetch_add(1, Ordering.Relaxed)
        return BroadcastReceiver { inner: this.inner.duplicate(), pos: pos }
    }
}

/// Broadcast receiver
pub type BroadcastReceiver[T] {
    inner: Arc[BroadcastInner[T]],
    pos: U64,
}

extend BroadcastReceiver[T] where T: Duplicate {
    /// Receives the next value
    pub func recv(mut this) -> Outcome[T, RecvError] {
        this.inner.recv(mut ref this.pos)
    }

    /// Tries to receive without blocking
    pub func try_recv(mut this) -> Outcome[T, TryRecvError] {
        this.inner.try_recv(mut ref this.pos)
    }

    /// Returns the number of messages lagging behind
    pub func lag(this) -> U64 {
        let tail = this.inner.tail.load(Ordering.Relaxed)
        tail.saturating_sub(this.pos)
    }
}

extend BroadcastReceiver[T] with Duplicate {
    func clone(this) -> BroadcastReceiver[T] {
        this.inner.receiver_count.fetch_add(1, Ordering.Relaxed)
        return BroadcastReceiver { inner: this.inner.duplicate(), pos: this.pos }
    }
}

extend BroadcastReceiver[T] with Disposable {
    func drop(mut this) {
        this.inner.receiver_count.fetch_sub(1, Ordering.Relaxed)
    }
}
```

---

## Watch Channel

Single value that notifies receivers on change.

```tml
/// Creates a watch channel
pub func watch[T](initial: T) -> (WatchSender[T], WatchReceiver[T])
    where T: Duplicate
{
    let inner = Arc.new(WatchInner[T].new(initial))
    return (
        WatchSender { inner: inner.duplicate() },
        WatchReceiver { inner: inner, version: 0 },
    )
}

/// Watch sender
pub type WatchSender[T] {
    inner: Arc[WatchInner[T]],
}

extend WatchSender[T] where T: Duplicate {
    /// Sends a new value
    pub func send(this, value: T) -> Outcome[Unit, SendError[T]] {
        this.inner.send(value)
    }

    /// Modifies the value in place
    pub func send_modify(this, f: func(mut ref T)) {
        this.inner.send_modify(f)
    }

    /// Returns a reference to the current value
    pub func borrow(this) -> WatchRef[T] {
        this.inner.borrow()
    }

    /// Creates a new receiver
    pub func subscribe(this) -> WatchReceiver[T] {
        let version = this.inner.version.load(Ordering.Relaxed)
        return WatchReceiver { inner: this.inner.duplicate(), version: version }
    }

    /// Returns the number of receivers
    pub func receiver_count(this) -> U64 {
        Arc.strong_count(ref this.inner) - 1
    }
}

/// Watch receiver
pub type WatchReceiver[T] {
    inner: Arc[WatchInner[T]],
    version: U64,
}

extend WatchReceiver[T] where T: Duplicate {
    /// Returns a reference to the current value
    pub func borrow(this) -> WatchRef[T] {
        this.inner.borrow()
    }

    /// Waits for a new value
    pub async func changed(mut this) -> Outcome[Unit, RecvError] {
        this.inner.changed(mut ref this.version).await
    }

    /// Marks the current value as seen
    pub func mark_seen(mut this) {
        this.version = this.inner.version.load(Ordering.Relaxed)
    }

    /// Returns true if the value has changed since last seen
    pub func has_changed(this) -> Bool {
        this.version != this.inner.version.load(Ordering.Relaxed)
    }
}

extend WatchReceiver[T] with Duplicate {
    func clone(this) -> WatchReceiver[T] {
        return WatchReceiver { inner: this.inner.duplicate(), version: this.version }
    }
}
```

---

## Oneshot Channel

Single-use channel for one value.

```tml
/// Creates a oneshot channel
pub func oneshot[T]() -> (OneshotSender[T], OneshotReceiver[T]) {
    let inner = Arc.new(OneshotInner[T].new())
    return (
        OneshotSender { inner: Just(inner.duplicate()) },
        OneshotReceiver { inner: inner },
    )
}

/// Oneshot sender
pub type OneshotSender[T] {
    inner: Maybe[Arc[OneshotInner[T]]],
}

extend OneshotSender[T] {
    /// Sends a value, consuming the sender
    pub func send(mut this, value: T) -> Outcome[Unit, T] {
        when this.inner.take() {
            Just(inner) -> inner.send(value),
            Nothing -> Err(value),
        }
    }

    /// Returns true if the receiver is still waiting
    pub func is_connected(this) -> Bool {
        this.inner.as_ref().map(|i| Arc.strong_count(i) > 1).unwrap_or(false)
    }
}

/// Oneshot receiver
pub type OneshotReceiver[T] {
    inner: Arc[OneshotInner[T]],
}

extend OneshotReceiver[T] {
    /// Waits for the value
    pub async func await(this) -> Outcome[T, RecvError] {
        this.inner.recv().await
    }

    /// Tries to receive without blocking
    pub func try_recv(this) -> Outcome[T, TryRecvError] {
        this.inner.try_recv()
    }

    /// Blocks waiting for the value
    pub func blocking_recv(this) -> Outcome[T, RecvError] {
        this.inner.blocking_recv()
    }
}
```

---

## Select

Wait on multiple channel operations.

```tml
/// Selects from multiple channel operations
pub macro select! {
    ($($pattern:pat = $expr:expr => $body:expr),+ $(,)?) => {
        // Implementation uses an internal select mechanism
    }
}

// Example usage:
select! {
    msg = rx1.recv() => {
        handle_message1(msg)
    },
    msg = rx2.recv() => {
        handle_message2(msg)
    },
    _ = timeout(Duration.from_secs(5)) => {
        handle_timeout()
    },
}
```

### Biased Select

```tml
/// Biased select (checks in order)
pub macro select_biased! {
    ($($pattern:pat = $expr:expr => $body:expr),+ $(,)?) => {
        // Checks channels in order, useful for priority
    }
}
```

---

## Barrier

Synchronization point for multiple threads.

```tml
/// A barrier for synchronizing threads
pub type Barrier {
    count: U64,
    waiting: AtomicU64,
    generation: AtomicU64,
    mutex: Mutex[Unit],
    cond: Condvar,
}

extend Barrier {
    /// Creates a barrier for n threads
    pub func new(count: U64) -> Barrier {
        return Barrier {
            count: count,
            waiting: AtomicU64.new(0),
            generation: AtomicU64.new(0),
            mutex: Mutex.new(()),
            cond: Condvar.new(),
        }
    }

    /// Waits at the barrier
    pub func wait(this) -> BarrierWaitResult {
        let guard = this.mutex.lock()
        let gen = this.generation.load(Ordering.Relaxed)

        let waiting = this.waiting.fetch_add(1, Ordering.Relaxed) + 1

        if waiting == this.count then {
            // Last thread to arrive
            this.waiting.store(0, Ordering.Relaxed)
            this.generation.fetch_add(1, Ordering.Relaxed)
            this.cond.notify_all()
            return BarrierWaitResult { is_leader: true }
        }

        // Wait for others
        loop this.generation.load(Ordering.Relaxed) == gen {
            this.cond.wait(ref guard)
        }

        return BarrierWaitResult { is_leader: false }
    }
}

/// Result of waiting at a barrier
pub type BarrierWaitResult {
    is_leader: Bool,
}

extend BarrierWaitResult {
    /// Returns true if this thread was the last to arrive
    pub func is_leader(this) -> Bool {
        this.is_leader
    }
}
```

---

## Once

One-time initialization.

```tml
/// Ensures code runs exactly once
pub type Once {
    state: AtomicU8,
}

const INCOMPLETE: U8 = 0
const RUNNING: U8 = 1
const COMPLETE: U8 = 2

extend Once {
    /// Creates a new Once
    pub const func new() -> Once {
        return Once { state: AtomicU8.new(INCOMPLETE) }
    }

    /// Calls the function if not already called
    pub func call_once(this, f: func()) {
        if this.state.load(Ordering.Acquire) == COMPLETE then {
            return
        }
        this.call_once_slow(f)
    }

    func call_once_slow(this, f: func()) {
        when this.state.compare_exchange(INCOMPLETE, RUNNING, Ordering.Acquire, Ordering.Relaxed) {
            Ok(_) -> {
                f()
                this.state.store(COMPLETE, Ordering.Release)
            },
            Err(RUNNING) -> {
                // Spin until complete
                loop this.state.load(Ordering.Acquire) == RUNNING {
                    thread.yield_now()
                }
            },
            Err(_) -> {},  // Already complete
        }
    }

    /// Returns true if initialization is complete
    pub func is_completed(this) -> Bool {
        this.state.load(Ordering.Acquire) == COMPLETE
    }
}
```

### OnceCell

Lazy initialization container.

```tml
/// A cell that can be written to once
pub type OnceCell[T] {
    once: Once,
    value: UnsafeCell[Maybe[T]],
}

extend OnceCell[T] {
    /// Creates an empty OnceCell
    pub const func new() -> OnceCell[T] {
        return OnceCell {
            once: Once.new(),
            value: UnsafeCell.new(Nothing),
        }
    }

    /// Gets or initializes the value
    pub func get_or_init(this, f: func() -> T) -> ref T {
        this.once.call_once(do() {
            lowlevel {
                *this.value.get() = Just(f())
            }
        })
        return lowlevel { (*this.value.get()).as_ref().unwrap() }
    }

    /// Gets the value if initialized
    pub func get(this) -> Maybe[ref T] {
        if this.once.is_completed() then {
            return lowlevel { (*this.value.get()).as_ref() }
        }
        return None
    }

    /// Sets the value if not initialized
    pub func set(this, value: T) -> Outcome[Unit, T] {
        var stored = false
        this.once.call_once(do() {
            lowlevel {
                *this.value.get() = Just(value)
            }
            stored = true
        })
        if stored then Ok(()) else Err(value)
    }
}

// Thread-safe: Sync if T is Send
extend OnceCell[T] with Sync where T: Send {}
```

### Lazy

Lazy evaluation with memoization.

```tml
/// Lazy value computed on first access
pub type Lazy[T] {
    cell: OnceCell[T],
    init: Cell[Maybe[func() -> T]],
}

extend Lazy[T] {
    /// Creates a lazy value
    pub const func new(f: func() -> T) -> Lazy[T] {
        return Lazy {
            cell: OnceCell.new(),
            init: Cell.new(Just(f)),
        }
    }

    /// Forces evaluation and returns a reference
    pub func force(this) -> ref T {
        this.cell.get_or_init(do() {
            let f = this.init.take().expect("Lazy already initialized")
            f()
        })
    }
}

extend Lazy[T] with Deref {
    type Target = T

    func deref(this) -> ref T {
        this.force()
    }
}
```

---

## Concurrent Data Structures

### ConcurrentHashMap

```tml
/// Thread-safe hash map
pub type ConcurrentHashMap[K, V] {
    shards: [RwLock[HashMap[K, V]]; 16],
}

extend ConcurrentHashMap[K, V] where K: Hash + Eq {
    /// Creates a new concurrent hash map
    pub func new() -> ConcurrentHashMap[K, V]

    /// Inserts a key-value pair
    pub func insert(this, key: K, value: V) -> Maybe[V] {
        let shard = this.get_shard(ref key)
        shard.write().insert(key, value)
    }

    /// Gets a value by key
    pub func get[Q](this, key: ref Q) -> Maybe[V]
        where K: Borrow[Q], Q: Hash + Eq, V: Duplicate
    {
        let shard = this.get_shard(key)
        shard.read().get(key).duplicated()
    }

    /// Removes a key
    pub func remove[Q](this, key: ref Q) -> Maybe[V]
        where K: Borrow[Q], Q: Hash + Eq
    {
        let shard = this.get_shard(key)
        shard.write().remove(key)
    }

    /// Returns true if the key exists
    pub func contains_key[Q](this, key: ref Q) -> Bool
        where K: Borrow[Q], Q: Hash + Eq
    {
        let shard = this.get_shard(key)
        shard.read().contains_key(key)
    }

    /// Gets or inserts a value
    pub func get_or_insert(this, key: K, value: V) -> V
        where V: Duplicate
    {
        let shard = this.get_shard(ref key)
        var guard = shard.write()
        guard.entry(key).or_insert(value).duplicate()
    }

    func get_shard[Q](this, key: ref Q) -> &RwLock[HashMap[K, V]]
        where Q: Hash
    {
        let hash = hash(key)
        let index = (hash as U64) % 16
        return ref this.shards[index]
    }
}
```

### ConcurrentQueue

```tml
/// Lock-free concurrent queue
pub type ConcurrentQueue[T] {
    head: AtomicPtr[Node[T]],
    tail: AtomicPtr[Node[T]],
}

type Node[T] {
    value: Maybe[T],
    next: AtomicPtr[Node[T]],
}

extend ConcurrentQueue[T] {
    /// Creates a new concurrent queue
    pub func new() -> ConcurrentQueue[T]

    /// Pushes a value to the back
    pub func push(this, value: T)

    /// Pops a value from the front
    pub func pop(this) -> Maybe[T]

    /// Returns true if the queue is empty
    pub func is_empty(this) -> Bool
}
```

---

## Examples

### Worker Pool with Channels

```tml
use std::sync.{mpsc, channel}
use std::thread

type Task = func() -> I32

func worker_pool(num_workers: U64) {
    let (tx, rx) = mpsc[Task](100)

    // Spawn workers
    var handles = Vec.new()
    loop i in 0 to num_workers {
        let rx = rx.duplicate()
        let handle = thread.spawn(do() {
            loop task in rx {
                let result = task()
                print("Worker " + i.to_string() + " completed: " + result.to_string())
            }
        })
        handles.push(handle)
    }

    // Send tasks
    loop i in 0 to 10 {
        tx.send(do() i * i).unwrap()
    }

    // Close channel
    drop(tx)

    // Wait for workers
    loop handle in handles {
        handle.join().unwrap()
    }
}
```

### Broadcast Updates

```tml
use std::sync.broadcast

type Update = DataUpdate { data: String } | Shutdown

func broadcast_example() {
    let (tx, rx) = broadcast[Update](16)

    // Spawn subscribers
    loop i in 0 to 3 {
        var rx = tx.subscribe()
        thread.spawn(do() {
            loop {
                when rx.recv() {
                    Ok(DataUpdate { data }) -> print("Subscriber " + i.to_string() + ": " + data),
                    Ok(Shutdown) -> break,
                    Err(_) -> break,
                }
            }
        })
    }

    // Send updates
    tx.send(DataUpdate { data: "Hello" }).unwrap()
    tx.send(DataUpdate { data: "World" }).unwrap()
    tx.send(Shutdown).unwrap()
}
```

### Barrier for Parallel Computation

```tml
use std::sync.Barrier

func parallel_compute() {
    let barrier = Arc.new(Barrier.new(4))
    var handles = Vec.new()

    loop i in 0 to 4 {
        let barrier = barrier.duplicate()
        let handle = thread.spawn(do() {
            // Phase 1
            compute_phase1(i)
            print("Thread " + i.to_string() + " finished phase 1")

            // Wait for all threads
            barrier.wait()

            // Phase 2 (all threads start together)
            compute_phase2(i)
            print("Thread " + i.to_string() + " finished phase 2")
        })
        handles.push(handle)
    }

    loop handle in handles {
        handle.join().unwrap()
    }
}
```

---

## See Also

- [22-LOW-LEVEL.md](../specs/22-LOW-LEVEL.md) — Low-level atomics and locks
- [std.thread](./22-THREAD.md) — Thread creation and management
- [std.async](./14-ASYNC.md) — Async runtime
