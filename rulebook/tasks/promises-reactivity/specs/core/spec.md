# Core Library - Promises and Reactive Streams Specification

## Purpose

This specification defines the Promise and Observable types for asynchronous and reactive
programming in TML. These types provide high-level abstractions familiar to JavaScript
developers while integrating with TML's existing async/await infrastructure.

---

## ADDED Requirements

### Requirement: Promise Type Definition
The system SHALL provide a `Promise[T]` generic type that represents an asynchronous
computation that may complete with a value of type T or fail with an error.

#### Scenario: Create pending promise
Given a promise constructor with a resolver function
When the promise is created
Then the promise state MUST be Pending

#### Scenario: Resolve promise with value
Given a pending promise
When resolve(value) is called
Then the promise state MUST be Fulfilled with the value

#### Scenario: Reject promise with error
Given a pending promise
When reject(error) is called
Then the promise state MUST be Rejected with the error

---

### Requirement: Promise Static Constructors
The system SHALL provide static methods to create promises in specific states.

#### Scenario: Create resolved promise
Given a value of type T
When Promise.resolve(value) is called
Then a new promise MUST be returned in Fulfilled state with that value

#### Scenario: Create rejected promise
Given an error value
When Promise.reject(error) is called
Then a new promise MUST be returned in Rejected state with that error

---

### Requirement: Promise Then Chaining
The system SHALL provide a `.then()` method that transforms the fulfilled value.

#### Scenario: Chain successful promise
Given a fulfilled promise with value 42
When .then(do(x) x * 2) is called
Then a new promise MUST be returned that fulfills with 84

#### Scenario: Chain rejected promise
Given a rejected promise with an error
When .then(on_fulfilled) is called
Then a new promise MUST be returned that remains rejected with the same error

---

### Requirement: Promise Catch Handler
The system SHALL provide a `.catch()` method for error handling.

#### Scenario: Catch rejected promise
Given a rejected promise with error E
When .catch(do(e) recovery_value) is called
Then a new promise MUST be returned that fulfills with the recovery value

#### Scenario: Catch fulfilled promise
Given a fulfilled promise with value V
When .catch(handler) is called
Then a new promise MUST be returned that fulfills with V (handler not invoked)

---

### Requirement: Promise Finally Handler
The system SHALL provide a `.finally()` method for cleanup.

#### Scenario: Finally on fulfilled promise
Given a fulfilled promise
When .finally(cleanup_fn) is called
Then cleanup_fn MUST be invoked and promise MUST fulfill with original value

#### Scenario: Finally on rejected promise
Given a rejected promise
When .finally(cleanup_fn) is called
Then cleanup_fn MUST be invoked and promise MUST reject with original error

---

### Requirement: Promise All Combinator
The system SHALL provide `Promise.all()` that waits for all promises.

#### Scenario: All promises succeed
Given an array of 3 promises that all fulfill
When Promise.all(promises) is called
Then it MUST return a promise that fulfills with array of all values in order

#### Scenario: One promise fails
Given an array of promises where one rejects
When Promise.all(promises) is called
Then it MUST return a promise that rejects with the first rejection error

---

### Requirement: Promise Race Combinator
The system SHALL provide `Promise.race()` that returns first settled promise.

#### Scenario: First promise fulfills
Given promises where first to settle fulfills with value V
When Promise.race(promises) is called
Then it MUST return a promise that fulfills with V

#### Scenario: First promise rejects
Given promises where first to settle rejects with error E
When Promise.race(promises) is called
Then it MUST return a promise that rejects with E

---

### Requirement: Observable Type Definition
The system SHALL provide an `Observable[T]` type for push-based streams of values.

#### Scenario: Create observable from producer
Given a producer function that emits values
When Observable.new(producer) is called
Then an observable MUST be created that will invoke producer on subscription

#### Scenario: Subscribe to observable
Given an observable and an observer
When observable.subscribe(observer) is called
Then a subscription MUST be returned and observer.on_next called for each value

---

### Requirement: Observer Behavior
The system SHALL define an `Observer[T]` behavior with event handlers.

#### Scenario: Receive next value
Given a subscribed observer
When the observable emits a value
Then observer.on_next(value) MUST be called

#### Scenario: Receive error
Given a subscribed observer
When the observable errors
Then observer.on_error(error) MUST be called and stream MUST terminate

#### Scenario: Receive completion
Given a subscribed observer
When the observable completes
Then observer.on_complete() MUST be called and stream MUST terminate

---

### Requirement: Subscription Cancellation
The system SHALL provide subscription cancellation via unsubscribe.

#### Scenario: Cancel subscription
Given an active subscription
When subscription.unsubscribe() is called
Then the subscription MUST be closed and no more events delivered

#### Scenario: Check subscription status
Given a subscription
When subscription.closed is accessed
Then it MUST return true if unsubscribed, false otherwise

---

### Requirement: Observable Creation Operators
The system SHALL provide operators to create observables from various sources.

#### Scenario: Create from values
Given a list of values
When Observable.of(v1, v2, v3) is called
Then an observable MUST emit each value in order then complete

#### Scenario: Create from iterable
Given an iterable collection
When Observable.from(iterable) is called
Then an observable MUST emit each element in order then complete

#### Scenario: Create interval
Given a duration in milliseconds
When Observable.interval(ms) is called
Then an observable MUST emit incrementing integers at that interval

---

### Requirement: Observable Map Operator
The system SHALL provide a `.map()` operator to transform values.

#### Scenario: Map transform values
Given an observable emitting [1, 2, 3]
When .map(do(x) x * 10) is applied
Then the resulting observable MUST emit [10, 20, 30]

---

### Requirement: Observable Filter Operator
The system SHALL provide a `.filter()` operator to select values.

#### Scenario: Filter values
Given an observable emitting [1, 2, 3, 4, 5]
When .filter(do(x) x > 2) is applied
Then the resulting observable MUST emit [3, 4, 5]

---

### Requirement: Observable Merge Operator
The system SHALL provide a `.merge()` operator to combine streams.

#### Scenario: Merge two observables
Given two observables A and B emitting values
When A.merge(B) is called
Then the resulting observable MUST emit values from both as they arrive

---

### Requirement: Pipe Operator Syntax
The system SHALL support the `|>` pipe operator for fluent chaining.

#### Scenario: Chain operators with pipe
Given an observable and transformation functions
When observable |> map(f) |> filter(g) |> take(n) is written
Then it MUST be equivalent to observable.map(f).filter(g).take(n)

---

### Requirement: Promise Await Integration
The system SHALL allow awaiting promises in async functions.

#### Scenario: Await promise in async function
Given an async function and a Promise[T]
When await promise is used
Then execution MUST suspend until promise settles and return T or propagate error

---

### Requirement: Observable to Promise Conversion
The system SHALL provide conversion from Observable to Promise.

#### Scenario: First value as promise
Given an observable
When observable.first().to_promise() is called
Then a promise MUST be returned that fulfills with first emitted value

#### Scenario: All values as promise
Given an observable that completes
When observable.to_array() is called
Then a promise MUST be returned that fulfills with array of all values

---

## MODIFIED Requirements

None - this is a new feature addition.

---

## REMOVED Requirements

None - no existing features are removed.
