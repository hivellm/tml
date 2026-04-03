# Tasks: Typed Emit Helpers — Validated IR Emission

**Status**: New. 0% (0/15). **Priority**: MEDIUM
**Reference**: `docs/analyses/codegen/06-IR-GENERATION-STRATEGY.md` (Option B)

## 1. IREmitter Class

- [ ] 1.1 Create `compiler/include/codegen/ir_emitter.hpp`
- [ ] 1.2 Add `emit_load(type, ptr, volatile)` — returns reg, asserts type != void
- [ ] 1.3 Add `emit_store(type, value, ptr, volatile)` — asserts type != void
- [ ] 1.4 Add `emit_alloca(type, align)` — returns reg, asserts type != void
- [ ] 1.5 Add `emit_gep(base_type, ptr, indices, inbounds)` — returns reg
- [ ] 1.6 Add `emit_call(ret_type, func, typed_args)` — returns reg or empty for void
- [ ] 1.7 Add `emit_memcpy(dst, src, size, volatile)`, `emit_memset()`, `emit_memmove()`
- [ ] 1.8 Build — verify compiles

## 2. Integration (Incremental)

- [ ] 2.1 Add `IREmitter emitter_` to MirCodegen, wire to `output_` stream
- [ ] 2.2 Convert `LoadInst` handler to use `emitter_.emit_load()`
- [ ] 2.3 Convert `StoreInst` handler to use `emitter_.emit_store()`
- [ ] 2.4 Convert `AllocaInst` handler to use `emitter_.emit_alloca()`
- [ ] 2.5 Run `core/str` + `core/fmt` — verify no regressions

## 3. Expand Coverage

- [ ] 3.1 Convert GEP emissions to `emitter_.emit_gep()`
- [ ] 3.2 Convert memcpy/memset/memmove calls to `emitter_.emit_memcpy()` etc.
- [ ] 3.3 Run full test suite — verify zero regressions
