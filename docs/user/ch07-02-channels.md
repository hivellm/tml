# MPSC Channels

Channels provide a way for threads to communicate by passing messages.
TML provides MPSC (Multi-Producer, Single-Consumer) channels—multiple
threads can send, but only one receives.

## What is a Channel?

A channel is a typed conduit through which you can send and receive values.
Think of it as a thread-safe queue connecting producers to a consumer.

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

// Create a channel for I32 values
let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

// tx = transmitter (sender)
// rx = receiver
```

## Basic Usage

### Sending and Receiving

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    // Send a value
    tx.send(42)

    // Receive the value
    when rx.recv() {
        Ok(value) => println(value),  // 42
        Err(e) => println("Channel closed")
    }
}
```

### Non-Blocking Receive

Use `try_recv()` when you don't want to block:

```tml
use std::sync::mpsc::{channel, Sender, Receiver, TryRecvError}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    // Try to receive from empty channel
    when rx.try_recv() {
        Ok(value) => println("Got: ", value),
        Err(TryRecvError::Empty) => println("No message yet"),
        Err(TryRecvError::Disconnected) => println("Sender dropped")
    }

    // Send a value
    tx.send(42)

    // Now try_recv succeeds
    when rx.try_recv() {
        Ok(value) => println("Got: ", value),  // Got: 42
        Err(_) => println("Failed")
    }
}
```

## Multiple Producers

Clone the sender to allow multiple threads to send:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    // Clone sender for second producer
    let tx2: Sender[I32] = tx.duplicate()

    // Producer 1
    thread::spawn(do() {
        tx.send(1)
        tx.send(2)
    })

    // Producer 2
    thread::spawn(do() {
        tx2.send(10)
        tx2.send(20)
    })

    // Consumer receives all messages (order may vary)
    for i in 0 to 4 {
        when rx.recv() {
            Ok(value) => println(value),
            Err(_) => break
        }
    }
}
```

## Channel Patterns

### Worker Pool

Distribute work to multiple worker threads:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

type Task { id: I32 }

func main() {
    let (tx, rx): (Sender[Task], Receiver[Task]) = channel[Task]()

    // Single receiver, but work distributed across threads
    thread::spawn(do() {
        loop {
            when rx.recv() {
                Ok(task) => {
                    println("Processing task ", task.id)
                },
                Err(_) => break  // Channel closed
            }
        }
    })

    // Send tasks
    for i in 0 to 5 {
        tx.send(Task { id: i })
    }

    // Drop sender to close channel (worker will exit)
}
```

### Request-Response

Use a channel in each message for responses:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

type Request {
    query: Str,
    response_tx: Sender[I32]
}

func main() {
    let (tx, rx): (Sender[Request], Receiver[Request]) = channel[Request]()

    // Server thread
    thread::spawn(do() {
        loop {
            when rx.recv() {
                Ok(req) => {
                    // Process request and send response
                    let result: I32 = req.query.len() as I32
                    req.response_tx.send(result)
                },
                Err(_) => break
            }
        }
    })

    // Client: send request with response channel
    let (resp_tx, resp_rx): (Sender[I32], Receiver[I32]) = channel[I32]()
    tx.send(Request { query: "hello", response_tx: resp_tx })

    // Wait for response
    when resp_rx.recv() {
        Ok(result) => println("Response: ", result),  // Response: 5
        Err(_) => println("No response")
    }
}
```

### Pipeline

Chain channels to form processing pipelines:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

func main() {
    // Stage 1 -> Stage 2 -> Stage 3
    let (tx1, rx1): (Sender[I32], Receiver[I32]) = channel[I32]()
    let (tx2, rx2): (Sender[I32], Receiver[I32]) = channel[I32]()

    // Stage 2: Double the value
    thread::spawn(do() {
        loop {
            when rx1.recv() {
                Ok(v) => tx2.send(v * 2),
                Err(_) => break
            }
        }
    })

    // Stage 3: Print the value
    thread::spawn(do() {
        loop {
            when rx2.recv() {
                Ok(v) => println("Final: ", v),
                Err(_) => break
            }
        }
    })

    // Stage 1: Generate values
    for i in 1 through 5 {
        tx1.send(i)
    }
    // Output: 2, 4, 6, 8, 10
}
```

## Error Handling

### SendError

Sending fails if the receiver is dropped:

```tml
use std::sync::mpsc::{channel, Sender, Receiver, SendError}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    // Drop the receiver
    drop(rx)

    // Sending now fails
    when tx.send(42) {
        Ok(_) => println("Sent"),
        Err(SendError(value)) => {
            println("Failed to send: ", value)  // value is returned
        }
    }
}
```

### RecvError

Receiving fails when all senders are dropped and channel is empty:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    // Send one value then drop sender
    tx.send(42)
    drop(tx)

    // First recv succeeds
    when rx.recv() {
        Ok(v) => println(v),  // 42
        Err(_) => {}
    }

    // Second recv fails - channel closed
    when rx.recv() {
        Ok(v) => println(v),
        Err(_) => println("Channel closed")  // This runs
    }
}
```

## Iterating Over Messages

Receive messages in a loop until the channel closes:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    tx.send(1)
    tx.send(2)
    tx.send(3)
    drop(tx)  // Close the channel

    // Receive until channel closes
    loop {
        when rx.recv() {
            Ok(value) => println(value),
            Err(_) => break
        }
    }
}
```

## Comparison with Shared Memory

| Channels | Shared Memory (Mutex) |
|----------|----------------------|
| Data is transferred, not shared | Data is shared with locks |
| Clear ownership semantics | Must manage lock/unlock |
| No direct data races | Possible to forget unlocking |
| May have more allocation | May have lock contention |
| Better for message passing | Better for shared state |

## Best Practices

1. **Use channels for communication, mutex for shared state**
   - If you're passing data between threads, use channels
   - If multiple threads need to modify the same data, use `Mutex[T]`

2. **Drop senders when done**
   - This signals receivers that no more messages are coming
   - Helps receivers know when to stop

3. **Handle errors**
   - `send()` can fail if receiver is dropped
   - `recv()` can fail if all senders are dropped

4. **Consider backpressure**
   - Unbounded channels can grow forever
   - Consider bounded channels for production systems (future feature)

## Thread Safety

- `Sender[T]` is `Send` and `Sync` - can be cloned and shared
- `Receiver[T]` is `Send` but NOT `Sync` - only one thread can receive
- `T` must be `Send` - values are transferred between threads

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use core::marker::Send

// This works because I32 is Send
let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

// Sender can be cloned for multiple producers
let tx2: Sender[I32] = tx.duplicate()
```
