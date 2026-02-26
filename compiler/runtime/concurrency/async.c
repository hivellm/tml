/**
 * @file async.c
 * @brief TML Runtime - Async Executor Implementation
 *
 * Implements the async runtime infrastructure declared in async.h.
 * Provides a cooperative multitasking executor based on polling.
 *
 * ## Components
 *
 * - **Executor**: Single-threaded task scheduler with ready/pending queues
 * - **Waker**: Re-schedules suspended tasks when they can make progress
 * - **Timer**: Poll-based sleep/delay using OS monotonic clock
 * - **Yield**: Single-shot yield to give other tasks a chance to run
 * - **Poll utilities**: Constructors and predicates for TmlPoll values
 *
 * ## Platform Support
 *
 * - **Windows**: Uses GetTickCount64() for monotonic time
 * - **Unix/POSIX**: Uses clock_gettime(CLOCK_MONOTONIC) for monotonic time
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
#include <time.h>
#endif

// ============================================================================
// Type Definitions (matching async.h — self-contained to avoid linkage mismatch)
// ============================================================================

#define TML_POLL_READY 0
#define TML_POLL_PENDING 1

typedef struct TmlPoll {
    int32_t tag;
    int32_t _pad;
    union {
        int64_t i64_value;
        double f64_value;
        void* ptr_value;
        int32_t i32_value;
        int8_t bytes[8];
    } value;
} TmlPoll;

struct TmlTask;
struct TmlExecutor;
struct TmlContext;

typedef TmlPoll (*TmlPollFn)(void* state, struct TmlContext* cx);

typedef enum TmlTaskState {
    TML_TASK_PENDING,
    TML_TASK_RUNNING,
    TML_TASK_COMPLETED,
    TML_TASK_FAILED
} TmlTaskState;

typedef struct TmlTask {
    uint64_t id;
    void* state;
    size_t state_size;
    TmlPollFn poll_fn;
    TmlTaskState task_state;
    TmlPoll result;
    struct TmlTask* next;
} TmlTask;

typedef void (*TmlWakeFn)(void* data);

typedef struct TmlWaker {
    TmlWakeFn wake_fn;
    void* data;
    uint64_t task_id;
} TmlWaker;

typedef struct TmlContext {
    TmlWaker waker;
    struct TmlExecutor* executor;
} TmlContext;

typedef struct TmlTaskQueue {
    TmlTask* head;
    TmlTask* tail;
    size_t count;
} TmlTaskQueue;

typedef struct TmlExecutor {
    TmlTaskQueue ready_queue;
    TmlTaskQueue pending_queue;
    uint64_t next_task_id;
    int32_t running;
    TmlTask* current_task;
} TmlExecutor;

typedef struct TmlTimerState {
    int64_t start_time_ms;
    int64_t duration_ms;
    int32_t started;
} TmlTimerState;

typedef struct TmlYieldState {
    int32_t yielded;
} TmlYieldState;

typedef struct TmlChannel {
    void* buffer;
    size_t capacity;
    size_t item_size;
    size_t head;
    size_t tail;
    size_t count;
    TmlWaker* pending_sender;
    TmlWaker* pending_receiver;
    int32_t closed;
} TmlChannel;

typedef struct TmlTaskHandle {
    uint64_t task_id;
    TmlExecutor* executor;
    int32_t completed;
    TmlPoll result;
} TmlTaskHandle;

// Forward declarations
TML_EXPORT void tml_executor_wake(TmlExecutor* executor, uint64_t task_id);
TML_EXPORT void tml_waker_destroy(TmlWaker* waker);

// ============================================================================
// Monotonic Time (platform-specific)
// ============================================================================

static int64_t get_time_ms(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// ============================================================================
// Poll Result Utilities
// ============================================================================

TML_EXPORT TmlPoll tml_poll_ready_i64(int64_t value) {
    TmlPoll p;
    p.tag = TML_POLL_READY;
    p._pad = 0;
    p.value.i64_value = value;
    return p;
}

TML_EXPORT TmlPoll tml_poll_ready_ptr(void* value) {
    TmlPoll p;
    p.tag = TML_POLL_READY;
    p._pad = 0;
    p.value.ptr_value = value;
    return p;
}

TML_EXPORT TmlPoll tml_poll_pending(void) {
    TmlPoll p;
    p.tag = TML_POLL_PENDING;
    p._pad = 0;
    p.value.i64_value = 0;
    return p;
}

TML_EXPORT int32_t tml_poll_is_ready(const TmlPoll* poll) {
    return poll->tag == TML_POLL_READY ? 1 : 0;
}

TML_EXPORT int32_t tml_poll_is_pending(const TmlPoll* poll) {
    return poll->tag == TML_POLL_PENDING ? 1 : 0;
}

// ============================================================================
// Task Queue Operations
// ============================================================================

TML_EXPORT void tml_queue_init(TmlTaskQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

TML_EXPORT void tml_queue_push(TmlTaskQueue* queue, TmlTask* task) {
    task->next = NULL;
    if (queue->tail) {
        queue->tail->next = task;
    } else {
        queue->head = task;
    }
    queue->tail = task;
    queue->count++;
}

TML_EXPORT TmlTask* tml_queue_pop(TmlTaskQueue* queue) {
    if (!queue->head)
        return NULL;
    TmlTask* task = queue->head;
    queue->head = task->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    task->next = NULL;
    queue->count--;
    return task;
}

TML_EXPORT TmlTask* tml_queue_remove_by_id(TmlTaskQueue* queue, uint64_t task_id) {
    TmlTask* prev = NULL;
    TmlTask* curr = queue->head;
    while (curr) {
        if (curr->id == task_id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                queue->head = curr->next;
            }
            if (curr == queue->tail) {
                queue->tail = prev;
            }
            curr->next = NULL;
            queue->count--;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

TML_EXPORT int32_t tml_queue_is_empty(const TmlTaskQueue* queue) {
    return queue->count == 0 ? 1 : 0;
}

// ============================================================================
// Waker Operations
// ============================================================================

/** @brief Default wake function that calls tml_executor_wake. */
static void default_wake_fn(void* data) {
    /* data encodes (executor_ptr, task_id) packed in a heap allocation */
    if (!data)
        return;
    void** pair = (void**)data;
    TmlExecutor* executor = (TmlExecutor*)pair[0];
    uint64_t task_id = (uint64_t)(uintptr_t)pair[1];
    tml_executor_wake(executor, task_id);
}

