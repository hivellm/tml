# Tasks: Promises and Reactive Streams (JavaScript/RxJS-like)

## Progress: 0% (0/58 tasks complete)

## Objective

Implement a Promise system similar to JavaScript and reactive streams similar to RxJS,
natively integrated into TML core or compiler. This will enable high-level asynchronous
and reactive programming, familiar to web developers.

---

## 1. Promise Core - Type Definition

### 1.1 Promise[T] Enum
- [ ] 1.1.1 Define `Promise[T]` as generic type in core
- [ ] 1.1.2 States: `Pending`, `Fulfilled(T)`, `Rejected(Error)`
- [ ] 1.1.3 Add `PromiseState` auxiliary enum
- [ ] 1.1.4 Implement `Promise.new()` constructor

```tml
// lib/core/promise.tml
enum PromiseState {
    Pending
    Fulfilled
    Rejected
}

struct Promise[T] {
    state: PromiseState
    value: Maybe[T]
    error: Maybe[Error]
    callbacks: Vec[Callback[T]]
}
```

### 1.2 Promise Resolvers
- [ ] 1.2.1 Implement `Promise.resolve(value: T)` - creates fulfilled Promise
- [ ] 1.2.2 Implement `Promise.reject(error: Error)` - creates rejected Promise
- [ ] 1.2.3 Implement internal `resolve()` to change state
- [ ] 1.2.4 Implement internal `reject()` to change state

---

## 2. Promise Combinators

### 2.1 Instance Methods
- [ ] 2.1.1 Implement `.then(on_fulfilled)` - chaining
- [ ] 2.1.2 Implement `.catch(on_rejected)` - error handling
- [ ] 2.1.3 Implement `.finally(on_settled)` - cleanup
- [ ] 2.1.4 Implement `.map(func)` - transform value (alias for then)

### 2.2 Static Methods
- [ ] 2.2.1 Implement `Promise.all(promises)` - wait for all
- [ ] 2.2.2 Implement `Promise.race(promises)` - first to settle
- [ ] 2.2.3 Implement `Promise.any(promises)` - first fulfilled
- [ ] 2.2.4 Implement `Promise.allSettled(promises)` - wait for all without failing

### 2.3 Async/Await Integration
- [ ] 2.3.1 Allow `await promise` in async functions
- [ ] 2.3.2 Convert `Promise[T]` to `Poll[T]` internally
- [ ] 2.3.3 Add support for `async func -> Promise[T]`

---

## 3. Observable Core (Reactive Streams)

### 3.1 Observable[T] Base Type
- [ ] 3.1.1 Define `Observable[T]` struct
- [ ] 3.1.2 Define `Observer[T]` behavior (trait)
- [ ] 3.1.3 Define `Subscription` for cancellation
- [ ] 3.1.4 Implement `Observable.new(producer)`

```tml
// lib/core/reactive.tml
behavior Observer[T] {
    func on_next(value: T)
    func on_error(error: Error)
    func on_complete()
}

struct Observable[T] {
    subscribe: func(Observer[T]) -> Subscription
}

struct Subscription {
    unsubscribe: func()
    closed: Bool
}
```

### 3.2 Subject Types
- [ ] 3.2.1 Implement `Subject[T]` - Observable + Observer
- [ ] 3.2.2 Implement `BehaviorSubject[T]` - with initial value
- [ ] 3.2.3 Implement `ReplaySubject[T]` - value buffer
- [ ] 3.2.4 Implement `AsyncSubject[T]` - last value on complete

---

## 4. Observable Creation Operators

### 4.1 Basic Creation
- [ ] 4.1.1 `Observable.of(values...)` - from static values
- [ ] 4.1.2 `Observable.from(iterable)` - from iterable
- [ ] 4.1.3 `Observable.from_promise(promise)` - from Promise
- [ ] 4.1.4 `Observable.empty()` - completes immediately
- [ ] 4.1.5 `Observable.never()` - never emits/completes
- [ ] 4.1.6 `Observable.throw_error(error)` - emits error

### 4.2 Temporal Creation
- [ ] 4.2.1 `Observable.interval(ms)` - emits every N ms
- [ ] 4.2.2 `Observable.timer(delay, period)` - delay + interval
- [ ] 4.2.3 `Observable.range(start, count)` - numeric sequence

---

## 5. Observable Transformation Operators

### 5.1 Mapping
- [ ] 5.1.1 `.map(func)` - transform each value
- [ ] 5.1.2 `.flat_map(func)` - map + flatten
- [ ] 5.1.3 `.switch_map(func)` - cancel previous
- [ ] 5.1.4 `.concat_map(func)` - in sequence

