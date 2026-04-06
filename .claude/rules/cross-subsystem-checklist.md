# Cross-Subsystem Implementation Checklist

When implementing a change that touches 2+ subsystems, you MUST follow this checklist. This prevents the #1 failure mode: making a locally-correct change that breaks something downstream.

## BEFORE Writing Any Code

1. **Identify all affected subsystems** — Use the Architecture Map to trace upstream and downstream
2. **Read the boundary types** — What data format enters and exits each affected subsystem?
3. **Write down the invariants** — What does each subsystem EXPECT from its input?
4. **Check THIR→MIR path** — MIR now has a SINGLE builder (THIR→MIR only, consolidated in phase12a).

## DURING Implementation

5. **Change ONE subsystem at a time** — Verify each change before moving to the next
6. **Emit intermediate output** — Use `emit-mir` and `emit-ir` after each stage change to verify
7. **Never assume** — If unsure how a type flows downstream, READ the consuming code first
8. **Track what you changed** — Keep a mental list: "I changed X in file A, this produces Y, consumed by Z in file B"

## AFTER Implementation

9. **Verify with smallest test first** — Run a single affected test, not the full suite
10. **Check IR output** — Use `emit-ir` to confirm the generated LLVM IR is correct
11. **Run affected suite** — Use `mcp__tml__test` with `suite` parameter
12. **Update agent memory** — If you discovered a new cross-subsystem pattern, save it

## Common Cross-Subsystem Failure Patterns

### Pattern 1: Type Shape Mismatch
**Symptom**: LLVM verification error, "invalid type for function argument"
**Cause**: One subsystem produces a value of type A, downstream expects type B
**Fix**: Trace the type through each stage: TypeChecker → HIR → MIR → LLVM IR

### Pattern 2: THIR→MIR Path (Single Path)
**Note**: As of phase12a, there is only ONE MIR builder path (THIR→MIR).
The legacy HIR→MIR path has been removed. All MIR fixes go in:
  - `compiler/src/mir/thir_mir_builder.cpp` + `thir_mir_builder_expr.cpp`

### Pattern 3: Monomorphization Cache Miss
**Symptom**: Generic type works for some instantiations but not others
**Cause**: HIR monomorphization didn't generate all needed specializations
**Fix**: Check `hir_builder.cpp` monomorphization logic for the specific generic pattern

### Pattern 4: MIR Codegen Method Dispatch
**Symptom**: Method call generates wrong code or crashes
**Cause**: MIR path uses CallInst for methods (NOT MethodCallInst)
**Fix**: All method dispatch for MIR path must go through `emit_call_inst` in `instructions.cpp`

### Pattern 5: Runtime ABI Mismatch
**Symptom**: Crash at runtime, garbage values, segfault
**Cause**: TML-generated call doesn't match C runtime function signature
**Fix**: Compare the `@extern("c")` declaration in TML with the actual C function signature

### Pattern 6: sret Convention Mismatch
**Symptom**: Struct return value is garbage or causes crash
**Cause**: Caller uses sret but callee doesn't (or vice versa)
**Fix**: Check both `emit_call_inst` (caller side) and function declaration (callee side) for consistent sret usage
