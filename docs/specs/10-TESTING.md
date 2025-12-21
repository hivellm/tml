# TML v1.0 — Testing Framework

## 1. Basic Tests

### 1.1 @test Directive

```tml
@test
func test_addition() {
    assert_eq(2 + 2, 4)
}

@test
func test_string_concat() {
    let result = "hello" + " world"
    assert_eq(result, "hello world")
}
```

### 1.2 Assertions

```tml
assert(condition)                    // fails if false
assert_eq(actual, expected)          // equality
assert_ne(actual, expected)          // difference
assert_lt(a, b)                      // a < b
assert_le(a, b)                      // a <= b
assert_gt(a, b)                      // a > b
assert_ge(a, b)                      // a >= b
```

### 1.3 Custom Messages

```tml
@test
func test_with_message() {
    let x = compute()
    assert(x > 0, "x should be positive, got: " + x.to_string())
    assert_eq(x, 42, "expected 42")
}
```

## 2. Outcome Tests

### 2.1 Expect Panic

```tml
@test
@should_panic
func test_divide_by_zero() {
    divide(10, 0)
}

@test
@should_panic(message = "division by zero")
func test_panic_message() {
    divide(10, 0)
}
```

### 2.2 Expect Error

```tml
@test
func test_file_not_found() -> Outcome[Unit, TestError] {
    let result = File.open("nonexistent.txt")
    assert(result.is_failure())
    return Success(unit)
}

@test
@should_error(IoError)
func test_expects_io_error() -> Outcome[Unit, IoError] {
    let _ = File.open("nonexistent.txt")!
    return Success(unit)
}
```

## 3. Fixtures and Setup

### 3.1 Before/After

```tml
module tests {
    var test_db: Maybe[Database] = Nothing

    @before_all
    func setup_database() {
        test_db = Just(Database.create_test())
    }

    @after_all
    func cleanup_database() {
        when test_db {
            Just(db) -> db.destroy(),
            Nothing -> unit,
        }
    }

    @before_each
    func reset_tables() {
        test_db.unwrap().reset()
    }

    @test
    func test_insert() {
        let db = test_db.unwrap()
        db.insert("key", "value")
        assert_eq(db.get("key"), Just("value"))
    }
}
```

### 3.2 Test Context

```tml
type TestContext {
    temp_dir: Path,
    config: Config,
}

@fixture
func create_context() -> TestContext {
    return TestContext {
        temp_dir: TempDir.create(),
        config: Config.test_defaults(),
    }
}

@test
func test_with_context(ctx: TestContext) {
    let file = ctx.temp_dir.join("test.txt")
    File.write(file, "data")
    assert(file.exists())
}
```

## 4. Property-Based Testing

### 4.1 @property Directive

```tml
@property
func prop_addition_commutative(a: I32, b: I32) -> Bool {
    return a + b == b + a
}

@property
func prop_list_reverse_twice[T: Equal](list: List[T]) -> Bool {
    return list.reverse().reverse() == list
}
```

### 4.2 Configuration

```tml
@property(samples = 1000)
func prop_with_more_samples(x: I32) -> Bool {
    return x * 0 == 0
}

@property(seed = 12345)
func prop_reproducible(x: I32) -> Bool {
    return x + 0 == x
}
```

### 4.3 Custom Generators

```tml
@generator
func gen_positive_int() -> I32 {
    return random_range(1, I32.MAX)
}

@property
func prop_sqrt_positive(@gen(gen_positive_int) x: I32) -> Bool {
    let sq = (x as F64).sqrt()
    return sq >= 0.0
}
```

## 5. Mocking

### 5.1 Mock Behaviors

```tml
behavior HttpClient {
    func get(this, url: String) -> Outcome[Response, Error]
}

@mock
type MockHttpClient {}

extend MockHttpClient with HttpClient {
    func get(this, url: String) -> Outcome[Response, Error] {
        return Success(Response { status: 200, body: "mocked" })
    }
}

@test
func test_with_mock() {
    let client = MockHttpClient {}
    let service = Service.new(client)
    let result = service.fetch_data()
    assert(result.is_success())
}
```

### 5.2 Spy and Verification

```tml
@test
func test_call_count() {
    let spy = Spy.new[func(String) -> Unit]()

    let service = Service.with_logger(spy.func())
    service.process()
    service.process()

    assert_eq(spy.call_count(), 2)
    assert_eq(spy.calls()[0], ("first call",))
}
```

## 6. Benchmarks

### 6.1 @bench Directive

```tml
@bench
func bench_sort(b: Bencher) {
    let data = random_list(1000)

    b.iter(do() {
        data.duplicate().sort()
    })
}
```

### 6.2 Configuration

```tml
@bench(samples = 100, warmup = 10)
func bench_with_config(b: Bencher) {
    b.iter(do() {
        expensive_operation()
    })
}
```

### 6.3 Run Benchmarks

```bash
tml bench
tml bench --filter sort
tml bench --baseline main
```

## 7. Coverage

### 7.1 Generate Report

```bash
tml test --coverage
tml test --coverage --format html --output coverage/
tml test --coverage --format json
```

### 7.2 Thresholds

```toml
# tml.toml
[test]
coverage-threshold = 80  # minimum 80%
coverage-fail-under = true
```

## 8. Organization

### 8.1 Test Module

```tml
// src/math.tml
module math

public func add(a: I32, b: I32) -> I32 {
    return a + b
}

// Inline tests (only compiled with test flag)
@when(test)
module tests {
    import super.*

    @test
    func test_add() {
        assert_eq(add(2, 3), 5)
    }
}
```

### 8.2 Separate Test Files

```
src/
  math.tml
tests/
  test_math.tml
```

```tml
// tests/test_math.tml
module test_math

import mylib.math.*

@test
func test_add_external() {
    assert_eq(add(10, 20), 30)
}
```

## 9. Commands

```bash
# Run all
tml test

# Filter by name
tml test test_add
tml test "test_*"

# Filter by module
tml test --module math

# Verbose
tml test --verbose

# List only
tml test --list

# Timeout
tml test --timeout 30s

# Retry on failure
tml test --retries 3

# Report
tml test --format junit --output results.xml
```

## 10. Output

```
   Running tests for myproject v1.0.0

running 24 tests
test math::test_add ........................ ok (0.1ms)
test math::test_subtract ................... ok (0.1ms)
test math::prop_commutative [1000 samples] . ok (42ms)
test io::test_file_read .................... ok (5ms)
test io::test_file_write ................... ok (3ms)

test result: ok. 24 passed; 0 failed; 0 skipped
   finished in 0.45s

   Coverage: 87.3% (threshold: 80%)
```

---

*Previous: [09-CLI.md](./09-CLI.md)*
*Next: [11-DEBUG.md](./11-DEBUG.md) — Debug System*
