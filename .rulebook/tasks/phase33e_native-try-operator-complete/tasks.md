## 1. Implementation
- [ ] 1.1 Fix chained try in `emit_call.tml`: when a `?` application is itself the receiver of a method call or another `?`, propagate the extracted `Ok` payload as the new value through successive applications without re-wrapping in an intermediate `Outcome`
- [ ] 1.2 Add error wrapping via `From::from` in `emit_call.tml`: detect when the `Err` variant's type differs from the enclosing function's declared error type, look up the `From` impl in the type environment, and emit a call to `From::from(err)` before constructing the returned `Err` value
- [ ] 1.3 Handle try inside loop bodies in `emit_call.tml`: when a `?` check branch is inside a loop, emit the early-return block so it exits the enclosing function rather than the loop; ensure loop continuation and loop-exit blocks are not confused with the try-fail block
- [ ] 1.4 Write `compiler-tml/tests/codegen/try_operator.test.tml` covering: simple single `?` success path, simple single `?` failure early-return, chained `foo()?.bar()` success and failure, `From` error wrapping between distinct error types, `?` inside a loop body that exits the function on first error

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
