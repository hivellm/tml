# Borrow Checker Specification

## Purpose

The borrow checker enforces TML's memory safety rules at compile-time. It tracks ownership and borrows to prevent data races, use-after-free, and dangling references without requiring garbage collection.

## ADDED Requirements

### Requirement: Ownership Tracking
The borrow checker SHALL track ownership of all values.

#### Scenario: Simple move
Given `let x = Vec.new(); let y = x`
When the borrow checker analyzes this code
Then `x` is marked as moved and cannot be used after the assignment to `y`

#### Scenario: Use after move
Given `let x = String.new("hello"); let y = x; print(x)`
When the borrow checker analyzes this code
Then it reports "use of moved value: x"

#### Scenario: Copy types
Given `let x: I32 = 42; let y = x; print(x)`
When the borrow checker analyzes this code
Then it succeeds because I32 implements Copy

### Requirement: Borrow Rules Enforcement
The borrow checker MUST enforce the fundamental borrow rules.

#### Scenario: Multiple shared borrows
Given `let r1 = &x; let r2 = &x; use(r1, r2)`
When the borrow checker analyzes this code
Then it succeeds (multiple shared borrows allowed)

#### Scenario: Single mutable borrow
Given `let r = &mut x; use(r)`
When the borrow checker analyzes this code
Then it succeeds (single mutable borrow allowed)

#### Scenario: Conflicting borrows
Given `let r1 = &mut x; let r2 = &x`
When the borrow checker analyzes this code while r1 is still active
Then it reports "cannot borrow x as shared because it is already borrowed as mutable"

#### Scenario: Multiple mutable borrows
Given `let r1 = &mut x; let r2 = &mut x`
When the borrow checker analyzes this code while r1 is still active
Then it reports "cannot borrow x as mutable more than once"

### Requirement: Non-Lexical Lifetimes
The borrow checker SHALL use non-lexical lifetimes (NLL) for borrow analysis.

#### Scenario: NLL allows reborrow after last use
Given `let r1 = &mut x; use(r1); let r2 = &mut x; use(r2)`
When the borrow checker analyzes this code
Then it succeeds because r1's lifetime ends at its last use

#### Scenario: Conditional borrow
Given a borrow inside an if branch not taken
When the borrow checker analyzes this code
Then it allows reborrowing in other branches

### Requirement: Lifetime Checking
The borrow checker MUST ensure references do not outlive their referents.

#### Scenario: Reference outlives value
Given `let r; { let x = 1; r = &x; } use(r)`
When the borrow checker analyzes this code
Then it reports "x does not live long enough"

#### Scenario: Return reference to local
Given `func get_ref() -> &I32 { let x = 1; return &x }`
When the borrow checker analyzes this code
Then it reports "cannot return reference to local variable"

#### Scenario: Valid reference lifetime
Given `func get_first(list: &Vec[I32]) -> &I32 { return &list[0] }`
When the borrow checker analyzes this code
Then it succeeds (reference tied to input lifetime)

### Requirement: Partial Move Tracking
The borrow checker SHALL track partial moves of struct fields.

#### Scenario: Move single field
Given `let s = Struct { a: vec1, b: vec2 }; let x = s.a; use(s.b)`
When the borrow checker analyzes this code
Then it succeeds (s.a moved, s.b still usable)

#### Scenario: Use partially moved struct
Given `let s = Struct { a: vec1, b: vec2 }; let x = s.a; use(s)`
When the borrow checker analyzes this code
Then it reports "use of partially moved value"

### Requirement: Control Flow Sensitivity
The borrow checker SHALL be control-flow sensitive.

#### Scenario: Borrow in branch
Given `if cond { let r = &mut x; use(r) } else { let r = &mut x; use(r) }`
When the borrow checker analyzes this code
Then it succeeds (borrows in different branches)

#### Scenario: Loop borrow
Given `loop { let r = &mut x; use(r) }`
When the borrow checker analyzes this code
Then it succeeds (borrow released each iteration)

#### Scenario: Borrow escapes loop
Given `var r; loop { r = &mut x; if cond { break } }; use(r)`
When the borrow checker analyzes this code
Then it handles the borrow correctly across the break

### Requirement: Mutable Reference Rules
The borrow checker MUST enforce exclusive access for mutable references.

#### Scenario: Modify through reference
Given `let r = &mut x; *r = 42`
When the borrow checker analyzes this code
Then it succeeds

#### Scenario: No aliasing with mutable
Given code that attempts to alias a mutable reference
When the borrow checker analyzes this code
Then it prevents the aliasing

### Requirement: Function Boundary Checking
The borrow checker SHALL verify borrows across function boundaries.

#### Scenario: Borrow passed to function
Given `let r = &x; func_taking_ref(r); use(r)`
When the borrow checker analyzes this code
Then it succeeds (function borrows temporarily)

#### Scenario: Borrow consumed by function
Given a function that stores the reference
When the borrow checker analyzes the call site
Then it enforces appropriate lifetime bounds

### Requirement: Error Messages
All borrow errors MUST include clear explanations.

#### Scenario: Conflicting borrow error
Given a conflicting borrow error
When the error is reported
Then it shows both borrow locations and explains the conflict

#### Scenario: Move error
Given a use-after-move error
When the error is reported
Then it shows where the value was moved

#### Scenario: Lifetime error
Given a lifetime error
When the error is reported
Then it shows the relevant lifetimes and why they don't match
