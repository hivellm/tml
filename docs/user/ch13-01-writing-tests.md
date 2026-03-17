# Writing Tests

TML tests are ordinary functions with the `@test` decorator. They live in `.test.tml` files, import the `test` package for assertion utilities, and are discovered and run automatically by `tml test`. A passing test returns `0`; a failing assertion terminates the test immediately with a diagnostic message.

## The Basic Test Structure

Every test file begins with a `use test` declaration to bring assertion functions into scope:

```tml
use test

@test
func test_addition() -> I32 {
    assert_eq(1 + 1, 2, "basic addition")
    return 0
}
```

The `@test` decorator marks the function as a test. The return type is `I32`. A return value of `0` signals success; a non-zero return signals failure. In practice you rarely write an explicit failure return — assertion functions terminate the test with a detailed message before the function can reach a `return` statement.

The `use test` import makes all assertion functions available directly. You do not need to qualify them with `test::`.

## Test File Organization

Test files use the `.test.tml` extension and live in a `tests/` directory adjacent to the code they exercise:

```
project/
├── src/
│   ├── parser.tml
│   └── formatter.tml
└── tests/
    ├── parser/
    │   ├── basic.test.tml
    │   ├── edge_cases.test.tml
    │   └── errors.test.tml
    └── formatter/
        └── output.test.tml
```

A single test file can contain multiple `@test` functions. Group related assertions into separate tests rather than one monolithic test — focused tests produce more useful failure messages and pinpoint failures more precisely.

## Named Tests

The `@test` decorator accepts an optional `name` argument for a human-readable description. Named tests appear in test output using the provided name rather than the function name, which is useful when the function name is abbreviated or when you want a full sentence description:

```tml
use test

@test(name = "parser rejects empty input")
func test_empty() -> I32 {
    let result = parse("")
    assert_eq(result.is_err(), true, "empty string should fail")
    return 0
}

@test(name = "parser accepts valid IPv4 addresses")
func test_valid_ipv4() -> I32 {
    assert_eq(parse("127.0.0.1").is_ok(), true, "loopback")
    assert_eq(parse("255.255.255.255").is_ok(), true, "broadcast")
    assert_eq(parse("0.0.0.0").is_ok(), true, "unspecified")
    return 0
}
```

## Assertion Functions

The `test` package provides a complete set of assertion functions. Each accepts an optional trailing string for a custom failure message.

### Equality and Inequality

```tml
use test

@test
func test_equality() -> I32 {
    let x: I32 = 42

    assert_eq(x, 42, "x should equal 42")
    assert_ne(x, 0, "x should not be zero")

    let s: Str = "hello"
    assert_eq(s, "hello", "string equality")
    assert_ne(s, "world", "string inequality")

    return 0
}
```

`assert_eq` and `assert_ne` print both values when they fail, making it easy to see what was expected versus what was received.

### Boolean Conditions

```tml
use test

@test
func test_booleans() -> I32 {
    let items: List[I32] = [1, 2, 3]

    assert_true(items.len() > 0, "list should not be empty")
    assert_false(items.is_empty(), "is_empty should return false")

    // assert() is an alias for assert_true
    assert(items.len() == 3, "list should have 3 elements")

    return 0
}
```

### Ordering Comparisons

```tml
use test

@test
func test_ordering() -> I32 {
    let count: I64 = compute_count()

    assert_gt(count, 0, "count must be positive")   // count > 0
    assert_ge(count, 1, "count must be at least 1") // count >= 1
    assert_lt(count, 1000, "count must be small")   // count < 1000
    assert_le(count, 999, "count must be at most 999")  // count <= 999

    return 0
}
```

### Testing Maybe and Outcome

Standard `assert_eq` comparisons work on `Maybe[T]` and `Outcome[T, E]` when the inner types implement `PartialEq`. For common patterns, extract the inner value before asserting:

```tml
use test

@test
func test_maybe_just() -> I32 {
    let result: Maybe[I32] = find_item(1)

    assert_true(result.is_just(), "item 1 should exist")
    assert_eq(result.unwrap(), 42, "item 1 should be 42")

    return 0
}

@test
func test_maybe_nothing() -> I32 {
    let result: Maybe[I32] = find_item(9999)

    assert_true(result.is_nothing(), "item 9999 should not exist")
    assert_eq(result.unwrap_or(-1), -1, "fallback to -1")

    return 0
}

@test
func test_outcome_ok() -> I32 {
    let result: Outcome[I32, Str] = parse_number("42")

    assert_true(result.is_ok(), "parsing '42' should succeed")
    assert_eq(result.unwrap(), 42, "parsed value should be 42")

    return 0
}

@test
func test_outcome_err() -> I32 {
    let result: Outcome[I32, Str] = parse_number("not-a-number")

    assert_true(result.is_err(), "parsing garbage should fail")

    return 0
}
```