TML_EXPORT TmlWaker tml_waker_create(TmlExecutor* executor, uint64_t task_id) {
    TmlWaker waker;
    waker.wake_fn = default_wake_fn;
    /* Allocate a small pair to hold (executor, task_id) */
    void** pair = (void**)malloc(sizeof(void*) * 2);
    if (pair) {
        pair[0] = executor;
        pair[1] = (void*)(uintptr_t)task_id;
    }
    waker.data = pair;
    waker.task_id = task_id;
    return waker;
}

TML_EXPORT void tml_waker_wake(TmlWaker* waker) {
    if (waker && waker->wake_fn) {
        waker->wake_fn(waker->data);
    }
}

TML_EXPORT TmlWaker tml_waker_clone(const TmlWaker* waker) {
    TmlWaker clone;
    clone.wake_fn = waker->wake_fn;
    clone.task_id = waker->task_id;
    /* Clone the data pair */
    if (waker->data) {
        void** pair = (void**)malloc(sizeof(void*) * 2);
        if (pair) {
            void** src = (void**)waker->data;
            pair[0] = src[0];
            pair[1] = src[1];
        }
        clone.data = pair;
    } else {
        clone.data = NULL;
    }
    return clone;
}

TML_EXPORT void tml_waker_destroy(TmlWaker* waker) {
    if (waker && waker->data) {
        free(waker->data);
        waker->data = NULL;
    }
}

// ============================================================================
// Executor
// ============================================================================

TML_EXPORT TmlExecutor* tml_executor_new(void) {
    TmlExecutor* exec = (TmlExecutor*)malloc(sizeof(TmlExecutor));
    if (!exec)
        return NULL;
    tml_queue_init(&exec->ready_queue);
    tml_queue_init(&exec->pending_queue);
    exec->next_task_id = 1;
    exec->running = 0;
    exec->current_task = NULL;
    return exec;
}

