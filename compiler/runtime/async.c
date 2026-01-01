// TML Runtime - Async Executor Implementation
// Provides task scheduling and execution for async/await

#include "async.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Task Queue Implementation
// ============================================================================

void tml_queue_init(TmlTaskQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

void tml_queue_push(TmlTaskQueue* queue, TmlTask* task) {
    task->next = NULL;
    if (queue->tail) {
        queue->tail->next = task;
        queue->tail = task;
    } else {
        queue->head = task;
        queue->tail = task;
    }
    queue->count++;
}

TmlTask* tml_queue_pop(TmlTaskQueue* queue) {
    if (!queue->head) {
        return NULL;
    }
    TmlTask* task = queue->head;
    queue->head = task->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    task->next = NULL;
    queue->count--;
    return task;
}

TmlTask* tml_queue_remove_by_id(TmlTaskQueue* queue, uint64_t task_id) {
    TmlTask* prev = NULL;
    TmlTask* current = queue->head;

    while (current) {
        if (current->id == task_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                queue->head = current->next;
            }
            if (current == queue->tail) {
                queue->tail = prev;
            }
            current->next = NULL;
            queue->count--;
            return current;
        }
        prev = current;
        current = current->next;
    }
    return NULL;
}

int32_t tml_queue_is_empty(const TmlTaskQueue* queue) {
    return queue->head == NULL;
}

// ============================================================================
// Poll Result Utilities
// ============================================================================

TmlPoll tml_poll_ready_i64(int64_t value) {
    TmlPoll poll;
    poll.tag = TML_POLL_READY;
    poll._pad = 0;
    poll.value.i64_value = value;
    return poll;
}

TmlPoll tml_poll_ready_ptr(void* value) {
    TmlPoll poll;
    poll.tag = TML_POLL_READY;
    poll._pad = 0;
    poll.value.ptr_value = value;
    return poll;
}

TmlPoll tml_poll_pending(void) {
    TmlPoll poll;
    poll.tag = TML_POLL_PENDING;
    poll._pad = 0;
    poll.value.i64_value = 0;
    return poll;
}

int32_t tml_poll_is_ready(const TmlPoll* poll) {
    return poll->tag == TML_POLL_READY;
}

int32_t tml_poll_is_pending(const TmlPoll* poll) {
    return poll->tag == TML_POLL_PENDING;
}

// ============================================================================
// Waker Implementation
// ============================================================================

// Internal wake function that wakes a task on the executor
static void tml_internal_wake(void* data) {
    // data contains packed executor pointer and task_id
    // For simplicity, we use a struct
    struct WakeData {
        TmlExecutor* executor;
        uint64_t task_id;
    }* wake_data = (struct WakeData*)data;

    if (wake_data && wake_data->executor) {
        tml_executor_wake(wake_data->executor, wake_data->task_id);
    }
}

TmlWaker tml_waker_create(TmlExecutor* executor, uint64_t task_id) {
    TmlWaker waker;
    waker.wake_fn = tml_internal_wake;
    // Allocate wake data (leaked for now, proper impl would ref-count)
    struct WakeData {
        TmlExecutor* executor;
        uint64_t task_id;
    }* data = (struct WakeData*)malloc(sizeof(struct WakeData));
    data->executor = executor;
    data->task_id = task_id;
    waker.data = data;
    waker.task_id = task_id;
    return waker;
}

void tml_waker_wake(TmlWaker* waker) {
    if (waker && waker->wake_fn) {
        waker->wake_fn(waker->data);
    }
}

TmlWaker tml_waker_clone(const TmlWaker* waker) {
    TmlWaker clone;
    clone.wake_fn = waker->wake_fn;
    clone.data = waker->data; // Shallow copy - proper impl would ref-count
    clone.task_id = waker->task_id;
    return clone;
}

// ============================================================================
// Executor Implementation
// ============================================================================

TmlExecutor* tml_executor_new(void) {
    TmlExecutor* executor = (TmlExecutor*)malloc(sizeof(TmlExecutor));
    if (!executor) {
        return NULL;
    }

    tml_queue_init(&executor->ready_queue);
    tml_queue_init(&executor->pending_queue);
    executor->next_task_id = 1;
    executor->running = 0;
    executor->current_task = NULL;

    return executor;
}

