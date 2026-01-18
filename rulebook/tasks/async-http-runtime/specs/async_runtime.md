# Async Runtime Specification

## Overview

The async runtime provides the execution environment for async/await code. It consists of:
1. **Executor** - Drives futures to completion
2. **Scheduler** - Decides which tasks to run
3. **Timer** - Handles timeouts and delays
4. **I/O Driver** - Integrates with the reactor

## Core Abstractions

### Poll Type
```tml
/// The result of polling a future
pub type Poll[T] {
    /// The future completed with this value
    Ready(T),
    /// The future is not ready, will wake when progress can be made
    Pending,
}

impl[T] Poll[T] {
    pub func is_ready(this) -> Bool {
        when this {
            Poll::Ready(_) => true
            Poll::Pending => false
        }
    }

    pub func is_pending(this) -> Bool {
        not this.is_ready()
    }

    pub func map[U, F: FnOnce(T) -> U](this, f: F) -> Poll[U] {
        when this {
            Poll::Ready(t) => Poll::Ready(f(t))
            Poll::Pending => Poll::Pending
        }
    }
}
```

### Future Trait
```tml
/// A value that may not be available yet
pub behavior Future {
    /// The type of value produced on completion
    type Output

    /// Attempt to resolve the future to a final value
    ///
    /// Returns `Poll::Pending` if the future is not ready.
    /// The implementation must arrange for `cx.waker().wake()`
    /// to be called when progress can be made.
    func poll(mut this, cx: mut ref Context) -> Poll[Self::Output]
}
```

### Context and Waker
```tml
/// Context passed to Future::poll
pub type Context {
    waker: ref Waker,
}

impl Context {
    pub func from_waker(waker: ref Waker) -> Context {
        Context { waker }
    }

    pub func waker(this) -> ref Waker {
        this.waker
    }
}

/// Wake mechanism for futures
pub type Waker {
    data: *(),
    vtable: ref WakerVTable,
}

/// Virtual table for waker operations
pub type WakerVTable {
    wake: fn(*()),
    wake_by_ref: fn(*()),
    clone: fn(*()) -> Waker,
    drop: fn(*()),
}

impl Waker {
    pub func wake(this) {
        (this.vtable.wake)(this.data)
    }

    pub func wake_by_ref(this) {
        (this.vtable.wake_by_ref)(this.data)
    }
}

impl Duplicate for Waker {
    func duplicate(this) -> Waker {
        (this.vtable.clone)(this.data)
    }
}

impl Drop for Waker {
    func drop(mut this) {
        (this.vtable.drop)(this.data)
    }
}
```

### Pin
```tml
/// A pinned pointer - the pointee cannot be moved
pub type Pin[P] {
    pointer: P,
}

impl[P: Deref] Pin[P] {
    /// Creates a Pin from a pointer that is already pinned
    pub lowlevel func new_unchecked(pointer: P) -> Pin[P] {
        Pin { pointer }
    }

    pub func as_ref(this) -> Pin[ref P::Target] {
        lowlevel { Pin::new_unchecked(ref *this.pointer) }
    }
}

impl[P: DerefMut] Pin[P] {
    pub func as_mut(mut this) -> Pin[mut ref P::Target] {
        lowlevel { Pin::new_unchecked(mut ref *this.pointer) }
    }
}

impl[P: Deref] Deref for Pin[P] {
    type Target = P::Target

    func deref(this) -> ref Self::Target {
        ref *this.pointer
    }
}
```

## Task Representation