### 5.2 Filtering
- [ ] 5.2.1 `.filter(predicate)` - filter values
- [ ] 5.2.2 `.take(count)` - first N values
- [ ] 5.2.3 `.skip(count)` - skip first N
- [ ] 5.2.4 `.distinct()` - unique values
- [ ] 5.2.5 `.debounce(ms)` - wait for silence
- [ ] 5.2.6 `.throttle(ms)` - rate limit

### 5.3 Combination
- [ ] 5.3.1 `.merge(other)` - combine streams
- [ ] 5.3.2 `.concat(other)` - in sequence
- [ ] 5.3.3 `.zip(other)` - pair values
- [ ] 5.3.4 `.combine_latest(other)` - latest from each

---

## 6. Observable Utility Operators

### 6.1 Side Effects
- [ ] 6.1.1 `.tap(func)` - side effect without modifying
- [ ] 6.1.2 `.delay(ms)` - delay emissions
- [ ] 6.1.3 `.timeout(ms)` - error if too slow

### 6.2 Error Handling
- [ ] 6.2.1 `.catch_error(handler)` - recover from errors
- [ ] 6.2.2 `.retry(count)` - try again
- [ ] 6.2.3 `.retry_when(notifier)` - conditional retry

### 6.3 Aggregation
- [ ] 6.3.1 `.reduce(accumulator, seed)` - accumulate values
- [ ] 6.3.2 `.scan(accumulator, seed)` - intermediate reduce
- [ ] 6.3.3 `.to_array()` - collect into array
- [ ] 6.3.4 `.first()` / `.last()` - first/last value

---

## 7. Compiler Support

### 7.1 Parser
- [ ] 7.1.1 Support `|>` (pipe operator) syntax for chaining
- [ ] 7.1.2 Support `observable |> map(x => x * 2)` syntax
- [ ] 7.1.3 Parse inline arrow functions `do(x) expr`

### 7.2 Type Checker
- [ ] 7.2.1 Type inference for Observable operators
- [ ] 7.2.2 Verify type compatibility in combinators
- [ ] 7.2.3 Support generics in Observable/Promise

### 7.3 Codegen
- [ ] 7.3.1 Generate code for Promise callbacks
- [ ] 7.3.2 Generate code for Observable subscriptions
- [ ] 7.3.3 Optimize operator chains (fusion)

---

## 8. Runtime Support

### 8.1 Event Loop
- [ ] 8.1.1 Implement minimal event loop
- [ ] 8.1.2 Implement microtask queue for Promises
- [ ] 8.1.3 Implement scheduler for Observables
- [ ] 8.1.4 Integrate with existing async executor

### 8.2 Memory Management
- [ ] 8.2.1 Garbage collection for uncanceled subscriptions
- [ ] 8.2.2 Weak references to avoid memory leaks
- [ ] 8.2.3 Automatic unsubscribe on scope exit

---

## 9. Tests

### 9.1 Promise Tests
- [ ] 9.1.1 Tests for resolve/reject
- [ ] 9.1.2 Tests for then/catch/finally chain
- [ ] 9.1.3 Tests for Promise.all/race/any

### 9.2 Observable Tests
- [ ] 9.2.1 Tests for Observable creation
- [ ] 9.2.2 Tests for transformation operators
- [ ] 9.2.3 Tests for combination operators
- [ ] 9.2.4 Tests for cancellation (unsubscribe)

### 9.3 Integration Tests
- [ ] 9.3.1 Tests for async/await with Promises
- [ ] 9.3.2 Tests for Observable to Promise conversion
- [ ] 9.3.3 Performance benchmarks

---

## 10. Documentation

### 10.1 Specification
- [ ] 10.1.1 Document Promise API in docs/
- [ ] 10.1.2 Document Observable API in docs/
- [ ] 10.1.3 Document available operators

### 10.2 Examples
- [ ] 10.2.1 Promise usage examples
- [ ] 10.2.2 Reactive stream examples
- [ ] 10.2.3 Complete reactive app example

---

## Design Notes

### Promise vs Poll/Future
- `Poll[T]` is the low-level type used by the compiler (Ready/Pending)
- `Promise[T]` is the high-level type for users (Fulfilled/Rejected/Pending)
- Promises are automatically converted to Poll when used with `await`

### Observable vs Async Iterators
- `Observable[T]` is push-based (values are sent to observer)
- Async iterators are pull-based (consumer requests next values)
- Both can coexist in TML

### Comparison with JavaScript
| JavaScript | TML |
|------------|-----|
| `new Promise((resolve, reject) => ...)` | `Promise.new(do(resolve, reject) ...)` |
| `promise.then(x => ...)` | `promise.then(do(x) ...)` |
| `Observable.pipe(map(...))` | `observable \|> map(...)` |
| `subscribe({ next, error, complete })` | `subscribe(observer)` |

### Dependencies
- Requires async/await working (Phase 14 of roadmap)
- Requires closures/lambdas working
- Requires generics working
