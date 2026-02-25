# TML Standard Library: Runtime

> `std::runtime` -- Async runtime for cooperative multitasking.

## Overview

Provides the async runtime infrastructure for TML's async/await system. The runtime uses a cooperative polling model: tasks are polled by the executor; if a task returns `Pending`, it is moved to the pending queue; when an event occurs (timer expires, data arrives), the task is woken and polled again.

This module is the foundation for TML's async programming model.

## Import

```tml
use std::runtime::executor::Executor
use std::runtime::sleep::{TimerState, PollResult, sleep_poll}
use std::runtime::yield_now::{YieldState, yield_poll}
use std::runtime::channel::Channel
```

---

## Submodules

| Module | Description |
|--------|-------------|
| `executor` | Single-threaded task executor |
| `sleep` | Timer and sleep operations |
| `yield_now` | Yield to other tasks |
| `channel` | Bounded async channel |

---

## Executor

A single-threaded async task executor that polls tasks cooperatively.

```tml
func Executor::new() -> Executor
func run(this) -> I32              // Run until all tasks complete (0 = success)
func destroy(this)                 // Free resources
func raw(this) -> *Unit            // Raw handle for FFI
```

---

## PollResult

Result of polling an async operation.

```tml
pub type PollResult { tag: I32, _pad: I32, value: I64 }

func is_ready(this) -> Bool        // tag == 0
func is_pending(this) -> Bool      // tag == 1
func get_value(this) -> I64        // Value when Ready
```

---

## TimerState

A poll-based timer that becomes Ready after a duration.

```tml
func TimerState::new(duration_ms: I64) -> TimerState
func poll(mut this) -> PollResult     // Poll until Ready
```

Free function: `sleep_poll(state: mut ref TimerState) -> PollResult`

---

## YieldState

A single-shot yield that returns Pending on first poll, Ready on second. Used to give other tasks a chance to run.

```tml
func YieldState::new() -> YieldState
func poll(mut this) -> PollResult
```

Free function: `yield_poll(state: mut ref YieldState) -> PollResult`

---

## Channel

A bounded SPSC (single-producer, single-consumer) channel backed by a circular buffer.

```tml
func Channel::new(capacity: I64, item_size: I64) -> Channel
func try_send(this, value: *Unit) -> I32     // 1=sent, 0=would block, -1=closed
func try_recv(this, value_out: *Unit) -> I32 // 1=received, 0=would block, -1=closed+empty
func close(this)                              // Close channel
func is_empty(this) -> Bool
func is_full(this) -> Bool
func destroy(this)                            // Free resources
```

---

## Example

```tml
use std::runtime::sleep::{TimerState, PollResult}

func main() {
    // Create a 100ms timer
    var timer = TimerState::new(100)

    // Poll until ready
    loop {
        let result = timer.poll()
        if result.is_ready() {
            print("Timer elapsed!\n")
            break
        }
    }
}
```