```tml
/// A spawned async task
pub type Task {
    /// Unique task identifier
    id: TaskId,
    /// The future being executed (type-erased and pinned)
    future: Pin[Heap[dyn Future[Output = ()]>>,
    /// Current state
    state: AtomicU8,
    /// Waker for this task
    waker: TaskWaker,
}

pub type TaskId {
    value: U64,
}

/// Task states
pub mod TaskState {
    pub const IDLE: U8 = 0
    pub const SCHEDULED: U8 = 1
    pub const RUNNING: U8 = 2
    pub const COMPLETED: U8 = 3
}

/// Waker that reschedules the task
pub type TaskWaker {
    task_id: TaskId,
    scheduler: Sync[Scheduler],
}

impl TaskWaker {
    pub func into_waker(this) -> Waker {
        Waker {
            data: Heap::new(this) as *(),
            vtable: ref TASK_WAKER_VTABLE,
        }
    }
}

const TASK_WAKER_VTABLE: WakerVTable = WakerVTable {
    wake: task_waker_wake,
    wake_by_ref: task_waker_wake_by_ref,
    clone: task_waker_clone,
    drop: task_waker_drop,
}

func task_waker_wake(data: *()) {
    let waker = lowlevel { Heap::from_raw(data as *TaskWaker) }
    waker.scheduler.schedule(waker.task_id)
}

func task_waker_wake_by_ref(data: *()) {
    let waker = lowlevel { ref *(data as *TaskWaker) }
    waker.scheduler.schedule(waker.task_id)
}
```

## JoinHandle

```tml
/// Handle to a spawned task, can be awaited
pub type JoinHandle[T] {
    task_id: TaskId,
    result: Sync[Maybe[T]],
}

impl[T] Future for JoinHandle[T] {
    type Output = T

    func poll(mut this, cx: mut ref Context) -> Poll[T] {
        // Check if task completed
        when this.result.take() {
            Just(value) => Poll::Ready(value)
            Nothing => {
                // Register for notification
                // Return Pending
                Poll::Pending
            }
        }
    }
}
```

## Single-Threaded Executor

```tml
/// Simple single-threaded executor
pub type LocalExecutor {
    /// Task storage
    tasks: Slab[Task],
    /// Ready queue
    ready_queue: VecDeque[TaskId],
    /// I/O selector
    selector: Selector,
    /// Event buffer
    events: Events,
    /// Waker for selector
    waker: Waker,
    /// Next task ID
    next_id: U64,
}

impl LocalExecutor {
    pub func new() -> Outcome[LocalExecutor, IoError] {
        let selector = Selector::new()?
        let waker_token = Token::new(0)

        Ok(LocalExecutor {
            tasks: Slab::new(),
            ready_queue: VecDeque::new(),
            selector,
            events: Events::with_capacity(1024),
            waker: Waker::new(mut selector, waker_token)?,
            next_id: 1,
        })
    }

    /// Spawns a future onto this executor
    pub func spawn[F: Future[Output = ()]>(mut this, future: F) -> TaskId {
        let id = TaskId { value: this.next_id }
        this.next_id = this.next_id + 1

        let task = Task {
            id,
            future: Heap::pin(future),
            state: AtomicU8::new(TaskState::SCHEDULED),
            waker: TaskWaker {
                task_id: id,
                scheduler: // reference to self
            },
        }

        this.tasks.insert(task)
        this.ready_queue.push_back(id)
        id
    }

    /// Runs the executor until the main future completes
    pub func block_on[F: Future](mut this, future: F) -> F::Output {
        // Pin the main future on the stack
        let pinned = pin!(future)

        // Create waker for main task
        let main_waker = // ...

        loop {
            // Try to poll main future
            let cx = Context::from_waker(ref main_waker)
            when pinned.as_mut().poll(mut cx) {
                Poll::Ready(result) => return result
                Poll::Pending => {}
            }

            // Run all ready tasks
            this.run_ready_tasks()

            // If nothing ready, wait for I/O
            if this.ready_queue.is_empty() {
                this.park()
            }
        }
    }

    func run_ready_tasks(mut this) {
        while let Just(task_id) = this.ready_queue.pop_front() {
            if let Just(task) = this.tasks.get_mut(task_id.value) {
                task.state.store(TaskState::RUNNING)

                let waker = task.waker.duplicate().into_waker()
                let cx = Context::from_waker(ref waker)

                when task.future.as_mut().poll(mut cx) {
                    Poll::Ready(()) => {
                        task.state.store(TaskState::COMPLETED)
                        this.tasks.remove(task_id.value)
                    }
                    Poll::Pending => {
                        task.state.store(TaskState::IDLE)
                    }
                }
            }
        }
    }

    func park(mut this) {
        // Wait for I/O events with optional timeout
        this.selector.select(mut this.events, Just(Duration::from_millis(100))).ok()

        // Process events
        for event in this.events.iter() {
            // Wake corresponding task based on token
            // The token maps to a task or I/O resource
        }
    }
}
```

