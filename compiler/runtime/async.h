/**
 * @file async.h
 * @brief TML Runtime - Async Executor Header
 *
 * Provides the async runtime infrastructure for TML's async/await system.
 * This header defines the types and functions needed to execute asynchronous
 * tasks using a cooperative multitasking model based on polling.
 *
 * ## Architecture
 *
 * The async system follows Rust's Future model:
 * - **Task**: A unit of async work with state and a poll function
 * - **Executor**: Schedules and runs tasks to completion
 * - **Waker**: Mechanism to re-schedule suspended tasks
 * - **Poll**: Result type indicating Ready(value) or Pending
 *
 * ## Poll Model
 *
 * Each async function is transformed into a state machine with a poll function:
 * ```c
 * TmlPoll poll_fn(void* state, TmlContext* cx) {
 *     // Check if work is complete
 *     if (ready) return tml_poll_ready_i64(result);
 *     // Still waiting - register waker and return pending
 *     return tml_poll_pending();
 * }
 * ```
 *
 * ## Usage
 *
 * ```c
 * TmlExecutor* exec = tml_executor_new();
 * uint64_t task_id = tml_executor_spawn(exec, my_poll_fn, &state, sizeof(state));
 * tml_executor_run(exec);
 * tml_executor_destroy(exec);
 * ```
 *
 * ## Synchronization Primitives
 *
 * - **Channel**: Bounded SPSC channel for task communication
 * - **Timeout**: Wraps a future with a timeout
 * - **Join/Select**: Wait for multiple tasks
 *
 * @see async.c for implementation
 */

#ifndef TML_ASYNC_H
#define TML_ASYNC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

struct TmlTask;
struct TmlExecutor;
struct TmlWaker;
struct TmlContext;

// ============================================================================
// Poll Type (mirrors TML's Poll[T] enum)
// ============================================================================

/** @brief Poll tag value indicating the future completed with a value. */
#define TML_POLL_READY 0

/** @brief Poll tag value indicating the future is not yet complete. */
#define TML_POLL_PENDING 1

/**
 * @brief Generic poll result (tag + 8-byte payload).
 *
 * Mirrors TML's `Poll[T]` enum type. The tag indicates whether the
 * operation completed (Ready) or needs to be polled again (Pending).
 *
 * When Ready, the value union contains the result. When Pending, the
 * value is undefined.
 */
typedef struct TmlPoll {
    int32_t tag;  /**< @brief 0 = Ready, 1 = Pending */
    int32_t _pad; /**< @brief Padding for 8-byte alignment */
    union {
        int64_t i64_value; /**< @brief 64-bit integer result */
        double f64_value;  /**< @brief 64-bit float result */
        void* ptr_value;   /**< @brief Pointer result */
        int32_t i32_value; /**< @brief 32-bit integer result */
        int8_t bytes[8];   /**< @brief Raw byte access */
    } value;
} TmlPoll;

// ============================================================================
// Task Representation
// ============================================================================

/**
 * @brief Poll function signature.
 *
 * All async tasks implement this signature. The function receives
 * the task's state and a context containing the waker.
 *
 * @param state Pointer to the task's state struct.
 * @param cx Context with waker for rescheduling.
 * @return Poll result indicating Ready or Pending.
 */
typedef TmlPoll (*TmlPollFn)(void* state, struct TmlContext* cx);

/**
 * @brief Task execution state.
 */
typedef enum TmlTaskState {
    TML_TASK_PENDING,   /**< @brief Not yet started or suspended */
    TML_TASK_RUNNING,   /**< @brief Currently executing */
    TML_TASK_COMPLETED, /**< @brief Finished with result */
    TML_TASK_FAILED     /**< @brief Panicked or errored */
} TmlTaskState;

/**
 * @brief Represents an async task.
 *
 * A task encapsulates an async operation with its state machine state,
 * poll function, and completion result.
 */
typedef struct TmlTask {
    uint64_t id;             /**< @brief Unique task ID */
    void* state;             /**< @brief State machine struct (heap allocated) */
    size_t state_size;       /**< @brief Size of state struct in bytes */
    TmlPollFn poll_fn;       /**< @brief Pointer to poll function */
    TmlTaskState task_state; /**< @brief Current task execution state */
    TmlPoll result;          /**< @brief Result when completed */
    struct TmlTask* next;    /**< @brief Next pointer for queue linked list */
} TmlTask;

// ============================================================================
// Waker (for waking pending tasks)
// ============================================================================

/**
 * @brief Wake function signature.
 * @param data Opaque data passed to the wake function.
 */
typedef void (*TmlWakeFn)(void* data);