void tml_executor_destroy(TmlExecutor* executor) {
    if (!executor) {
        return;
    }

    // Free all tasks in ready queue
    TmlTask* task;
    while ((task = tml_queue_pop(&executor->ready_queue)) != NULL) {
        if (task->state) {
            free(task->state);
        }
        free(task);
    }

    // Free all tasks in pending queue
    while ((task = tml_queue_pop(&executor->pending_queue)) != NULL) {
        if (task->state) {
            free(task->state);
        }
        free(task);
    }

    free(executor);
}

uint64_t tml_executor_spawn(TmlExecutor* executor, TmlPollFn poll_fn, void* initial_state,
                            size_t state_size) {
    if (!executor || !poll_fn) {
        return 0;
    }

    TmlTask* task = (TmlTask*)malloc(sizeof(TmlTask));
    if (!task) {
        return 0;
    }

    task->id = executor->next_task_id++;
    task->poll_fn = poll_fn;
    task->task_state = TML_TASK_PENDING;
    task->result = tml_poll_pending();
    task->next = NULL;

    // Copy state if provided
    if (initial_state && state_size > 0) {
        task->state = malloc(state_size);
        if (!task->state) {
            free(task);
            return 0;
        }
        memcpy(task->state, initial_state, state_size);
        task->state_size = state_size;
    } else {
        task->state = NULL;
        task->state_size = 0;
    }

    // Add to ready queue
    tml_queue_push(&executor->ready_queue, task);

    return task->id;
}

void tml_executor_wake(TmlExecutor* executor, uint64_t task_id) {
    if (!executor) {
        return;
    }

    // Find task in pending queue and move to ready queue
    TmlTask* task = tml_queue_remove_by_id(&executor->pending_queue, task_id);
    if (task) {
        task->task_state = TML_TASK_PENDING;
        tml_queue_push(&executor->ready_queue, task);
    }
}

int32_t tml_executor_poll_task(TmlExecutor* executor, TmlTask* task) {
    if (!executor || !task || !task->poll_fn) {
        return 1; // Treat as completed (error)
    }

    // Create context for this poll
    TmlContext cx;
    cx.waker = tml_waker_create(executor, task->id);
    cx.executor = executor;

    // Mark task as running
    task->task_state = TML_TASK_RUNNING;
    executor->current_task = task;

    // Call the poll function
    TmlPoll result = task->poll_fn(task->state, &cx);

    executor->current_task = NULL;

    if (tml_poll_is_ready(&result)) {
        // Task completed
        task->task_state = TML_TASK_COMPLETED;
        task->result = result;
        return 1;
    } else {
        // Task is pending - move to pending queue
        task->task_state = TML_TASK_PENDING;
        tml_queue_push(&executor->pending_queue, task);
        return 0;
    }
}

int32_t tml_executor_run(TmlExecutor* executor) {
    if (!executor) {
        return -1;
    }

    executor->running = 1;

    // Run until no more tasks
    while (executor->running) {
        // Get next ready task
        TmlTask* task = tml_queue_pop(&executor->ready_queue);

        if (!task) {
            // No ready tasks - check if there are pending tasks
            if (tml_queue_is_empty(&executor->pending_queue)) {
                // All done
                break;
            }

            // In a real executor, we'd wait for I/O or a waker.
            // For now, if there are pending tasks but none ready,
            // we have a deadlock (no external wake source).
            // In this simple model, we just return.
            break;
        }

        // Poll the task
        if (tml_executor_poll_task(executor, task)) {
            // Task completed - free it
            if (task->state) {
                free(task->state);
            }
            free(task);
        }
        // If not completed, poll_task already moved it to pending queue
    }

    executor->running = 0;
    return 0;
}

// ============================================================================
// block_on Implementation
// ============================================================================

TmlPoll tml_block_on(TmlPollFn poll_fn, void* state, size_t state_size) {
    // Create a temporary executor
    TmlExecutor* executor = tml_executor_new();
    if (!executor) {
        return tml_poll_pending(); // Error - return pending
    }

    // Create the task
    TmlTask task;
    task.id = 1;
    task.poll_fn = poll_fn;
    task.task_state = TML_TASK_PENDING;
    task.result = tml_poll_pending();
    task.next = NULL;

    // Copy state if provided
    if (state && state_size > 0) {
        task.state = malloc(state_size);
        if (!task.state) {
            tml_executor_destroy(executor);
            return tml_poll_pending();
        }
        memcpy(task.state, state, state_size);
        task.state_size = state_size;
    } else {
        task.state = NULL;
        task.state_size = 0;
    }

    // Create context
    TmlContext cx;
    cx.waker = tml_waker_create(executor, task.id);
    cx.executor = executor;

    TmlPoll result;

    // Poll until ready (simple busy-wait for synchronous futures)
    int max_iterations = 10000; // Prevent infinite loops
    int iterations = 0;

    while (iterations < max_iterations) {
        result = poll_fn(task.state, &cx);

        if (tml_poll_is_ready(&result)) {
            break;
        }

        iterations++;

        // In synchronous model, if still pending after first poll,
        // something is wrong (no external wake source)
        // For now, just retry a few times then give up
        if (iterations > 10) {
            // Deadlock - return what we have
            break;
        }
    }

    // Cleanup
    if (task.state) {
        free(task.state);
    }
    tml_executor_destroy(executor);

    return result;
}

