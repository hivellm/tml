# TML Standard Library: Events

> `std::events` — Publish/subscribe event emitter (Node.js-style).

## Overview

The events package provides an `EventEmitter` type for decoupled communication via named events. Listeners are registered with `on()` or `once()`, and triggered with `emit()`. This follows the Node.js EventEmitter pattern.

## Import

```tml
use std::events::EventEmitter
```

---

## EventEmitter

```tml
pub type EventEmitter {
    events: HashMap[Str, I64],
    max_listeners: I64,
    total_count: I64
}
```

### Constructor

```tml
/// Creates a new EventEmitter with default settings (max 10 listeners per event).
pub func new() -> EventEmitter
```

### Registering Listeners

```tml
/// Adds a persistent listener for the given event.
/// The listener will be called every time the event is emitted.
pub func on(mut this, event: Str, listener: I64)

/// Adds a one-time listener. It will be automatically removed after the first emit.
pub func once(mut this, event: Str, listener: I64)
```

Listeners are function pointers cast to `I64`. They must have the signature `func(I64)` where the argument is the data value passed to `emit()`.

### Emitting Events

```tml
/// Emits an event, calling all registered listeners with the given data.
/// Returns `true` if any listeners were called, `false` otherwise.
pub func emit(mut this, event: Str, data: I64) -> Bool
```

One-time listeners (`once`) are removed after being called. The listener snapshot is taken before removal, so all listeners registered at the time of emit are called.

### Removing Listeners

```tml
/// Removes the first occurrence of a specific listener for the given event.
/// Returns `true` if the listener was found and removed.
pub func off(mut this, event: Str, listener: I64) -> Bool

/// Removes all listeners for the given event.
/// Returns the number of listeners removed.
pub func remove_all(mut this, event: Str) -> I64
```

### Querying

```tml
/// Returns the number of listeners for a specific event.
pub func listener_count(this, event: Str) -> I64

/// Returns the total number of listeners across all events.
pub func total_listeners(this) -> I64

/// Returns `true` if there are any listeners for the given event.
pub func has_listeners(this, event: Str) -> Bool
```

### Configuration

```tml
/// Sets the maximum number of listeners per event (advisory, not enforced).
pub func set_max_listeners(mut this, n: I64)

/// Returns the current max listeners setting.
pub func get_max_listeners(this) -> I64
```

### Cleanup

```tml
/// Destroys the emitter, freeing all internal listener arrays and the HashMap.
pub func destroy(mut this)
```

---

## Example

```tml
use std::events::EventEmitter

func on_data(data: I64) {
    print("received: ")
    print_i64(data)
    print("\n")
}

func main() -> I32 {
    var emitter = EventEmitter::new()

    emitter.on("data", on_data as I64)
    emitter.once("data", on_data as I64)

    emitter.emit("data", 42)   // prints twice (on + once)
    emitter.emit("data", 99)   // prints once (only on, once was removed)

    emitter.off("data", on_data as I64)
    emitter.emit("data", 0)    // no output (no listeners)

    emitter.destroy()
    return 0
}
```

## Internal Design

Listener arrays are stored as raw memory (`la_*` functions) rather than using `List[I64]` to avoid auto-drop issues with the TML compiler's DLL test mode. Each event maps to an array of `(function_pointer, once_flag)` pairs stored as consecutive I64 values.

The `destroy()` method manually iterates HashMap's internal bucket array (ctrl bytes + entries) to free all listener arrays before calling `HashMap.destroy()`.

## Test Coverage

| Test File | Tests | Description |
|-----------|-------|-------------|
| `emitter_basic.test.tml` | 5 | Constructor, on, listener_count, max_listeners |
| `emitter_emit.test.tml` | 8 | emit, once, off, callback verification |
| `emitter_advanced.test.tml` | 6 | remove_all, multiple events, mixed once/on, destroy |