/**
 * @brief Waker for re-scheduling suspended tasks.
 *
 * When a task suspends waiting for I/O or another event, it stores
 * a waker. The event source calls the waker to re-schedule the task.
 */
typedef struct TmlWaker {
    TmlWakeFn wake_fn; /**< @brief Function to call to wake the task */
    void* data;        /**< @brief Data passed to wake function */
    uint64_t task_id;  /**< @brief ID of task to wake */
} TmlWaker;

// ============================================================================
// Context (passed to poll functions)
// ============================================================================

/**
 * @brief Context passed to poll functions.
 *
 * Contains the waker for the current task and a reference to the executor.
 * Tasks use this to register for wake-up when waiting on external events.
 */
typedef struct TmlContext {
    TmlWaker waker;               /**< @brief Waker for this task */
    struct TmlExecutor* executor; /**< @brief Reference to executor */
} TmlContext;

// ============================================================================
// Task Queue (simple linked list)
// ============================================================================

/**
 * @brief Simple linked-list task queue.
 *
 * Used by the executor to maintain ready and pending task lists.
 */
typedef struct TmlTaskQueue {
    TmlTask* head; /**< @brief Head of the queue */
    TmlTask* tail; /**< @brief Tail of the queue */
    size_t count;  /**< @brief Number of tasks in queue */
} TmlTaskQueue;

// ============================================================================
// Executor
// ============================================================================

/**
 * @brief Async task executor.
 *
 * The executor manages task scheduling and execution. It maintains
 * queues of ready and pending tasks, and runs them to completion.
 */
typedef struct TmlExecutor {
    TmlTaskQueue ready_queue;   /**< @brief Tasks ready to run */
    TmlTaskQueue pending_queue; /**< @brief Tasks waiting for wake */
    uint64_t next_task_id;      /**< @brief Task ID counter */
    int32_t running;            /**< @brief Is executor running? */
    TmlTask* current_task;      /**< @brief Currently executing task */
} TmlExecutor;

// ============================================================================
// Executor API
// ============================================================================

/**
 * @brief Creates a new executor.
 * @return Pointer to new executor, or NULL on allocation failure.
 */
TmlExecutor* tml_executor_new(void);

/**
 * @brief Destroys an executor and all its tasks.
 * @param executor The executor to destroy.
 */
void tml_executor_destroy(TmlExecutor* executor);

/**
 * @brief Spawns a new task on the executor.
 *
 * @param executor The executor to spawn on.
 * @param poll_fn The task's poll function.
 * @param initial_state Initial state (will be copied).
 * @param state_size Size of the state in bytes.
 * @return Task ID, or 0 on failure.
 */
uint64_t tml_executor_spawn(TmlExecutor* executor, TmlPollFn poll_fn, void* initial_state,
                            size_t state_size);

/**
 * @brief Runs the executor until all tasks complete.
 * @param executor The executor to run.
 * @return 0 on success, non-zero on error.
 */
int32_t tml_executor_run(TmlExecutor* executor);

/**
 * @brief Runs a single task to completion (block_on semantics).
 *
 * Creates a temporary executor, runs the task until complete, and
 * returns the result.
 *
 * @param poll_fn The task's poll function.
 * @param state The task's state.
 * @param state_size Size of the state in bytes.
 * @return The poll result when complete.
 */
TmlPoll tml_block_on(TmlPollFn poll_fn, void* state, size_t state_size);

/**
 * @brief Polls a single task once.
 * @param executor The executor context.
 * @param task The task to poll.
 * @return 1 if task completed, 0 if pending.
 */
int32_t tml_executor_poll_task(TmlExecutor* executor, TmlTask* task);

/**
 * @brief Wakes a task by ID (makes it ready to run again).
 * @param executor The executor.
 * @param task_id The task ID to wake.
 */
void tml_executor_wake(TmlExecutor* executor, uint64_t task_id);

// ============================================================================
// Task Queue Operations
// ============================================================================

/** @brief Initializes a task queue. */
void tml_queue_init(TmlTaskQueue* queue);

/** @brief Pushes a task to the back of the queue. */
void tml_queue_push(TmlTaskQueue* queue, TmlTask* task);

/** @brief Pops a task from the front of the queue. */
TmlTask* tml_queue_pop(TmlTaskQueue* queue);

/** @brief Removes a task by ID from the queue. */
TmlTask* tml_queue_remove_by_id(TmlTaskQueue* queue, uint64_t task_id);

/** @brief Checks if the queue is empty. */
int32_t tml_queue_is_empty(const TmlTaskQueue* queue);

// ============================================================================
// Waker Operations
// ============================================================================

/**
 * @brief Creates a waker for the specified task.
 * @param executor The executor the task runs on.
 * @param task_id The task ID to wake.
 * @return A new waker.
 */
