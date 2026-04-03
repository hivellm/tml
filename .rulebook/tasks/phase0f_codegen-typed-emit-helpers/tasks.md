# Tasks: Typed Emit Helpers — Validated IR Emission

**Status**: COMPLETE. 100% (15/15). **Priority**: MEDIUM
**Reference**: `docs/analyses/codegen/06-IR-GENERATION-STRATEGY.md` (Option B)

## 1. IREmitter Class

- [x] 1.1 Create `compiler/include/codegen/ir_emitter.hpp`
- [x] 1.2 Add `emit_load(type, ptr, volatile)` — returns reg, asserts type != void. Also `emit_load_to` for pre-assigned regs.
- [x] 1.3 Add `emit_store(type, value, ptr, volatile)` — asserts type != void, skips unit "{}"
- [x] 1.4 Add `emit_alloca(type, align)` — returns reg, asserts type != void. Also `emit_alloca_to` for pre-assigned regs.
- [x] 1.5 Add `emit_gep(base_type, ptr, indices, inbounds)` — returns reg. Also `emit_gep_to` for pre-assigned regs.
- [x] 1.6 Add `emit_call(ret_type, func, typed_args)` — returns reg or empty for void
- [x] 1.7 Add `emit_memcpy(dst, src, size, volatile)`, `emit_memset()`, `emit_memmove()`
- [x] 1.8 Build — verify compiles

## 2. Integration (Incremental)

- [x] 2.1 Add `IREmitter emitter_` to MirCodegen, wire to `output_` stream and `temp_counter_`
- [x] 2.2 Convert `LoadInst` handler to use `emitter_.emit_load_to()`
- [x] 2.3 Convert `StoreInst` handler to use `emitter_.emit_store()`
- [x] 2.4 Convert `AllocaInst` handler to use `emitter_.emit_alloca_to()`
- [x] 2.5 Run `core/str` 25/25, `core/fmt` 46/46, `core/ops` 47/47, `core/num` 53/53 — zero regressions

## 3. Expand Coverage

- [x] 3.1 Convert GEP emissions to `emitter_.emit_gep_to()` (main handler + spill alloca/store)
- [x] 3.2 Convert memcpy/memset/memmove calls in `instructions_call.cpp` to `emitter_.emit_memcpy()` etc.
- [x] 3.3 Verified: core/str 25/25, core/fmt 46/46, core/ops 47/47, core/num 53/53, core/slice 25/25, core/error 35/35 — zero regressions

## Design Notes

- Added `_to` variants (emit_load_to, emit_alloca_to, emit_gep_to) because MIR instructions use pre-assigned register names (%v<N>) not fresh temps
- IREmitter holds references to MirCodegen's `output_` (std::stringstream) and `temp_counter_` (int)
- The `emit_store` method handles unit type "{}" skip internally (emits comment instead of store)
- The `emit_load_to` asserts type != "void" and type != "{}" at emit time