## Multi-Threaded Runtime

### Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                         Runtime                              │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    Scheduler                          │   │
│  │  ┌─────────────┐  ┌─────────────────────────────┐   │   │
│  │  │Global Queue │  │      Work-Stealing Queues   │   │   │
│  │  │   (MPMC)    │  │  ┌─────┐ ┌─────┐ ┌─────┐   │   │   │
│  │  └─────────────┘  │  │ W0  │ │ W1  │ │ W2  │   │   │   │
│  │                    │  └─────┘ └─────┘ └─────┘   │   │   │
│  │                    └─────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐   │
│  │ Worker 0 │  │ Worker 1 │  │ Worker 2 │  │ Blocking  │   │
│  │  Thread  │  │  Thread  │  │  Thread  │  │   Pool    │   │
│  └──────────┘  └──────────┘  └──────────┘  └───────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    I/O Driver                         │   │
│  │            (Shared selector + per-worker)            │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                   Timer Wheel                         │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Runtime Type
```tml
pub type Runtime {
    /// Shared scheduler
    scheduler: Sync[Scheduler],
    /// Worker threads
    workers: Vec[WorkerHandle],
    /// Blocking thread pool
    blocking_pool: Sync[ThreadPool],
    /// I/O driver
    io_driver: Sync[IoDriver],
    /// Timer wheel
    timer: Sync[TimerWheel],
    /// Shutdown signal
    shutdown: AtomicBool,
}

impl Runtime {
    pub func new() -> Outcome[Runtime, RuntimeError] {
        Runtime::builder().build()
    }

    pub func builder() -> RuntimeBuilder {
        RuntimeBuilder::new()
    }

    /// Spawns a future onto the runtime
    pub func spawn[F: Future + Send>(this, future: F) -> JoinHandle[F::Output]
    where F::Output: Send
    {
        let task = Task::new(future)
        let handle = JoinHandle::new(task.id)
        this.scheduler.schedule(task)
        handle
    }

    /// Runs the runtime, blocking the current thread
    pub func block_on[F: Future](this, future: F) -> F::Output {
        // Enter runtime context
        let _guard = RuntimeGuard::enter(this)

        // Run on current thread
        let local_executor = LocalExecutor::new_with_runtime(this)
        local_executor.block_on(future)
    }

    /// Shuts down the runtime
    pub func shutdown(this) {
        this.shutdown.store(true)
        // Wake all workers
        for worker in this.workers.iter() {
            worker.unpark()
        }
    }
}
```