TmlWaker tml_waker_create(TmlExecutor* executor, uint64_t task_id);

/**
 * @brief Wakes a task using its waker.
 * @param waker The waker to trigger.
 */
void tml_waker_wake(TmlWaker* waker);

/**
 * @brief Clones a waker.
 * @param waker The waker to clone.
 * @return A copy of the waker.
 */
TmlWaker tml_waker_clone(const TmlWaker* waker);

/**
 * @brief Destroys a waker and frees associated memory.
 * @param waker The waker to destroy.
 *
 * This decrements the reference count on the waker's internal data.
 * When the count reaches zero, the data is freed.
 */
void tml_waker_destroy(TmlWaker* waker);

// ============================================================================
// Poll Result Utilities
// ============================================================================

/**
 * @brief Creates a Ready poll result with an i64 value.
 * @param value The result value.
 * @return A Ready poll result.
 */
TmlPoll tml_poll_ready_i64(int64_t value);

/**
 * @brief Creates a Ready poll result with a pointer value.
 * @param value The result pointer.
 * @return A Ready poll result.
 */
TmlPoll tml_poll_ready_ptr(void* value);

/**
 * @brief Creates a Pending poll result.
 * @return A Pending poll result.
 */
TmlPoll tml_poll_pending(void);

/**
 * @brief Checks if a poll result is Ready.
 * @param poll The poll result to check.
 * @return 1 if Ready, 0 if Pending.
 */
int32_t tml_poll_is_ready(const TmlPoll* poll);

/**
 * @brief Checks if a poll result is Pending.
 * @param poll The poll result to check.
 * @return 1 if Pending, 0 if Ready.
 */
int32_t tml_poll_is_pending(const TmlPoll* poll);

// ============================================================================
// Async I/O Primitives
// ============================================================================

/**
 * @brief Timer future state.
 *
 * Used to implement async sleep/delay operations.
 */
typedef struct TmlTimerState {
    int64_t start_time_ms; /**< @brief When timer started */
    int64_t duration_ms;   /**< @brief How long to wait */
    int32_t started;       /**< @brief Has timer been started? */
} TmlTimerState;

/**
 * @brief Creates a new timer state.
 * @param duration_ms The duration to wait in milliseconds.
 * @return Initialized timer state.
 */
TmlTimerState tml_timer_new(int64_t duration_ms);

/**
 * @brief Polls a sleep timer.
 *
 * First poll starts the timer. Subsequent polls check if elapsed.
 *
 * @param state The timer state.
 * @param cx The context (unused currently).
 * @return Ready(Unit) when complete, Pending otherwise.
 */
TmlPoll tml_sleep_poll(TmlTimerState* state, TmlContext* cx);

/**
 * @brief Polls a delay timer (alias for sleep).
 */
TmlPoll tml_delay_poll(TmlTimerState* state, TmlContext* cx);

/**
 * @brief Yield state for yielding to other tasks.
 */
typedef struct TmlYieldState {
    int32_t yielded; /**< @brief Has this yield occurred? */
} TmlYieldState;

/**
 * @brief Polls a yield operation.
 *
 * Returns Pending once, then Ready on subsequent polls.
 * Used to yield control to other tasks.
 *
 * @param state The yield state.
 * @param cx The context.
 * @return Pending first time, Ready after.
 */
TmlPoll tml_yield_poll(TmlYieldState* state, TmlContext* cx);

// ============================================================================
// Async Synchronization Primitives
// ============================================================================

/**
 * @brief Bounded single-producer single-consumer async channel.
 *
 * Provides async communication between tasks with a fixed-size buffer.
 */
typedef struct TmlChannel {
    void* buffer;               /**< @brief Circular buffer for values */
    size_t capacity;            /**< @brief Buffer capacity */
    size_t item_size;           /**< @brief Size of each item in bytes */
    size_t head;                /**< @brief Read position */
    size_t tail;                /**< @brief Write position */
    size_t count;               /**< @brief Number of items in buffer */
    TmlWaker* pending_sender;   /**< @brief Waker for blocked sender */
    TmlWaker* pending_receiver; /**< @brief Waker for blocked receiver */
    int32_t closed;             /**< @brief Is channel closed? */
} TmlChannel;

/**
 * @brief Creates a new channel.
 * @param capacity Maximum number of items.
 * @param item_size Size of each item in bytes.
 * @return New channel, or NULL on failure.
 */
TmlChannel* tml_channel_new(size_t capacity, size_t item_size);

/** @brief Destroys a channel. */
void tml_channel_destroy(TmlChannel* channel);

/**
 * @brief Tries to send a value (non-blocking).
 * @param channel The channel.
 * @param value Pointer to the value to send.
 * @return 1 if sent, 0 if would block, -1 if closed.
 */