TML_EXPORT void tml_executor_destroy(TmlExecutor* executor) {
    if (!executor)
        return;
    /* Free all tasks in ready queue */
    TmlTask* task;
    while ((task = tml_queue_pop(&executor->ready_queue)) != NULL) {
        if (task->state)
            free(task->state);
        free(task);
    }
    /* Free all tasks in pending queue */
    while ((task = tml_queue_pop(&executor->pending_queue)) != NULL) {
        if (task->state)
            free(task->state);
        free(task);
    }
    free(executor);
}

TML_EXPORT uint64_t tml_executor_spawn(TmlExecutor* executor, TmlPollFn poll_fn,
                                       void* initial_state, size_t state_size) {
    if (!executor || !poll_fn)
        return 0;

    TmlTask* task = (TmlTask*)malloc(sizeof(TmlTask));
    if (!task)
        return 0;

    task->id = executor->next_task_id++;
    task->poll_fn = poll_fn;
    task->task_state = TML_TASK_PENDING;
    task->result.tag = TML_POLL_PENDING;
    task->result._pad = 0;
    task->result.value.i64_value = 0;
    task->next = NULL;

    /* Copy initial state */
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

    tml_queue_push(&executor->ready_queue, task);
    return task->id;
}

TML_EXPORT int32_t tml_executor_poll_task(TmlExecutor* executor, TmlTask* task) {
    if (!executor || !task || !task->poll_fn)
        return 0;

    /* Create context with waker for this task */
    TmlWaker waker = tml_waker_create(executor, task->id);
    TmlContext cx;
    cx.waker = waker;
    cx.executor = executor;

    /* Mark as running */
    task->task_state = TML_TASK_RUNNING;
    executor->current_task = task;

    /* Poll the task */
    TmlPoll result = task->poll_fn(task->state, &cx);

    executor->current_task = NULL;

    if (result.tag == TML_POLL_READY) {
        task->task_state = TML_TASK_COMPLETED;
        task->result = result;
        tml_waker_destroy(&waker);
        return 1; /* completed */
    }

    /* Task is still pending */
    task->task_state = TML_TASK_PENDING;
    tml_waker_destroy(&waker);
    return 0; /* not done */
}

TML_EXPORT int32_t tml_executor_run(TmlExecutor* executor) {
    if (!executor)
        return -1;
    executor->running = 1;

    while (executor->running) {
        /* Check if there are any tasks left */
        if (tml_queue_is_empty(&executor->ready_queue) &&
            tml_queue_is_empty(&executor->pending_queue)) {
            break; /* All tasks done */
        }

        /* If no ready tasks but pending tasks exist, move all pending to ready.
         * This is a simple strategy — a real executor would use I/O polling.
         * Timer-based tasks will self-wake via time checks. */
        if (tml_queue_is_empty(&executor->ready_queue) &&
            !tml_queue_is_empty(&executor->pending_queue)) {
            /* Move all pending tasks to ready queue for re-polling */
            TmlTask* task;
            while ((task = tml_queue_pop(&executor->pending_queue)) != NULL) {
                tml_queue_push(&executor->ready_queue, task);
            }
        }

        /* Poll all ready tasks */
        size_t ready_count = executor->ready_queue.count;
        for (size_t i = 0; i < ready_count; i++) {
            TmlTask* task = tml_queue_pop(&executor->ready_queue);
            if (!task)
                break;

            int32_t completed = tml_executor_poll_task(executor, task);
            if (completed) {
                /* Task done — free it */
                if (task->state)
                    free(task->state);
                free(task);
            } else {
                /* Task still pending — move to pending queue */
                tml_queue_push(&executor->pending_queue, task);
            }
        }
    }

    executor->running = 0;
    return 0;
}

TML_EXPORT void tml_executor_wake(TmlExecutor* executor, uint64_t task_id) {
    if (!executor)
        return;
    /* Move task from pending to ready queue */
    TmlTask* task = tml_queue_remove_by_id(&executor->pending_queue, task_id);
    if (task) {
        tml_queue_push(&executor->ready_queue, task);
    }
}

