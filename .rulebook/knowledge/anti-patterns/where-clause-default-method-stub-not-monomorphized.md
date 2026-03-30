# Anti-Pattern: Default behavior methods with `where` clause constraints generate stubs, not real bodies

## Problem

When a `behavior` defines a default method with a `where This::Item: SomeTrait` constraint (e.g., `is_sorted` on `Iterator`), the compiler generates a **stub** for concrete implementing types instead of monomorphizing the default body:

```llvm
; Stub for unimplemented default method is_sorted
define internal i1 @tml_Counter_is_sorted(ptr %this) #0 {
  call void @panic(ptr @.str.stub_Counter_is_sorted)
  ...
}
```

At runtime this panics. If the test has a timeout, it may instead appear as a timeout because the panic message is mistaken for an infinite loop in some test runners.

## Affected pattern

```tml
behavior Iterator {
    // This default method will NOT be monomorphized for concrete types:
    pub func is_sorted(mut this) -> Bool where This::Item: Ord { ... }
}
```

## Workaround

Add an explicit `impl TypeName { pub func method_name(...) ... }` block for each concrete type that needs the method. Use `<` / `>` comparison operators instead of `.cmp(ref x)` to avoid borrow checker issues:

```tml
impl Counter {
    pub func is_sorted(mut this) -> Bool {
        when this.next() {
            Nothing => return true,
            Just(first) => {
                let mut prev: I32 = first
                loop (true) {
                    when this.next() {
                        Just(x) => {
                            if x < prev { return false }
                            prev = x
                        },
                        Nothing => return true
                    }
                }
                return true   // required: explicit trailing return after loop (true)
            }
        }
    }
}
```

## Additional pitfall: Bool return from loop (true) requires explicit trailing return

A function returning `Bool` that contains a `loop (true) { ... }` where all exits are `return` inside the loop generates invalid LLVM IR:

```
error: '%t381' defined with type 'i32' but expected 'i1'
  ret i1 %t381
```

Fix: add `return true` (or `return false`) as a trailing statement after the `loop` block, even though it is unreachable. This gives the codegen a valid terminal for the `Just` arm's block.

## Additional pitfall: `.cmp(ref prev)` triggers borrow checker

```tml
// ERROR: cannot assign to `prev` because it is borrowed
let ord: Ordering = x.cmp(ref prev)
prev = x   // borrow on `prev` not released yet
```

Use `<` instead — it takes values directly and does not hold a borrow across statements.

## Discovery

Session 2026-03-30. Files: `lib/core/tests/iter/iter_extras.test.tml`, `lib/core/src/iter/traits/iterator.tml`.
