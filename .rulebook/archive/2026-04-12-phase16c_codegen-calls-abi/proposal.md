# Proposal: Codegen Calls & ABI — Rewrite in TML

## Why

Function calls are the highest-risk area of LLVM IR codegen. Correct call emission requires
matching the platform ABI exactly: which arguments go in registers, which are passed by pointer,
whether the return value uses an sret slot, and how large structs are split across register pairs.
Any mismatch between a call site and its callee declaration produces silent data corruption or a
crash at runtime. The C++ implementation handles this in eight files totaling approximately 15K
LOC: `instructions_call.cpp` (1,236), `instructions_method.cpp`, `method.cpp` (1,597),
`method_impl.cpp` (1,499), `method_static_dispatch.cpp` (1,496), `method_outcome.cpp` (1,371),
`call_generic_struct.cpp` (1,124), and `generic_instantiate_impl.cpp` (1,539). Porting this
layer to TML completes the MIR codegen path and enables full end-to-end IR-diff testing.

## What Changes

The C++ call emission files are replaced by a TML implementation in
`compiler-tml/src/codegen/emit_call.tml` and `emit_method.tml`. The `GenericInstantiator` in
`emit_generic.tml` handles on-demand emission of generic instantiations.

### Architecture

```
compiler-tml/src/codegen/
  emit_call.tml    — CallEmitter: emit_call(MirCallInst) -> Text
                     abi_class(), cc_for(), sret/byval decisions (Phase 1-2)
                     Win64 vs SysV ABI selection (Phase 6)
  emit_method.tml  — MethodCallEmitter: inherent, behavior, auto-deref, vtable (Phase 3)
  emit_generic.tml — GenericInstantiator: on-demand generic function emission (Phase 4)
```

### Key Design Decisions

- **ABI classification first, emission second** — before emitting any call instruction, the
  emitter classifies each argument and the return type using `abi_class()`. This separation
  matches the C++ structure in `abi.hpp` and makes platform-specific decisions testable in
  isolation without requiring full function compilation.
- **sret is the caller's responsibility** — when a function returns a struct larger than 16
  bytes, the caller allocates the sret stack slot (alloca), passes it as the first argument
  with the `sret` annotation, and then reads the result from the slot after the call returns.
  The TML emitter must allocate this slot before emitting the call instruction, exactly as the
  C++ `emit_call_inst()` does. Getting this wrong causes the callee to write to a garbage address.
- **Method dispatch priority: inherent > behavior > auto-deref** — when resolving `obj.method()`,
  the emitter first checks for an inherent impl on the concrete type, then checks behavior impls,
  then auto-derefs once and repeats. This three-step priority is identical to C++ `method.cpp` and
  must not be changed — reordering causes wrong method selection for types that implement both
  inherent and behavior versions of a name.
- **Generic instantiation is on-demand** — when a call to `List[I64]::push` is encountered for
  the first time, `GenericInstantiator` emits the full concrete function body for that
  instantiation before the call instruction. Subsequent calls to the same instantiation skip
  emission and reuse the already-defined function. The `HashMap[Str, Bool]` tracking set uses
  the mangled function name as the key.
- **Win64 and SysV differ in struct-passing rules** — on Win64, any struct larger than 8 bytes is
  passed by pointer unconditionally. On SysV, structs up to 16 bytes may be split into two
  8-byte eightbytes and passed in register pairs. The TML emitter must branch on
  `CodegenConfig::target_triple` to select the correct rule. Using the wrong rule on either
  platform produces ABI mismatches when calling C runtime functions.
- **Outcome `?` operator as MIR instruction** — the `?` operator is not a call; it is a MIR
  control flow instruction (`CheckOutcome`) that the MIR builder emits. The call emitter handles
  it as a pattern: extract discriminant, conditional branch to error block, extract payload.
  This is tested separately from regular calls because it involves a branch, not just a call.

### ABI Summary Table

| Case | Win64 | SysV |
|---|---|---|
| Struct ≤ 8 bytes | Pass in register | Pass in register |
| Struct 9–16 bytes | Pass by pointer (caller copy) | Split into 2 registers if possible |
| Struct > 16 bytes | Pass by pointer (caller copy) | Pass by pointer (caller copy) |
| Return struct ≤ 8 bytes | RAX | RAX |
| Return struct 9–16 bytes | Pointer (hidden first arg) | RAX + RDX |
| Return struct > 16 bytes | Pointer (sret hidden first arg) | Pointer (sret hidden first arg) |

## Impact

- Affected code: `instructions_call.cpp`, `instructions_method.cpp`, `method.cpp`,
  `method_impl.cpp`, `method_static_dispatch.cpp`, `method_outcome.cpp`,
  `call_generic_struct.cpp`, `generic_instantiate_impl.cpp` (all replaced)
- Affected phases: 16d (legacy path uses same CallEmitter for runtime calls)
- Breaking change: NO — IR-diff testing ensures call instructions are character-identical to C++
- User benefit: self-hosting progress; full MIR→LLVM IR pipeline available in TML

## Success Criteria

All call instruction forms (direct, sret, byval, method, virtual, generic, Outcome `?`) produce
LLVM IR that is character-identical to C++ output. IR-diff on 15 stdlib functions shows zero
differences in call instructions. Win64 and SysV ABI unit tests both pass on their respective
target triples.

## Dependencies

- **Requires**: phase16b (InstructionEmitter for GEP/load/store/alloca in call preamble),
  phase16a (sret slot layout from LayoutComputer, ABI struct-size thresholds)
- **Blocks**: phase16d (legacy LLVM path reuses CallEmitter for runtime calls)
- **Risk**: HIGH — ABI rules are platform-specific and subtle; a single wrong threshold (e.g.,
  8 vs 16 bytes for Win64 struct-by-pointer decision) corrupts all calls to runtime functions.
  Mitigated by per-ABI-class unit tests in item 7.1 that verify each classification case before
  any full-function IR-diff testing.
