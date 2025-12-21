# TML v1.0 — Error Catalog

## 1. Lexical Errors (L)

### L001: Invalid character
```
error[L001]: invalid character
  --> src/main.tml:5:10
   |
 5 |     let x@ = 42
   |          ^ unexpected character '@'
```

### L002: Unterminated string
```
error[L002]: unterminated string literal
  --> src/main.tml:5:15
   |
 5 |     let s = "hello
   |             ^ string not closed
   |
   = help: add closing quote `"`
```

### L003: Invalid escape sequence
```
error[L003]: invalid escape sequence
  --> src/main.tml:5:20
   |
 5 |     let s = "hello\q"
   |                   ^^ unknown escape sequence
   |
   = note: valid escapes: \\, \", \n, \r, \t, \0, \xNN, \u{NNNN}
```

### L004: Invalid number literal
```
error[L004]: invalid number literal
  --> src/main.tml:5:15
   |
 5 |     let x = 0b123
   |             ^^^^^ invalid binary literal (only 0 and 1 allowed)
```

### L005: Integer overflow
```
error[L005]: integer literal out of range
  --> src/main.tml:5:15
   |
 5 |     let x: U8 = 256
   |                 ^^^ value 256 exceeds U8 max (255)
```

## 2. Syntax Errors (P)

### P001: Expected token
```
error[P001]: expected `,` or `)`
  --> src/main.tml:5:20
   |
 5 |     func add(a: I32 b: I32) -> I32
   |                     ^ expected `,` between parameters
