// TML Runtime - Async Executor Header
// Provides task scheduling and execution for async/await

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

// Poll tag values
#define TML_POLL_READY 0
#define TML_POLL_PENDING 1

// Generic poll result (tag + 8-byte payload)
typedef struct TmlPoll {
    int32_t tag;  // 0 = Ready, 1 = Pending
    int32_t _pad; // Padding for alignment
    union {
        int64_t i64_value;
        double f64_value;
        void* ptr_value;
        int32_t i32_value;
        int8_t bytes[8];
    } value;
} TmlPoll;

// ============================================================================
// Task Representation
// ============================================================================

// Poll function signature: takes state pointer and context, returns Poll
typedef TmlPoll (*TmlPollFn)(void* state, struct TmlContext* cx);

// Task state
typedef enum TmlTaskState {
    TML_TASK_PENDING,   // Not yet started or suspended
    TML_TASK_RUNNING,   // Currently executing
    TML_TASK_COMPLETED, // Finished with result
    TML_TASK_FAILED     // Panicked or errored
} TmlTaskState;

// Task structure
typedef struct TmlTask {
    uint64_t id;             // Unique task ID
    void* state;             // State machine struct (heap allocated)
    size_t state_size;       // Size of state struct
    TmlPollFn poll_fn;       // Pointer to poll function
    TmlTaskState task_state; // Current task state
    TmlPoll result;          // Result when completed
    struct TmlTask* next;    // For linked list in queue
} TmlTask;

// ============================================================================
// Waker (for waking pending tasks)
// ============================================================================

typedef void (*TmlWakeFn)(void* data);

typedef struct TmlWaker {
    TmlWakeFn wake_fn; // Function to call to wake
    void* data;        // Data passed to wake function
    uint64_t task_id;  // ID of task to wake
} TmlWaker;

// ============================================================================
// Context (passed to poll functions)
// ============================================================================

typedef struct TmlContext {
    TmlWaker waker;               // Waker for this task
    struct TmlExecutor* executor; // Reference to executor
} TmlContext;

// ============================================================================
// Task Queue (simple linked list)
// ============================================================================

typedef struct TmlTaskQueue {
    TmlTask* head;
    TmlTask* tail;
    size_t count;
} TmlTaskQueue;

// ============================================================================
// Executor
// ============================================================================

typedef struct TmlExecutor {
    TmlTaskQueue ready_queue;   // Tasks ready to run
    TmlTaskQueue pending_queue; // Tasks waiting for wake
    uint64_t next_task_id;      // Task ID counter
    int32_t running;            // Is executor running?
    TmlTask* current_task;      // Currently executing task
} TmlExecutor;

// ============================================================================
// Executor API
// ============================================================================

// Create a new executor
TmlExecutor* tml_executor_new(void);

// Destroy executor and all tasks
void tml_executor_destroy(TmlExecutor* executor);

// Spawn a new task on the executor
// Returns task ID
uint64_t tml_executor_spawn(TmlExecutor* executor, TmlPollFn poll_fn, void* initial_state,
                            size_t state_size);

// Run the executor until all tasks complete
// Returns 0 on success, non-zero on error
int32_t tml_executor_run(TmlExecutor* executor);

// Run a single task to completion (block_on semantics)
// Creates a temporary executor, runs the task, returns result
TmlPoll tml_block_on(TmlPollFn poll_fn, void* state, size_t state_size);

// Poll a single task once
// Returns 1 if task completed, 0 if pending
int32_t tml_executor_poll_task(TmlExecutor* executor, TmlTask* task);

// Wake a task by ID (makes it ready to run again)
void tml_executor_wake(TmlExecutor* executor, uint64_t task_id);

// ============================================================================
// Task Queue Operations
// ============================================================================

void tml_queue_init(TmlTaskQueue* queue);
void tml_queue_push(TmlTaskQueue* queue, TmlTask* task);
TmlTask* tml_queue_pop(TmlTaskQueue* queue);
TmlTask* tml_queue_remove_by_id(TmlTaskQueue* queue, uint64_t task_id);
int32_t tml_queue_is_empty(const TmlTaskQueue* queue);

// ============================================================================
// Waker Operations
// ============================================================================

// Create a waker for the current task
TmlWaker tml_waker_create(TmlExecutor* executor, uint64_t task_id);

// Wake using a waker
void tml_waker_wake(TmlWaker* waker);

// Clone a waker (just copies the struct for now)
TmlWaker tml_waker_clone(const TmlWaker* waker);

// ============================================================================
// Utility Functions
// ============================================================================

// Create a Ready poll result with i64 value
TmlPoll tml_poll_ready_i64(int64_t value);

// Create a Ready poll result with pointer value
TmlPoll tml_poll_ready_ptr(void* value);

// Create a Pending poll result
TmlPoll tml_poll_pending(void);

// Check if poll is ready
int32_t tml_poll_is_ready(const TmlPoll* poll);

// Check if poll is pending
int32_t tml_poll_is_pending(const TmlPoll* poll);

// ============================================================================
// Async I/O Primitives
// ============================================================================

