# Channels

Channels provide a way for threads to communicate by transferring values.
Instead of sharing memory and coordinating access through locks, threads pass
ownership of data from sender to receiver. This eliminates entire categories
of bugs because data cannot be accessed from two threads at once: once a value
is sent, only the receiver can use it.

TML provides MPSC channels — **Multi-Producer, Single-Consumer** — meaning
many threads can send into the same channel but only one thread may receive
from it.

## Creating a Channel

`channel[T]()` returns a `(Sender[T], Receiver[T])` pair connected to the
same underlying queue.

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()
// tx = the sender end (can be cloned for multiple producers)
// rx = the receiver end (unique, stays on one thread)
```

The channel is **unbounded** — there is no capacity limit on messages queued.
Senders never block waiting for space.

## Sending and Receiving

### send

`send` transfers a value into the channel. It returns `Ok(())` if the receiver
still exists, or `Err(SendError(value))` if the receiver was dropped and the
value cannot be delivered.

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    tx.send(42)

    when rx.recv() {
        Ok(value) => println("Received: ", value),  // Received: 42
        Err(_)    => println("Channel closed")
    }
}
```

### recv

`recv` blocks the calling thread until a message is available and returns it
as `Ok(T)`. If all senders have been dropped and the channel is empty, it
returns `Err(RecvError)` to signal that no more messages will ever arrive.

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

func main() {
    let (tx, rx): (Sender[Str], Receiver[Str]) = channel[Str]()

    tx.send("first")
    tx.send("second")
    drop(tx)  // Close the channel.

    // Drain all messages.
    loop {
        when rx.recv() {
            Ok(msg) => println(msg),
            Err(_)  => break  // No more messages.
        }
    }
    // Output: first
    //         second
}
```

### try_recv

`try_recv` returns immediately without blocking. It returns:
- `Ok(T)` if a message was available.
- `Err(TryRecvError::Empty)` if the channel is empty but still open.
- `Err(TryRecvError::Disconnected)` if all senders are dropped and the
  channel is empty.

```tml
use std::sync::mpsc::{channel, Sender, Receiver, TryRecvError}
use std::thread

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    thread::spawn(do() {
        thread::sleep_ms(200)
        tx.send(99)
    })

    // Poll until a message arrives.
    loop {
        when rx.try_recv() {
            Ok(value) => {
                println("Got: ", value)
                break
            },
            Err(TryRecvError::Empty)        => thread::yield_now(),
            Err(TryRecvError::Disconnected) => break
        }
    }
}
```

Use `try_recv` when the receiver has other work to do while waiting. For pure
waiting, `recv` is simpler and more efficient.

## Multiple Producers

`Sender[T]` is `Send` and can be cloned with `duplicate()`. Each clone is an
independent sender that adds messages to the same queue. When all senders are
dropped, the receiver's next `recv` returns `Err` to signal channel closure.

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()
    let tx2: Sender[I32] = tx.duplicate()

    thread::spawn(do() {
        tx.send(1)
        tx.send(2)
        // tx dropped here.
    })

    thread::spawn(do() {
        tx2.send(10)
        tx2.send(20)
        // tx2 dropped here.
    })

    // Receive all four messages (order between producers is non-deterministic).
    loop i in 0 to 4 {
        when rx.recv() {
            Ok(v) => println(v),
            Err(_) => break
        }
    }
}
```

## Practical Patterns

### Producer-Consumer

The classic pattern: one or more producer threads generate work items; a single
consumer thread processes them. The channel buffers items between production
and consumption rates.

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

type WorkItem { id: I32, payload: Str }

func main() {
    let (tx, rx): (Sender[WorkItem], Receiver[WorkItem]) = channel[WorkItem]()
    let tx2: Sender[WorkItem] = tx.duplicate()

    // Producer A
    thread::spawn(do() {
        loop i in 0 to 5 {
            tx.send(WorkItem { id: i, payload: "from-A" })
        }
        // Drop tx: one less sender.
    })

    // Producer B
    thread::spawn(do() {
        loop i in 0 to 5 {
            tx2.send(WorkItem { id: i + 100, payload: "from-B" })
        }
        // Drop tx2: all senders now dropped.
    })

    // Consumer: process until channel closes.
    loop {
        when rx.recv() {
            Ok(item) => println("Processing item ", item.id, " (", item.payload, ")"),
            Err(_)   => break
        }
    }
}
```

### Worker Pool with Channel-Distributed Tasks

Send closures over a channel to implement a thread pool that processes tasks
on demand:

```tml
use std::sync::{Arc, Mutex}
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

type Job = do()

func spawn_pool(size: I32) -> Sender[Job] {
    let (tx, rx): (Sender[Job], Receiver[Job]) = channel[Job]()
    let rx: Arc[Mutex[Receiver[Job]]] = Arc::new(Mutex::new(rx))

    loop _ in 0 to size {
        let rx: Arc[Mutex[Receiver[Job]]] = rx.duplicate()
        thread::spawn(do() {
            loop {
                let job: Job = {
                    let guard: MutexGuard[Receiver[Job]] = rx.lock()
                    when guard.get().recv() {
                        Ok(j)  => j,
                        Err(_) => break
                    }
                }
                // Run outside the lock so other workers can dequeue.
                job()
            }
        })
    }

    return tx
}