// ============================================================================
// Simple block_on for synchronous async functions (current model)
// This version doesn't use the full executor - just calls poll once
// since our async functions always return Ready immediately
// ============================================================================

// Extract value from Poll struct (for use by LLVM IR)
// This is what block_on currently does - just extract the Ready value
int64_t tml_block_on_simple_i64(void* poll_ptr) {
    TmlPoll* poll = (TmlPoll*)poll_ptr;
    return poll->value.i64_value;
}

int32_t tml_block_on_simple_i32(void* poll_ptr) {
    TmlPoll* poll = (TmlPoll*)poll_ptr;
    return poll->value.i32_value;
}

double tml_block_on_simple_f64(void* poll_ptr) {
    TmlPoll* poll = (TmlPoll*)poll_ptr;
    return poll->value.f64_value;
}

void* tml_block_on_simple_ptr(void* poll_ptr) {
    TmlPoll* poll = (TmlPoll*)poll_ptr;
    return poll->value.ptr_value;
}

// ============================================================================
// Async I/O Primitives Implementation
// ============================================================================

// Platform-specific time function
#ifdef _WIN32
#include <windows.h>
static int64_t get_time_ms(void) {
    return (int64_t)GetTickCount64();
}
#else
#include <time.h>
static int64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

TmlTimerState tml_timer_new(int64_t duration_ms) {
    TmlTimerState state;
    state.start_time_ms = 0;
    state.duration_ms = duration_ms;
    state.started = 0;
    return state;
}

TmlPoll tml_sleep_poll(TmlTimerState* state, TmlContext* cx) {
    (void)cx; // Unused for now

    if (!state->started) {
        // First poll - start the timer
        state->start_time_ms = get_time_ms();
        state->started = 1;
    }

    // Check if timer has elapsed
    int64_t now = get_time_ms();
    int64_t elapsed = now - state->start_time_ms;

    if (elapsed >= state->duration_ms) {
        // Timer complete - return Ready with unit value
        return tml_poll_ready_i64(0);
    }

    // Still waiting - return Pending
    // In a real executor, we'd register with a timer wheel here
    return tml_poll_pending();
}

TmlPoll tml_delay_poll(TmlTimerState* state, TmlContext* cx) {
    // Same as sleep for now
    return tml_sleep_poll(state, cx);
}

TmlPoll tml_yield_poll(TmlYieldState* state, TmlContext* cx) {
    (void)cx;

    if (!state->yielded) {
        // First poll - yield once
        state->yielded = 1;
        return tml_poll_pending();
    }

    // Second poll - ready
    return tml_poll_ready_i64(0);
}

// ============================================================================
// Channel Implementation
// ============================================================================

TmlChannel* tml_channel_new(size_t capacity, size_t item_size) {
    if (capacity == 0 || item_size == 0) {
        return NULL;
    }

    TmlChannel* channel = (TmlChannel*)malloc(sizeof(TmlChannel));
    if (!channel) {
        return NULL;
    }

    channel->buffer = malloc(capacity * item_size);
    if (!channel->buffer) {
        free(channel);
        return NULL;
    }

    channel->capacity = capacity;
    channel->item_size = item_size;
    channel->head = 0;
    channel->tail = 0;
    channel->count = 0;
    channel->pending_sender = NULL;
    channel->pending_receiver = NULL;
    channel->closed = 0;

    return channel;
}

void tml_channel_destroy(TmlChannel* channel) {
    if (!channel) {
        return;
    }

    if (channel->buffer) {
        free(channel->buffer);
    }
    free(channel);
}

