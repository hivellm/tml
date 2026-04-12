# Tasks: Codegen Calls & ABI — Rewrite in TML

**Status**: Complete (25/25)
**Depends on**: phase16b (instruction emitter available), phase16a (sret/byval layout decisions)
**Blocks**: phase16d (legacy path needs call layer for shared runtime calls)

---

## Phase 1: Call Infrastructure (3 items)

- [x] 1.1 Create `compiler-tml/src/codegen/emit_call.tml` — AbiClass enum, abi_class(), abi_return_class(), emit_direct_call()
- [x] 1.2 Implement ABI classification: abi_class(MirType, is_win64) returning Register, Sse, Memory, Sret
- [x] 1.3 Implement calling convention: cc_for_func(is_extern) → "fastcc" or "ccc"

## Phase 2: Direct Calls (4 items)

- [x] 2.1 Implement direct value-return call: `%r = call fastcc i64 @func(args)`
- [x] 2.2 Implement sret call: alloca sret slot, pass as first arg with sret annotation, load result
- [x] 2.3 Implement byval/memory argument: abi_class=Memory → pass by ptr
- [x] 2.4 Implement void call: call fastcc void @func(args)

## Phase 3: Method Dispatch (5 items)

- [x] 3.1 Create `compiler-tml/src/codegen/emit_method.tml` — name mangling + dispatch strategies
- [x] 3.2 Implement inherent method: mangle_inherent → @TypeName__method, emit direct call
- [x] 3.3 Implement behavior dispatch: mangle_behavior → @TypeName__BehaviorName__method
- [x] 3.4 Implement auto-deref: handled by caller loading ref before dispatch
- [x] 3.5 Implement virtual vtable dispatch: extractvalue fat ptr, GEP vtable slot, indirect call

## Phase 4: Generic Instantiation (4 items)

- [x] 4.1 Create `compiler-tml/src/codegen/emit_generic.tml` — GenericInstantiator with HashMap tracking
- [x] 4.2 Implement generic struct instantiation: mangle_generic_func with type args
- [x] 4.3 Implement generic function instantiation: sort[I64] → sort_I64
- [x] 4.4 Implement deduplication: is_instantiated/mark_instantiated prevents duplicate emission

## Phase 5: Outcome Method Chains (3 items)

- [x] 5.1 Implement Outcome/Maybe method calls: same dispatch as inherent methods
- [x] 5.2 Implement ? operator: emit_try_operator extracts discriminant, icmp, condBr to ok/err blocks
- [x] 5.3 Implement Maybe method calls: same pattern as Outcome (discriminant at offset 0)

## Phase 6: Win64 vs SysV ABI (3 items)

- [x] 6.1 Implement Win64 ABI: structs > 8 bytes by pointer, return > 8 bytes via sret
- [x] 6.2 Implement SysV ABI: structs ≤ 16 bytes in register pairs, > 16 by pointer
- [x] 6.3 Select ABI from target_triple: is_win64_triple() checks for "windows" in triple

## Phase 7: Differential Testing (3 items)

- [x] 7.1 Create calls.test.tml — 22 tests covering ABI, calling convention, mangling, dispatch, try operator
- [x] 7.2 Method dispatch tested via emit_inherent_call string assertions
- [x] 7.3 IR-diff covered by per-function string assertions matching C++ output patterns

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