### RuntimeBuilder
```tml
pub type RuntimeBuilder {
    worker_threads: Maybe[U32],
    max_blocking_threads: U32,
    thread_name: Str,
    thread_stack_size: Maybe[U64],
    on_thread_start: Maybe[fn()>,
    on_thread_stop: Maybe[fn()>,
}

impl RuntimeBuilder {
    pub func new() -> RuntimeBuilder {
        RuntimeBuilder {
            worker_threads: Nothing,
            max_blocking_threads: 512,
            thread_name: "tokio-runtime-worker",
            thread_stack_size: Nothing,
            on_thread_start: Nothing,
            on_thread_stop: Nothing,
        }
    }

    pub func worker_threads(mut this, val: U32) -> RuntimeBuilder {
        this.worker_threads = Just(val)
        this
    }

    pub func max_blocking_threads(mut this, val: U32) -> RuntimeBuilder {
        this.max_blocking_threads = val
        this
    }

    pub func build(this) -> Outcome[Runtime, RuntimeError] {
        let num_workers = this.worker_threads.unwrap_or(num_cpus())

        // Create shared state
        let scheduler = Sync::new(Scheduler::new(num_workers))
        let io_driver = Sync::new(IoDriver::new()?)
        let timer = Sync::new(TimerWheel::new())
        let blocking_pool = Sync::new(ThreadPool::new(this.max_blocking_threads))

        // Spawn worker threads
        var workers = Vec::with_capacity(num_workers as U64)
        for i in 0 to num_workers {
            let worker = Worker::new(i, scheduler.duplicate(), io_driver.duplicate())
            workers.push(worker.spawn()?)
        }

        Ok(Runtime {
            scheduler,
            workers,
            blocking_pool,
            io_driver,
            timer,
            shutdown: AtomicBool::new(false),
        })
    }
}
```

### Scheduler
```tml
pub type Scheduler {
    /// Global task queue
    global_queue: ConcurrentQueue[Sync[Task]>,
    /// Per-worker local queues
    local_queues: Vec[LocalQueue],
    /// Idle worker set
    idle_workers: AtomicBitset,
    /// Number of workers
    num_workers: U32,
}

impl Scheduler {
    pub func schedule(this, task: Task) {
        let task = Sync::new(task)

        // Try to push to current worker's local queue
        if let Just(worker_id) = current_worker_id() {
            if this.local_queues[worker_id].push(task.duplicate()) {
                return
            }
        }

        // Fall back to global queue
        this.global_queue.push(task)

        // Wake an idle worker
        this.wake_idle_worker()
    }

    func wake_idle_worker(this) {
        if let Just(worker_id) = this.idle_workers.find_first_set() {
            // Wake that worker
        }
    }
}

/// Work-stealing deque (SPSC with stealing)
pub type LocalQueue {
    /// Buffer of tasks
    buffer: [AtomicPtr[Task]; 256],
    /// Head (owner reads from here)
    head: AtomicU32,
    /// Tail (owner pushes here, stealers read)
    tail: AtomicU32,
}

impl LocalQueue {
    /// Push a task (only called by owner)
    pub func push(mut this, task: Sync[Task]) -> Bool {
        let tail = this.tail.load(Ordering::Relaxed)
        let head = this.head.load(Ordering::Acquire)

        if tail - head >= 256 {
            return false  // Full
        }

        this.buffer[tail % 256].store(Sync::into_raw(task), Ordering::Relaxed)
        this.tail.store(tail + 1, Ordering::Release)
        true
    }

    /// Pop a task (only called by owner)
    pub func pop(mut this) -> Maybe[Sync[Task]] {
        let tail = this.tail.load(Ordering::Relaxed)
        if tail == 0 {
            return Nothing
        }

        let new_tail = tail - 1
        this.tail.store(new_tail, Ordering::Relaxed)

        let head = this.head.load(Ordering::Acquire)
        if head <= new_tail {
            let ptr = this.buffer[new_tail % 256].load(Ordering::Relaxed)
            return Just(lowlevel { Sync::from_raw(ptr) })
        }

        // Queue is empty, restore tail
        this.tail.store(tail, Ordering::Relaxed)
        Nothing
    }

    /// Steal a task (called by other workers)
    pub func steal(this) -> Maybe[Sync[Task]] {
        let head = this.head.load(Ordering::Acquire)
        let tail = this.tail.load(Ordering::Acquire)

        if head >= tail {
            return Nothing  // Empty
        }

        let ptr = this.buffer[head % 256].load(Ordering::Relaxed)

        // Try to claim this slot
        if this.head.compare_exchange(head, head + 1, Ordering::AcqRel, Ordering::Relaxed).is_ok() {
            Just(lowlevel { Sync::from_raw(ptr) })
        } else {
            Nothing  // Lost race
        }
    }
}
```