int32_t tml_channel_try_send(TmlChannel* channel, const void* value) {
    if (!channel || !value) {
        return -1;
    }

    if (channel->closed) {
        return -1; // Channel closed
    }

    if (channel->count >= channel->capacity) {
        return 0; // Would block (buffer full)
    }

    // Copy value to buffer
    char* dest = (char*)channel->buffer + (channel->tail * channel->item_size);
    memcpy(dest, value, channel->item_size);

    channel->tail = (channel->tail + 1) % channel->capacity;
    channel->count++;

    // Wake any pending receiver
    if (channel->pending_receiver) {
        tml_waker_wake(channel->pending_receiver);
        channel->pending_receiver = NULL;
    }

    return 1; // Sent successfully
}

int32_t tml_channel_try_recv(TmlChannel* channel, void* value_out) {
    if (!channel || !value_out) {
        return -1;
    }

    if (channel->count == 0) {
        if (channel->closed) {
            return -1; // Channel closed and empty
        }
        return 0; // Would block (buffer empty)
    }

    // Copy value from buffer
    char* src = (char*)channel->buffer + (channel->head * channel->item_size);
    memcpy(value_out, src, channel->item_size);

    channel->head = (channel->head + 1) % channel->capacity;
    channel->count--;

    // Wake any pending sender
    if (channel->pending_sender) {
        tml_waker_wake(channel->pending_sender);
        channel->pending_sender = NULL;
    }

    return 1; // Received successfully
}

void tml_channel_close(TmlChannel* channel) {
    if (!channel) {
        return;
    }

    channel->closed = 1;

    // Wake any pending waiters
    if (channel->pending_sender) {
        tml_waker_wake(channel->pending_sender);
        channel->pending_sender = NULL;
    }
    if (channel->pending_receiver) {
        tml_waker_wake(channel->pending_receiver);
        channel->pending_receiver = NULL;
    }
}

int32_t tml_channel_is_empty(const TmlChannel* channel) {
    return channel ? (channel->count == 0) : 1;
}

int32_t tml_channel_is_full(const TmlChannel* channel) {
    return channel ? (channel->count >= channel->capacity) : 1;
}

// ============================================================================
// Spawn, Join, Select Implementation
// ============================================================================

TmlTaskHandle tml_spawn(TmlExecutor* executor, TmlPollFn poll_fn, void* initial_state,
                        size_t state_size) {
    TmlTaskHandle handle;
    handle.executor = executor;
    handle.completed = 0;
    handle.result = tml_poll_pending();

    if (executor && poll_fn) {
        handle.task_id = tml_executor_spawn(executor, poll_fn, initial_state, state_size);
    } else {
        handle.task_id = 0;
    }

    return handle;
}

// Internal helper to find task by ID
static TmlTask* find_task_by_id(TmlExecutor* executor, uint64_t task_id) {
    if (!executor)
        return NULL;

    // Search ready queue
    TmlTask* task = executor->ready_queue.head;
    while (task) {
        if (task->id == task_id)
            return task;
        task = task->next;
    }

    // Search pending queue
    task = executor->pending_queue.head;
    while (task) {
        if (task->id == task_id)
            return task;
        task = task->next;
    }

    return NULL;
}

TmlPoll tml_join_poll(TmlTaskHandle* handle, TmlContext* cx) {
    (void)cx;

    if (!handle || handle->task_id == 0) {
        return tml_poll_ready_i64(0); // Invalid handle, return ready
    }

    if (handle->completed) {
        return handle->result; // Already completed
    }

    // Find the task
    TmlTask* task = find_task_by_id(handle->executor, handle->task_id);

    if (!task) {
        // Task not found - might have completed and been freed
        // Check if we stored the result
        if (handle->completed) {
            return handle->result;
        }
        // Assume completed with default value
        handle->completed = 1;
        handle->result = tml_poll_ready_i64(0);
        return handle->result;
    }

    if (task->task_state == TML_TASK_COMPLETED) {
        handle->completed = 1;
        handle->result = task->result;
        return handle->result;
    }

    // Still pending
    return tml_poll_pending();
}

TmlJoinAllState* tml_join_all_new(TmlTaskHandle* handles, size_t count) {
    if (!handles || count == 0) {
        return NULL;
    }

    TmlJoinAllState* state = (TmlJoinAllState*)malloc(sizeof(TmlJoinAllState));
    if (!state)
        return NULL;

    state->handles = handles; // Reference, not copy
    state->count = count;
    state->completed_count = 0;
    state->results = (TmlPoll*)malloc(count * sizeof(TmlPoll));

    if (!state->results) {
        free(state);
        return NULL;
    }

    // Initialize results
    for (size_t i = 0; i < count; i++) {
        state->results[i] = tml_poll_pending();
    }

    return state;
}