TML_EXPORT TmlPoll tml_block_on(TmlPollFn poll_fn, void* state, size_t state_size) {
    TmlPoll result;

    /* Create a dummy waker and context for polling */
    TmlWaker dummy_waker;
    dummy_waker.wake_fn = NULL;
    dummy_waker.data = NULL;
    dummy_waker.task_id = 0;

    TmlContext cx;
    cx.waker = dummy_waker;
    cx.executor = NULL;

    /* Copy state to local buffer so poll_fn can mutate it */
    void* local_state = NULL;
    if (state && state_size > 0) {
        local_state = malloc(state_size);
        if (!local_state)
            return tml_poll_pending();
        memcpy(local_state, state, state_size);
    }

    /* Poll loop until Ready */
    for (;;) {
        result = poll_fn(local_state ? local_state : state, &cx);
        if (result.tag == TML_POLL_READY) {
            break;
        }
        /* Timer-based futures will eventually become Ready.
         * For other futures, this busy-loops — acceptable for block_on. */
    }

    if (local_state)
        free(local_state);
    return result;
}

// ============================================================================
// Spawn / Join
// ============================================================================

TML_EXPORT TmlTaskHandle tml_spawn(TmlExecutor* executor, TmlPollFn poll_fn, void* initial_state,
                                   size_t state_size) {
    TmlTaskHandle handle;
    handle.executor = executor;
    handle.completed = 0;
    handle.result = tml_poll_pending();

    if (executor) {
        handle.task_id = tml_executor_spawn(executor, poll_fn, initial_state, state_size);
    } else {
        handle.task_id = 0;
    }
    return handle;
}

TML_EXPORT TmlPoll tml_join_poll(TmlTaskHandle* handle, TmlContext* cx) {
    (void)cx;
    if (!handle || !handle->executor)
        return tml_poll_pending();

    if (handle->completed) {
        return handle->result;
    }

    /* Check if the task exists in either queue — if not, it completed */
    /* For now, return Pending and let the executor drive completion */
    return tml_poll_pending();
}

// ============================================================================
// Timer / Sleep
// ============================================================================

TML_EXPORT TmlTimerState tml_timer_new(int64_t duration_ms) {
    TmlTimerState state;
    state.start_time_ms = 0;
    state.duration_ms = duration_ms;
    state.started = 0;
    return state;
}

/** @brief FFI-safe version: writes TimerState to output pointer. */
TML_EXPORT void tml_timer_new_ptr(int64_t duration_ms, TmlTimerState* out) {
    if (!out)
        return;
    out->start_time_ms = 0;
    out->duration_ms = duration_ms;
    out->started = 0;
}

TML_EXPORT TmlPoll tml_sleep_poll(TmlTimerState* state, TmlContext* cx) {
    (void)cx;
    if (!state)
        return tml_poll_ready_i64(0);

    if (!state->started) {
        state->started = 1;
        state->start_time_ms = get_time_ms();

        /* If duration is 0, complete immediately */
        if (state->duration_ms <= 0) {
            return tml_poll_ready_i64(0);
        }
        return tml_poll_pending();
    }

    /* Check if elapsed */
    int64_t now = get_time_ms();
    int64_t elapsed = now - state->start_time_ms;
    if (elapsed >= state->duration_ms) {
        return tml_poll_ready_i64(0);
    }

    return tml_poll_pending();
}

/** @brief FFI-safe version: writes PollResult to output pointer. */
TML_EXPORT void tml_sleep_poll_ptr(TmlTimerState* state, TmlContext* cx, TmlPoll* out) {
    TmlPoll result = tml_sleep_poll(state, cx);
    if (out)
        *out = result;
}

TML_EXPORT TmlPoll tml_delay_poll(TmlTimerState* state, TmlContext* cx) {
    return tml_sleep_poll(state, cx);
}

/** @brief FFI-safe version: writes PollResult to output pointer. */
TML_EXPORT void tml_delay_poll_ptr(TmlTimerState* state, TmlContext* cx, TmlPoll* out) {
    TmlPoll result = tml_delay_poll(state, cx);
    if (out)
        *out = result;
}

