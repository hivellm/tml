# Tasks: Automatic Drop System

**Status**: In Progress (25%)

## Phase 1: Str Auto-Free (Guaranteed Heap Allocations)

- [x] 1.1 Extend `DropInfo` struct with `is_heap_str` flag
- [x] 1.2 Add `register_heap_str_for_drop` method to LLVMIRGen
- [x] 1.3 Implement `emit_drop_call` for heap Str (null-guarded free)
- [x] 1.4 Add `is_heap_str_producer` heuristic (InterpolatedStringExpr, TemplateLiteralExpr)
- [x] 1.5 Wire up registration at `is_ptr` path and general fallback path in `gen_let_stmt`
- [x] 1.6 Wire up registration at `gen_nested_decl` (const declarations)
- [x] 1.7 Skip string literals and function/method call returns (may return global constants)
- [x] 1.8 Verify with IR output: interpolated strings get free(), literals do not
- [x] 1.9 Full test suite passes (7,869 tests, zero regressions)

## Phase 1b: Safe Str Auto-Free for Function Returns

- [ ] 1b.1 Implement `@tml_str_free` runtime function with heap validation
- [ ] 1b.2 Use `_msize` (Windows) / `malloc_usable_size` (Linux) to check heap ownership
- [ ] 1b.3 Replace `@free` with `@tml_str_free` in `emit_drop_call` for heap Str
- [ ] 1b.4 Extend `is_heap_str_producer` to include CallExpr and MethodCallExpr
- [ ] 1b.5 Write tests for function returns, method returns, string literal returns
- [ ] 1b.6 Run coverage, measure leak reduction

## Phase 2: needs_drop Recursive Analysis

- [ ] 2.1 Implement `needs_drop(type_name)` query in codegen
- [ ] 2.2 Recursive field check: if any field needs_drop, parent needs_drop
- [ ] 2.3 Auto-generate field-level drops for structs without `impl Drop`
- [ ] 2.4 Handle generic instantiations (e.g., `Wrapper[String]` needs drop)
- [ ] 2.5 Write tests for recursive drop scenarios

## Phase 3: Temporary Value Drops

- [ ] 3.1 Track temporary values from function returns
- [ ] 3.2 Drop temporaries at statement end
- [ ] 3.3 Handle method chains (`a.foo().bar()` — drop intermediate)
- [ ] 3.4 Write tests for temporary drops

## Phase 4: Validation

- [ ] 4.1 Run full test suite with coverage + leak detection
- [ ] 4.2 Verify zero (or near-zero) leaks
- [ ] 4.3 Commit and update roadmap
