# Proposal: Typed Emit Helpers — Validated IR Emission

## Why

The codegen generates LLVM IR by concatenating strings (`emitln("load " + type + ", ptr " + ptr)`). This is the IR equivalent of SQL string injection — no type checking, no structural validation. Malformed IR is only caught when LLVM parses the text, producing cryptic errors far from the source. Rust and Clang use typed LLVM C API / IRBuilder where type mismatches are caught at the API call. A full migration to LLVM C API is too expensive now, but typed emit helpers provide 80% of the benefit at 10% of the cost.

## What Changes

1. **IREmitter class** (`compiler/include/codegen/ir_emitter.hpp`):
   - `emit_load(type, ptr, volatile)` — asserts type is not void/empty
   - `emit_store(type, value, ptr, volatile)` — asserts type is not void/empty
   - `emit_alloca(type, align)` — asserts type is not void/empty
   - `emit_gep(base_type, ptr, indices)` — validates indices non-empty
   - `emit_call(ret_type, func, typed_args)` — handles void returns
   - `emit_memcpy/memset/memmove()` — wraps LLVM intrinsics

2. **Incremental integration**: IREmitter added to MirCodegen, instruction handlers converted one at a time (LoadInst, StoreInst, AllocaInst first, then GEP, then calls).

3. **Assertions catch bugs early**: void loads, empty types, and null pointers caught at C++ assertion time instead of LLVM parse time.

## Impact

- Affected specs: None (internal compiler change)
- Affected code: `compiler/include/codegen/ir_emitter.hpp` (new), `compiler/include/codegen/mir_codegen.hpp` (new member), `compiler/src/codegen/mir/instructions.cpp` (converted handlers)
- Breaking change: NO — purely additive, same LLVM IR output
- User benefit: Faster diagnosis of codegen bugs; impossible to emit void loads/stores