// ============================================================================
// Yield
// ============================================================================

TML_EXPORT TmlPoll tml_yield_poll(TmlYieldState* state, TmlContext* cx) {
    (void)cx;
    if (!state)
        return tml_poll_ready_i64(0);

    if (!state->yielded) {
        state->yielded = 1;
        return tml_poll_pending();
    }

    return tml_poll_ready_i64(0);
}

/** @brief FFI-safe version: writes PollResult to output pointer. */
TML_EXPORT void tml_yield_poll_ptr(TmlYieldState* state, TmlContext* cx, TmlPoll* out) {
    TmlPoll result = tml_yield_poll(state, cx);
    if (out)
        *out = result;
}

// ============================================================================
// Channel (basic implementation)
// ============================================================================

TML_EXPORT TmlChannel* tml_channel_new(size_t capacity, size_t item_size) {
    TmlChannel* ch = (TmlChannel*)malloc(sizeof(TmlChannel));
    if (!ch)
        return NULL;

    ch->buffer = malloc(capacity * item_size);
    if (!ch->buffer) {
        free(ch);
        return NULL;
    }

    ch->capacity = capacity;
    ch->item_size = item_size;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->pending_sender = NULL;
    ch->pending_receiver = NULL;
    ch->closed = 0;
    return ch;
}

TML_EXPORT void tml_channel_destroy(TmlChannel* channel) {
    if (!channel)
        return;
    if (channel->pending_sender) {
        tml_waker_destroy(channel->pending_sender);
        free(channel->pending_sender);
    }
    if (channel->pending_receiver) {
        tml_waker_destroy(channel->pending_receiver);
        free(channel->pending_receiver);
    }
    free(channel->buffer);
    free(channel);
}

TML_EXPORT int32_t tml_channel_try_send(TmlChannel* channel, const void* value) {
    if (!channel || channel->closed)
        return -1;
    if (channel->count >= channel->capacity)
        return 0; /* would block */

    char* dst = (char*)channel->buffer + (channel->tail * channel->item_size);
    memcpy(dst, value, channel->item_size);
    channel->tail = (channel->tail + 1) % channel->capacity;
    channel->count++;

    /* Wake pending receiver */
    if (channel->pending_receiver) {
        tml_waker_wake(channel->pending_receiver);
        tml_waker_destroy(channel->pending_receiver);
        free(channel->pending_receiver);
        channel->pending_receiver = NULL;
    }

    return 1;
}

TML_EXPORT int32_t tml_channel_try_recv(TmlChannel* channel, void* value_out) {
    if (!channel)
        return -1;
    if (channel->count == 0) {
        return channel->closed ? -1 : 0; /* closed+empty or would block */
    }

    char* src = (char*)channel->buffer + (channel->head * channel->item_size);
    memcpy(value_out, src, channel->item_size);
    channel->head = (channel->head + 1) % channel->capacity;
    channel->count--;

    /* Wake pending sender */
    if (channel->pending_sender) {
        tml_waker_wake(channel->pending_sender);
        tml_waker_destroy(channel->pending_sender);
        free(channel->pending_sender);
        channel->pending_sender = NULL;
    }

    return 1;
}

TML_EXPORT void tml_channel_close(TmlChannel* channel) {
    if (!channel)
        return;
    channel->closed = 1;

    if (channel->pending_sender) {
        tml_waker_wake(channel->pending_sender);
        tml_waker_destroy(channel->pending_sender);
        free(channel->pending_sender);
        channel->pending_sender = NULL;
    }
    if (channel->pending_receiver) {
        tml_waker_wake(channel->pending_receiver);
        tml_waker_destroy(channel->pending_receiver);
        free(channel->pending_receiver);
        channel->pending_receiver = NULL;
    }
}

TML_EXPORT int32_t tml_channel_is_empty(const TmlChannel* channel) {
    return (channel && channel->count == 0) ? 1 : 0;
}

TML_EXPORT int32_t tml_channel_is_full(const TmlChannel* channel) {
    return (channel && channel->count >= channel->capacity) ? 1 : 0;
}