int32_t tml_channel_try_send(TmlChannel* channel, const void* value);

/**
 * @brief Tries to receive a value (non-blocking).
 * @param channel The channel.
 * @param value_out Pointer to store the received value.
 * @return 1 if received, 0 if would block, -1 if closed and empty.
 */
int32_t tml_channel_try_recv(TmlChannel* channel, void* value_out);

/** @brief Closes a channel, waking all waiters. */
void tml_channel_close(TmlChannel* channel);

/** @brief Checks if channel is empty. */
int32_t tml_channel_is_empty(const TmlChannel* channel);

/** @brief Checks if channel is full. */
int32_t tml_channel_is_full(const TmlChannel* channel);

// ============================================================================
// Spawn, Join, Select Primitives
// ============================================================================

/**
 * @brief Handle to a spawned task.
 *
 * Used to await or join on spawned tasks.
 */
typedef struct TmlTaskHandle {
    uint64_t task_id;      /**< @brief ID of the spawned task */
    TmlExecutor* executor; /**< @brief Executor the task runs on */
    int32_t completed;     /**< @brief Has task completed? */
    TmlPoll result;        /**< @brief Result when completed */
} TmlTaskHandle;

/**
 * @brief Spawns a new task and returns a handle.
 *
 * @param executor The executor to spawn on.
 * @param poll_fn The task's poll function.
 * @param initial_state Initial state.
 * @param state_size Size of state.
 * @return Handle to the spawned task.
 */
TmlTaskHandle tml_spawn(TmlExecutor* executor, TmlPollFn poll_fn, void* initial_state,
                        size_t state_size);

/**
 * @brief Polls a task handle to check if complete.
 * @return Ready(result) or Pending.
 */
TmlPoll tml_join_poll(TmlTaskHandle* handle, TmlContext* cx);

/**
 * @brief State for joining multiple tasks.
 */
typedef struct TmlJoinAllState {
    TmlTaskHandle* handles; /**< @brief Array of task handles */
    size_t count;           /**< @brief Number of handles */
    size_t completed_count; /**< @brief How many have completed */
    TmlPoll* results;       /**< @brief Array of results */
} TmlJoinAllState;

/** @brief Creates join_all state for multiple tasks. */
TmlJoinAllState* tml_join_all_new(TmlTaskHandle* handles, size_t count);

/** @brief Destroys join_all state. */
void tml_join_all_destroy(TmlJoinAllState* state);

/**
 * @brief Polls join_all.
 * @return Ready when all tasks complete.
 */
TmlPoll tml_join_all_poll(TmlJoinAllState* state, TmlContext* cx);

/**
 * @brief State for selecting first completed task.
 */
typedef struct TmlSelectState {
    TmlTaskHandle* handles; /**< @brief Array of task handles */
    size_t count;           /**< @brief Number of handles */
    size_t winner_index;    /**< @brief Index of first completed task */
    int32_t found_winner;   /**< @brief Has a winner been found? */
} TmlSelectState;

/** @brief Creates select state. */
TmlSelectState* tml_select_new(TmlTaskHandle* handles, size_t count);

/** @brief Destroys select state. */
void tml_select_destroy(TmlSelectState* state);

/**
 * @brief Polls select.
 * @return Ready(index) when first task completes.
 */
TmlPoll tml_select_poll(TmlSelectState* state, TmlContext* cx);

/**
 * @brief Polls race (like select but returns the value).
 * @return Ready(result) of first completed task.
 */
TmlPoll tml_race_poll(TmlSelectState* state, TmlContext* cx);

// ============================================================================
// Timeout Wrapper
// ============================================================================

/**
 * @brief State for timeout-wrapped futures.
 */
typedef struct TmlTimeoutState {
    TmlPollFn inner_poll; /**< @brief Inner future's poll function */
    void* inner_state;    /**< @brief Inner future's state */
    TmlTimerState timer;  /**< @brief Timeout timer */
    int32_t timed_out;    /**< @brief Did we time out? */
} TmlTimeoutState;

/**
 * @brief Creates timeout wrapper state.
 * @param inner_poll The inner future's poll function.
 * @param inner_state The inner future's state.
 * @param timeout_ms Timeout in milliseconds.
 * @return Initialized timeout state.
 */
TmlTimeoutState tml_timeout_new(TmlPollFn inner_poll, void* inner_state, int64_t timeout_ms);

/**
 * @brief Polls a timeout-wrapped future.
 * @return Ready(result) if inner completes, Ready(-1) on timeout.
 */
TmlPoll tml_timeout_poll(TmlTimeoutState* state, TmlContext* cx);

#ifdef __cplusplus
}
#endif

#endif // TML_ASYNC_H