## Testing Panics with @should_panic

The `@should_panic` decorator marks a test that is expected to panic. If the test function returns normally without panicking, the test framework reports a failure. If it panics, the test passes:

```tml
use test

@should_panic
@test
func test_unwrap_nothing_panics() -> I32 {
    let x: Maybe[I32] = Nothing
    x.unwrap()  // should panic
    return 0
}
```

To verify that the panic message contains a specific substring, provide the `expected` argument:

```tml
use test

@should_panic(expected = "index out of bounds")
@test
func test_out_of_bounds_panics() -> I32 {
    let arr: [I32; 3] = [1, 2, 3]
    let _ = arr[10]  // should panic with "index out of bounds"
    return 0
}

@should_panic(expected = "divide by zero")
@test
func test_division_by_zero() -> I32 {
    let _ = checked_divide(10, 0)
    return 0
}
```

The test passes only if the panic message contains the expected substring. A panic with a different message is treated as a test failure.

## Test Organization Patterns

### One Behavior Per Test

Each test should verify one specific behavior of the code under test. When a test fails, you want to know immediately what behavior broke, not which of seventeen assertions in a large test triggered:

```tml
use test

// Good: each test covers one thing
@test
func test_push_increases_length() -> I32 {
    var list: List[I32] = List.new()
    list.push(42)
    assert_eq(list.len(), 1, "length after one push")
    return 0
}

@test
func test_pop_returns_last_element() -> I32 {
    var list: List[I32] = List.new()
    list.push(1)
    list.push(2)
    let popped = list.pop()
    assert_eq(popped, Just(2), "pop should return last element")
    return 0
}

@test
func test_pop_empty_list_returns_nothing() -> I32 {
    var list: List[I32] = List.new()
    let result = list.pop()
    assert_eq(result, Nothing, "pop on empty list should return Nothing")
    return 0
}
```

### Descriptive Names

Test function names should read as sentences describing what the test verifies. A reader scanning a list of test names should understand what each test covers without opening the file:

```tml
// clear intent from the name alone
func test_parse_accepts_leading_whitespace() -> I32 { ... }
func test_parse_rejects_trailing_letters() -> I32 { ... }
func test_parse_handles_negative_numbers() -> I32 { ... }
func test_parse_max_value_does_not_overflow() -> I32 { ... }
```

### Setup Helpers

When multiple tests share setup logic, extract it into a regular helper function. The helper is not a test itself — it has no `@test` decorator:

```tml
use test

func make_test_user() -> User {
    return User {
        id: 1,
        name: "Alice",
        email: "alice@example.com",
        role: Role.Member,
    }
}

@test
func test_user_can_read_posts() -> I32 {
    let user = make_test_user()
    assert_true(user.can_read_posts(), "members can read posts")
    return 0
}

@test
func test_user_cannot_delete_posts() -> I32 {
    let user = make_test_user()
    assert_false(user.can_delete_posts(), "members cannot delete posts")
    return 0
}
```

For setup that involves mutable state and needs a teardown step, structure it as a pair of calls bracketing the assertions:

```tml
use test

func open_test_db() -> TestDb {
    let db = TestDb.open(":memory:")
    db.run_migrations()
    db.seed_fixture("users_fixture.json")
    return db
}

func close_test_db(db: TestDb) {
    db.clear()
    db.close()
}

@test
func test_find_user_by_email() -> I32 {
    let db = open_test_db()

    let result = db.find_user_by_email("alice@example.com")
    assert_true(result.is_just(), "alice should be in the fixture")
    assert_eq(result.unwrap().name, "Alice", "name should match fixture")

    close_test_db(db)
    return 0
}
```

### Testing Edge Cases

Robust test suites cover boundary conditions, empty inputs, and maximum values — not just the happy path:

```tml
use test

@test
func test_sum_empty_slice() -> I32 {
    let empty: List[I32] = List.new()
    assert_eq(sum(ref empty), 0, "sum of empty list is 0")
    return 0
}

@test
func test_sum_single_element() -> I32 {
    let single: List[I32] = [42]
    assert_eq(sum(ref single), 42, "sum of one element")
    return 0
}

@test
func test_sum_maximum_values() -> I32 {
    let big: List[I64] = [I64.MAX / 2, I64.MAX / 2]
    assert_eq(sum(ref big), I64.MAX - 1, "sum near maximum")
    return 0
}

@test
func test_sum_mixed_signs() -> I32 {
    let mixed: List[I32] = [-5, 3, -2, 7, -1]
    assert_eq(sum(ref mixed), 2, "sum with negative values")
    return 0
}
```