func main() {
    let pool: Sender[Job] = spawn_pool(4)

    loop i in 0 to 20 {
        let task_id: I32 = i
        pool.send(do() {
            println("Task ", task_id, " processed")
        })
    }

    drop(pool)  // Signal workers to stop when queue empties.
    thread::sleep_ms(500)  // Wait for workers to finish.
}
```

### Request-Response

Embed a one-shot sender in the request so the server can reply directly to
the originating thread:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

type Request {
    query: Str,
    reply_to: Sender[I32]
}

func main() {
    let (req_tx, req_rx): (Sender[Request], Receiver[Request]) =
        channel[Request]()

    // Server thread: process requests and send replies.
    thread::spawn(do() {
        loop {
            when req_rx.recv() {
                Ok(req) => {
                    let result: I32 = req.query.len() as I32
                    req.reply_to.send(result)
                },
                Err(_) => break
            }
        }
    })

    // Client: create a reply channel, send request, wait for reply.
    let (reply_tx, reply_rx): (Sender[I32], Receiver[I32]) = channel[I32]()
    req_tx.send(Request { query: "hello", reply_to: reply_tx })

    when reply_rx.recv() {
        Ok(len) => println("Query length: ", len),  // Query length: 5
        Err(_)  => println("Server closed")
    }
}
```

### Pipeline

Chain channels into a processing pipeline where each stage reads from one
channel and writes to the next:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

func main() {
    let (raw_tx, raw_rx):       (Sender[I32], Receiver[I32]) = channel[I32]()
    let (doubled_tx, doubled_rx): (Sender[I32], Receiver[I32]) = channel[I32]()
    let (filtered_tx, filtered_rx): (Sender[I32], Receiver[I32]) = channel[I32]()

    // Stage 1 → Stage 2: double each value.
    thread::spawn(do() {
        loop {
            when raw_rx.recv() {
                Ok(v)  => doubled_tx.send(v * 2),
                Err(_) => break
            }
        }
    })

    // Stage 2 → Stage 3: keep values over 10.
    thread::spawn(do() {
        loop {
            when doubled_rx.recv() {
                Ok(v) => {
                    if v > 10 {
                        filtered_tx.send(v)
                    }
                },
                Err(_) => break
            }
        }
    })

    // Source: push values 1 to 10 into the pipeline.
    loop i in 1 through 10 {
        raw_tx.send(i)
    }
    drop(raw_tx)

    // Sink: collect results.
    loop {
        when filtered_rx.recv() {
            Ok(v)  => println(v),  // Prints 12, 14, 16, 18, 20
            Err(_) => break
        }
    }
}
```

## Error Handling

### SendError

Sending fails if the receiver was dropped before the message could be
delivered. The unsent value is returned inside `SendError` so it is not
silently lost.

```tml
use std::sync::mpsc::{channel, Sender, Receiver, SendError}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()
    drop(rx)  // Drop the receiver.

    when tx.send(42) {
        Ok(_)                => println("Sent"),
        Err(SendError(value)) => println("Receiver gone; could not send: ", value)
    }
}
```

### RecvError

Receiving fails when all senders have been dropped and the queue is empty.
This is the normal way a channel closes: producers signal completion by
dropping their senders.

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

func main() {
    let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()
    tx.send(1)
    drop(tx)

    println(rx.recv().unwrap())  // 1
    // Channel is now closed; next recv returns Err.
    when rx.recv() {
        Ok(_)  => println("Unexpected"),
        Err(_) => println("Channel closed (expected)")
    }
}
```

## Thread Safety Properties

| Type | Send | Sync | Notes |
|------|------|------|-------|
| `Sender[T]` | Yes (if T: Send) | Yes (if T: Send) | Can be cloned and sent freely |
| `Receiver[T]` | Yes (if T: Send) | No | Only one thread may call recv |

`Receiver` is not `Sync` by design: allowing concurrent reads from two threads
would require coordination inside the channel to decide which thread receives
each message, which MPSC intentionally avoids.

## Channels vs. Shared Memory

| Concern | Channels | Mutex + Arc |
|---------|----------|-------------|
| Ownership transfer | Clear: sender gives up value | Shared: all threads access together |
| Locking | None at use site | Explicit lock/unlock |
| Backpressure | Not built-in (unbounded queue) | Not applicable |
| Best for | Passing events and work items | Shared mutable state |
| Deadlock risk | Low | Requires care |

## Best Practices

1. **Drop senders when done.** Dropping all senders closes the channel and
   lets the receiver's `recv` loop terminate naturally. Forgetting to drop
   a sender causes the receiver to block forever waiting for a message that
   never comes.

2. **Handle `send` errors.** If the receiver is dropped, sending into the
   channel returns the value back. Ignoring this silently discards work.

3. **Use `recv` over `try_recv` when possible.** Blocking is more efficient
   than a spin loop on `try_recv`. Reserve `try_recv` for cases where the
   receiver thread has other useful work to do.

4. **One receiver, many senders.** If you need multiple independent consumers,
   use multiple channels, or protect a single `Receiver` behind a
   `Mutex` (see the worker pool pattern above).

5. **Consider bounded channels for backpressure.** The current channel is
   unbounded. When producers are much faster than consumers, the queue can
   grow without limit. Structure your application to rate-limit producers
   if memory growth is a concern.