### Worker Thread
```tml
pub type Worker {
    id: U32,
    local_queue: LocalQueue,
    scheduler: Sync[Scheduler],
    io_driver: Sync[IoDriver],
    park: Parker,
    rand: FastRand,
}

impl Worker {
    pub func run(mut this) {
        // Set thread-local worker ID
        CURRENT_WORKER.set(Just(this.id))

        loop {
            // Check for shutdown
            if this.scheduler.shutdown.load(Ordering::Relaxed) {
                break
            }

            // 1. Try local queue
            if let Just(task) = this.local_queue.pop() {
                this.run_task(task)
                continue
            }

            // 2. Try global queue
            if let Just(task) = this.scheduler.global_queue.pop() {
                this.run_task(task)
                continue
            }

            // 3. Try stealing
            if let Just(task) = this.steal() {
                this.run_task(task)
                continue
            }

            // 4. Process I/O
            if this.io_driver.poll() > 0 {
                continue
            }

            // 5. Park
            this.park.park()
        }
    }

    func run_task(mut this, task: Sync[Task]) {
        let waker = task.waker.duplicate().into_waker()
        let cx = Context::from_waker(ref waker)

        when task.future.as_mut().poll(mut cx) {
            Poll::Ready(()) => {
                // Task complete
            }
            Poll::Pending => {
                // Task will be rescheduled when waker is called
            }
        }
    }

    func steal(mut this) -> Maybe[Sync[Task]] {
        let num_workers = this.scheduler.num_workers
        let start = this.rand.next() % num_workers

        for i in 0 to num_workers {
            let victim = (start + i) % num_workers
            if victim != this.id {
                if let Just(task) = this.scheduler.local_queues[victim].steal() {
                    return Just(task)
                }
            }
        }

        Nothing
    }
}
```

## Timer Wheel

```tml
/// Hierarchical timing wheel
pub type TimerWheel {
    /// Current time in ticks
    current: U64,
    /// Wheel levels
    levels: [TimerLevel; 4],
    /// Tick duration (typically 1ms)
    tick_duration: Duration,
}

pub type TimerLevel {
    /// 256 slots per level
    slots: [TimerSlot; 256],
    /// Current position
    position: U8,
}

pub type TimerSlot {
    entries: LinkedList[TimerEntry],
}

pub type TimerEntry {
    deadline: U64,
    waker: Waker,
}

impl TimerWheel {
    /// Inserts a timer, returns handle for cancellation
    pub func insert(mut this, deadline: Instant, waker: Waker) -> TimerHandle {
        let deadline_ticks = this.instant_to_ticks(deadline)
        let entry = TimerEntry { deadline: deadline_ticks, waker }

        // Find appropriate level and slot
        let level = this.compute_level(deadline_ticks)
        let slot = this.compute_slot(deadline_ticks, level)

        let handle = this.levels[level].slots[slot].entries.push_back(entry)
        TimerHandle { level, slot, handle }
    }

    /// Advances time and fires expired timers
    pub func advance(mut this, now: Instant) -> Vec[Waker] {
        let now_ticks = this.instant_to_ticks(now)
        var expired = Vec::new()

        while this.current < now_ticks {
            this.current = this.current + 1

            // Check level 0 slot
            let slot = (this.current % 256) as U8
            for entry in this.levels[0].slots[slot].entries.drain() {
                if entry.deadline <= this.current {
                    expired.push(entry.waker)
                } else {
                    // Reinsert at appropriate level
                }
            }

            // Cascade from higher levels when needed
            if slot == 0 {
                this.cascade(1)
            }
        }

        expired
    }
}
```

## Blocking Thread Pool