```

### P002: Unexpected token
```
error[P002]: unexpected token
  --> src/main.tml:5:1
   |
 5 |     } else {
   |     ^ unexpected `}` - no matching `{`
```

### P003: Missing expression
```
error[P003]: expected expression
  --> src/main.tml:5:15
   |
 5 |     let x =
   |             ^ expected expression after `=`
```

### P004: Invalid pattern
```
error[P004]: invalid pattern
  --> src/main.tml:5:10
   |
 5 |     when x { 1 + 2 -> ... }
   |              ^^^^^ expected pattern, found expression
```

### P005: Missing `then` keyword
```
error[P005]: missing `then` after condition
  --> src/main.tml:5:15
   |
 5 |     if x > 0 x else -x
   |              ^ expected `then`
   |
   = help: TML requires `if cond then expr else expr`
```

## 3. Type Errors (T)

### T001: Type mismatch
```
error[T001]: type mismatch
  --> src/main.tml:5:15
   |
 5 |     let x: String = 42
   |            ^^^^^^   ^^ expected String, found I32
   |
   = help: use `42.to_string()` to convert
```

### T002: Unknown type
```
error[T002]: unknown type `Strng`
  --> src/main.tml:5:12
   |
 5 |     let x: Strng = "hello"
   |            ^^^^^ not found in this scope
   |
   = help: did you mean `String`?
```

### T003: Missing type annotation
```
error[T003]: type annotation required
  --> src/main.tml:5:5
   |
 5 |     public func process(data) -> Result
   |                         ^^^^ parameter needs type
```

### T004: Generic constraint not satisfied
```
error[T004]: trait bound not satisfied
  --> src/main.tml:10:5
   |
 8 |     func sort[T: Ord](list: List[T])
   |                  --- required by this bound
...
10 |     sort(my_list)
   |     ^^^^ `MyType` does not implement `Ord`
   |
   = help: implement `Ord` for `MyType`
```

### T005: Infinite type
```
error[T005]: infinite type detected
  --> src/main.tml:5:5
   |
 5 |     let x = x
   |         ^ recursive definition creates infinite type
```

### T006: Return type mismatch
```
error[T006]: return type mismatch
  --> src/main.tml:7:12
   |
 5 |     func get_value() -> String {
   |                         ^^^^^^ expected String
 6 |         let x = compute()
 7 |         return x
   |                ^ found I32
```

### T007: Option/Result not unwrapped
```
error[T007]: cannot use Option[I32] as I32
  --> src/main.tml:6:15
   |
 6 |     let y = x + 1
   |             ^ expected I32, found Option[I32]
   |
   = help: use `x.unwrap()` or handle None case with `when`
```

## 4. Semantic Errors (S)

### S001: Use of moved value
```
error[S001]: use of moved value
  --> src/main.tml:7:11
   |
 5 |     let s = String.from("hello")
 6 |     let t = s
   |             - value moved here
 7 |     print(s)
   |           ^ value used after move
   |
   = help: use `s.clone()` on line 6 if you need both
```

### S002: Borrow conflict
```
error[S002]: cannot borrow as mutable
  --> src/main.tml:7:15
   |
 5 |     let r1 = &data
   |              ----- immutable borrow here
 6 |     let r2 = &mut data
   |              ^^^^^^^^^ cannot borrow as mutable
 7 |     print(r1)
   |           -- immutable borrow used here
```

### S003: Dangling reference
```
error[S003]: returns reference to local variable
  --> src/main.tml:6:12
   |
 5 |     func bad() -> &String {
 6 |         let s = String.from("local")
 7 |         return &s
   |                ^^ returns reference to `s`
   |
   = note: `s` will be dropped at end of function
   = help: return owned `String` instead
```

### S004: Undefined variable
```
error[S004]: undefined variable
  --> src/main.tml:5:15
   |
 5 |     let y = x + 1
   |             ^ `x` not found in this scope
```

### S005: Immutable assignment
```
error[S005]: cannot assign to immutable variable
  --> src/main.tml:6:5
   |
 5 |     let x = 1
   |         - declared as immutable (use `var` for mutable)
 6 |     x = 2
   |     ^ cannot assign
```

### S006: Capability violation
```
error[S006]: capability not declared
  --> src/main.tml:8:9
   |
 3 |     module reader {
 4 |         caps: []
   |               -- no io capabilities
...
 8 |         File.read("data.txt")
   |         ^^^^^^^^^^^^^^^^^^^^^ requires `io.file.read`
   |
   = help: add `io.file.read` to module caps
```

### S007: Effect not declared
```
error[S007]: undeclared effect
  --> src/main.tml:5:5
   |
 5 |     func process() {
   |          ^^^^^^^ function has undeclared effects
...
 8 |         File.write(path, data)
   |         ---------------------- io.file.write effect
   |
   = help: add `effects: [io.file.write]` to function
```

### S008: Precondition violation
```
error[S008]: precondition may be violated
  --> src/main.tml:10:5
   |
 5 |     func sqrt(x: F64) -> F64
 6 |     pre: x >= 0.0
   |          ^^^^^^^^ precondition
...
10 |     sqrt(value)
   |     ^^^^^^^^^^^ cannot prove `value >= 0.0`
   |
   = help: add check: `if value >= 0.0 then sqrt(value) else ...`
```

### S009: Non-exhaustive patterns
```
error[S009]: non-exhaustive patterns
  --> src/main.tml:5:5
   |
 5 |     when status {
 6 |         Active -> "active",
 7 |         Inactive -> "inactive",
   |     }
   |     ^ pattern `Pending` not covered
   |
   = help: add `Pending -> ...` or `_ -> ...`
```

### S010: Unused variable
```
warning[S010]: unused variable
  --> src/main.tml:5:9
   |
 5 |     let result = compute()
   |         ^^^^^^ never used
   |
   = help: prefix with underscore: `_result`
```

### S011: Unreachable code
```
warning[S011]: unreachable code
  --> src/main.tml:7:5
   |
 5 |     return early
 6 |
 7 |     process()
   |     ^^^^^^^^^ this code will never execute
```

### S012: Private access
```
error[S012]: private field
  --> src/main.tml:10:15
   |
 3 |     type User {
 4 |         private password: String,
   |                 -------- declared private here
...
10 |     print(user.password)
   |               ^^^^^^^^ cannot access private field
```

## 5. JSON Format for LLMs

```json
{
  "errors": [
    {
      "code": "T001",
      "level": "error",
      "message": "type mismatch",
      "file": "src/main.tml",
      "line": 5,
      "column": 15,
      "stable_id": "@let_x",
      "expected_type": "String",
      "found_type": "I32",
      "suggestions": [
        {
          "message": "convert I32 to String",
          "replacement": "42.to_string()",
          "span": {
            "start": {"line": 5, "column": 24},
            "end": {"line": 5, "column": 26}
          }
        }
      ]
    }
  ],
  "warnings": [],
  "summary": {
    "errors": 1,
    "warnings": 0
  }
}
```

---

*Previous: [11-DEBUG.md](./11-DEBUG.md)*
*Next: [13-BUILTINS.md](./13-BUILTINS.md) — Builtin Types and Functions*