// Timer future state
typedef struct TmlTimerState {
    int64_t start_time_ms; // When timer started
    int64_t duration_ms;   // How long to wait
    int32_t started;       // Has timer been started?
} TmlTimerState;

// Sleep asynchronously (returns Poll[Unit])
// First poll starts the timer, subsequent polls check if elapsed
TmlPoll tml_sleep_poll(TmlTimerState* state, TmlContext* cx);

// Create a timer state for sleeping
TmlTimerState tml_timer_new(int64_t duration_ms);

// Delay future - similar to sleep but for generic use
TmlPoll tml_delay_poll(TmlTimerState* state, TmlContext* cx);

// Yield to other tasks (returns immediately pending once, then ready)
typedef struct TmlYieldState {
    int32_t yielded;
} TmlYieldState;

TmlPoll tml_yield_poll(TmlYieldState* state, TmlContext* cx);

// ============================================================================
// Async Synchronization Primitives
// ============================================================================

// Simple async channel (single-producer single-consumer)
typedef struct TmlChannel {
    void* buffer;               // Circular buffer for values
    size_t capacity;            // Buffer capacity
    size_t item_size;           // Size of each item
    size_t head;                // Read position
    size_t tail;                // Write position
    size_t count;               // Number of items in buffer
    TmlWaker* pending_sender;   // Waker for blocked sender
    TmlWaker* pending_receiver; // Waker for blocked receiver
    int32_t closed;             // Is channel closed?
} TmlChannel;

// Create a channel with given capacity and item size
TmlChannel* tml_channel_new(size_t capacity, size_t item_size);

// Destroy a channel
void tml_channel_destroy(TmlChannel* channel);

// Try to send a value (non-blocking)
// Returns 1 if sent, 0 if would block, -1 if closed
int32_t tml_channel_try_send(TmlChannel* channel, const void* value);

// Try to receive a value (non-blocking)
// Returns 1 if received, 0 if would block, -1 if closed
int32_t tml_channel_try_recv(TmlChannel* channel, void* value_out);

// Close a channel
void tml_channel_close(TmlChannel* channel);

// Check if channel is empty
int32_t tml_channel_is_empty(const TmlChannel* channel);

// Check if channel is full
int32_t tml_channel_is_full(const TmlChannel* channel);

// ============================================================================
// Spawn, Join, Select Primitives
// ============================================================================

// Task handle for spawned tasks
typedef struct TmlTaskHandle {
    uint64_t task_id;      // ID of the spawned task
    TmlExecutor* executor; // Executor the task runs on
    int32_t completed;     // Has task completed?
    TmlPoll result;        // Result when completed
} TmlTaskHandle;

// Spawn a new task and return a handle
// The poll_fn takes (state, context) and returns Poll
TmlTaskHandle tml_spawn(TmlExecutor* executor, TmlPollFn poll_fn, void* initial_state,
                        size_t state_size);

// Poll a task handle to check if complete
// Returns Ready(result) or Pending
TmlPoll tml_join_poll(TmlTaskHandle* handle, TmlContext* cx);

// Join all state for waiting on multiple tasks
typedef struct TmlJoinAllState {
    TmlTaskHandle* handles; // Array of task handles
    size_t count;           // Number of handles
    size_t completed_count; // How many have completed
    TmlPoll* results;       // Array of results
} TmlJoinAllState;

// Create join_all state
TmlJoinAllState* tml_join_all_new(TmlTaskHandle* handles, size_t count);

// Destroy join_all state
void tml_join_all_destroy(TmlJoinAllState* state);

// Poll join_all - returns Ready when all tasks complete
TmlPoll tml_join_all_poll(TmlJoinAllState* state, TmlContext* cx);

// Select state for waiting on first task to complete
typedef struct TmlSelectState {
    TmlTaskHandle* handles; // Array of task handles
    size_t count;           // Number of handles
    size_t winner_index;    // Index of first completed task
    int32_t found_winner;   // Has a winner been found?
} TmlSelectState;

// Create select state
TmlSelectState* tml_select_new(TmlTaskHandle* handles, size_t count);

// Destroy select state
void tml_select_destroy(TmlSelectState* state);

// Poll select - returns Ready(index, result) when first task completes
// Result is packed as: lower 32 bits = index, upper bits = result type
TmlPoll tml_select_poll(TmlSelectState* state, TmlContext* cx);

// Race multiple futures - returns result of first to complete
// Similar to select but discards index
TmlPoll tml_race_poll(TmlSelectState* state, TmlContext* cx);

// ============================================================================
// Timeout Wrapper
// ============================================================================

typedef struct TmlTimeoutState {
    TmlPollFn inner_poll; // Inner future's poll function
    void* inner_state;    // Inner future's state
    TmlTimerState timer;  // Timeout timer
    int32_t timed_out;    // Did we time out?
} TmlTimeoutState;

// Create timeout wrapper state
TmlTimeoutState tml_timeout_new(TmlPollFn inner_poll, void* inner_state, int64_t timeout_ms);

// Poll timeout - returns Ready(result) or Ready(timeout_error)
TmlPoll tml_timeout_poll(TmlTimeoutState* state, TmlContext* cx);

#ifdef __cplusplus
}
#endif

#endif // TML_ASYNC_H