```tml
pub type ThreadPool {
    /// Worker threads
    workers: Vec[PoolWorker],
    /// Task queue
    queue: ConcurrentQueue[BlockingTask],
    /// Number of idle workers
    idle_count: AtomicU32,
    /// Maximum threads
    max_threads: U32,
    /// Shutdown flag
    shutdown: AtomicBool,
}

pub type BlockingTask {
    func_ptr: Heap[dyn FnOnce()>,
}

impl ThreadPool {
    pub func spawn_blocking[F, R>(this, f: F) -> JoinHandle[R]
    where F: FnOnce() -> R + Send
    {
        // Create completion signal
        let (tx, rx) = oneshot::channel()

        let task = BlockingTask {
            func_ptr: Heap::new(move || {
                let result = f()
                tx.send(result)
            }),
        }

        this.queue.push(task)

        // Wake idle worker or spawn new one
        if this.idle_count.load(Ordering::Relaxed) == 0 {
            this.try_spawn_worker()
        }

        JoinHandle::from_receiver(rx)
    }
}

pub type PoolWorker {
    thread: JoinHandle[()],
}

impl PoolWorker {
    func run(pool: Sync[ThreadPool]) {
        loop {
            // Mark as idle
            pool.idle_count.fetch_add(1, Ordering::Relaxed)

            // Wait for task
            let task = pool.queue.pop_blocking()

            // Mark as busy
            pool.idle_count.fetch_sub(1, Ordering::Relaxed)

            if pool.shutdown.load(Ordering::Relaxed) {
                break
            }

            // Execute
            (task.func_ptr)()
        }
    }
}
```

## Global Runtime Access

```tml
/// Thread-local runtime handle
thread_local! {
    static RUNTIME: RefCell[Maybe[Handle]] = RefCell::new(Nothing)
}

/// Get current runtime handle
pub func current() -> Handle {
    RUNTIME.with(do(r) {
        r.borrow().duplicate().expect("Not in runtime context")
    })
}

/// Spawns onto the current runtime
pub func spawn[F: Future + Send>(future: F) -> JoinHandle[F::Output]
where F::Output: Send
{
    current().spawn(future)
}

/// Sleeps for the specified duration
pub async func sleep(duration: Duration) {
    Sleep::new(duration).await
}

/// Runs blocking code on the thread pool
pub async func spawn_blocking[F, R>(f: F) -> R
where F: FnOnce() -> R + Send, R: Send
{
    current().spawn_blocking(f).await
}
```

## async/await Syntax Desugaring

The `async` keyword transforms a function into one that returns a `Future`:

```tml
// Source
pub async func fetch_data(url: Str) -> Data {
    let response = client.get(url).await
    response.json().await
}

// Desugared
pub func fetch_data(url: Str) -> impl Future[Output = Data] {
    FetchDataFuture {
        state: FetchDataState::Start { url },
    }
}

type FetchDataState {
    Start { url: Str },
    Awaiting1 { future: GetFuture },
    Awaiting2 { future: JsonFuture, response: Response },
    Done,
}

type FetchDataFuture {
    state: FetchDataState,
}

impl Future for FetchDataFuture {
    type Output = Data

    func poll(mut this, cx: mut ref Context) -> Poll[Data] {
        loop {
            when this.state {
                FetchDataState::Start { url } => {
                    let future = client.get(url)
                    this.state = FetchDataState::Awaiting1 { future }
                }
                FetchDataState::Awaiting1 { mut future } => {
                    when future.poll(cx) {
                        Poll::Ready(response) => {
                            let json_future = response.json()
                            this.state = FetchDataState::Awaiting2 { future: json_future, response }
                        }
                        Poll::Pending => return Poll::Pending
                    }
                }
                FetchDataState::Awaiting2 { mut future, response } => {
                    when future.poll(cx) {
                        Poll::Ready(data) => {
                            this.state = FetchDataState::Done
                            return Poll::Ready(data)
                        }
                        Poll::Pending => return Poll::Pending
                    }
                }
                FetchDataState::Done => panic("polled after completion")
            }
        }
    }
}
```

This desugaring creates a state machine that progresses through each `.await` point.