void tml_join_all_destroy(TmlJoinAllState* state) {
    if (!state)
        return;

    if (state->results) {
        free(state->results);
    }
    free(state);
}

TmlPoll tml_join_all_poll(TmlJoinAllState* state, TmlContext* cx) {
    if (!state) {
        return tml_poll_ready_i64(0);
    }

    // Check each handle
    size_t completed = 0;
    for (size_t i = 0; i < state->count; i++) {
        if (!state->handles[i].completed) {
            TmlPoll result = tml_join_poll(&state->handles[i], cx);
            if (tml_poll_is_ready(&result)) {
                state->results[i] = result;
                state->handles[i].completed = 1;
            }
        }

        if (state->handles[i].completed) {
            completed++;
        }
    }

    state->completed_count = completed;

    if (completed == state->count) {
        // All completed - return Ready with pointer to results array
        return tml_poll_ready_ptr(state->results);
    }

    return tml_poll_pending();
}

TmlSelectState* tml_select_new(TmlTaskHandle* handles, size_t count) {
    if (!handles || count == 0) {
        return NULL;
    }

    TmlSelectState* state = (TmlSelectState*)malloc(sizeof(TmlSelectState));
    if (!state)
        return NULL;

    state->handles = handles; // Reference, not copy
    state->count = count;
    state->winner_index = 0;
    state->found_winner = 0;

    return state;
}

void tml_select_destroy(TmlSelectState* state) {
    if (state) {
        free(state);
    }
}

TmlPoll tml_select_poll(TmlSelectState* state, TmlContext* cx) {
    if (!state) {
        return tml_poll_ready_i64(0);
    }

    if (state->found_winner) {
        // Already found winner
        TmlPoll result;
        result.tag = TML_POLL_READY;
        result._pad = 0;
        // Pack index in lower 32 bits
        result.value.i64_value = (int64_t)state->winner_index;
        return result;
    }

    // Check each handle for completion
    for (size_t i = 0; i < state->count; i++) {
        TmlPoll result = tml_join_poll(&state->handles[i], cx);
        if (tml_poll_is_ready(&result)) {
            state->winner_index = i;
            state->found_winner = 1;

            // Return index as the result
            TmlPoll select_result;
            select_result.tag = TML_POLL_READY;
            select_result._pad = 0;
            select_result.value.i64_value = (int64_t)i;
            return select_result;
        }
    }

    return tml_poll_pending();
}

TmlPoll tml_race_poll(TmlSelectState* state, TmlContext* cx) {
    if (!state) {
        return tml_poll_ready_i64(0);
    }

    if (state->found_winner) {
        // Return the actual result of the winner
        return state->handles[state->winner_index].result;
    }

    // Check each handle for completion
    for (size_t i = 0; i < state->count; i++) {
        TmlPoll result = tml_join_poll(&state->handles[i], cx);
        if (tml_poll_is_ready(&result)) {
            state->winner_index = i;
            state->found_winner = 1;
            return result; // Return the actual result
        }
    }

    return tml_poll_pending();
}

// ============================================================================
// Timeout Implementation
// ============================================================================

TmlTimeoutState tml_timeout_new(TmlPollFn inner_poll, void* inner_state, int64_t timeout_ms) {
    TmlTimeoutState state;
    state.inner_poll = inner_poll;
    state.inner_state = inner_state;
    state.timer = tml_timer_new(timeout_ms);
    state.timed_out = 0;
    return state;
}

TmlPoll tml_timeout_poll(TmlTimeoutState* state, TmlContext* cx) {
    if (!state || !state->inner_poll) {
        return tml_poll_ready_i64(-1); // Error
    }

    if (state->timed_out) {
        // Already timed out
        TmlPoll result;
        result.tag = TML_POLL_READY;
        result._pad = 0;
        result.value.i64_value = -1; // Timeout error code
        return result;
    }

    // First, poll the timer to see if we've timed out
    TmlPoll timer_result = tml_sleep_poll(&state->timer, cx);
    if (tml_poll_is_ready(&timer_result)) {
        // Timer elapsed - timeout!
        state->timed_out = 1;
        TmlPoll result;
        result.tag = TML_POLL_READY;
        result._pad = 0;
        result.value.i64_value = -1; // Timeout error code
        return result;
    }

    // Timer hasn't elapsed - try the inner future
    TmlPoll inner_result = state->inner_poll(state->inner_state, cx);
    if (tml_poll_is_ready(&inner_result)) {
        return inner_result; // Inner future completed
    }

    return tml_poll_pending();
}
