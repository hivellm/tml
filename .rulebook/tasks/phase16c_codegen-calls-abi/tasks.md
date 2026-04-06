# Tasks: Codegen Calls & ABI — Rewrite in TML

**Status**: Planned (0/25)
**Depends on**: phase16b (instruction emitter available), phase16a (sret/byval layout decisions)
**Blocks**: phase16d (legacy path needs call layer for shared runtime calls)
**Duration**: 6–8 weeks
**Risk**: HIGH — ABI rules for struct passing are subtle; sret/byval mismatch causes silent corruption
**C++ reference**: ~15K LOC → ~9.8K TML

---

## Phase 1: Call Infrastructure (3 items)

- [ ] 1.1 Create `compiler-tml/src/codegen/emit_call.tml` — `CallEmitter` struct extending `InstructionEmitter`; `emit_call(call: MirCallInst) -> Text` entry point
- [ ] 1.2 Implement ABI classification: `abi_class(t: MirType) -> AbiClass` returning `Integer`, `Sse`, `Memory`, `Sret` — matches C++ Win64/SysV classification in `abi.hpp`
- [ ] 1.3 Implement calling convention selection: `cc_for(func: MirFunc) -> Text` → `"fastcc"`, `"ccc"`, `"win64cc"` from func attributes; `@extern("c")` always gets `"ccc"`

## Phase 2: Direct Calls (4 items)

- [ ] 2.1 Implement direct value-return call: `%r = call fastcc i64 @func_name(i64 %a, i64 %b)` — simple case, no sret, no byval
- [ ] 2.2 Implement sret call: caller allocates `%sret = alloca %struct.T, align 8`, emits `call fastcc void @func(ptr sret(%struct.T) align 8 %sret, ...)`, result is `%sret` pointer
- [ ] 2.3 Implement byval argument: struct arg ≤ 16 bytes → copy to stack slot, pass `byval(%struct.T) align 8` annotation; struct arg > 16 bytes → pass pointer (caller-allocated copy)
- [ ] 2.4 Implement void call (unit return): `call fastcc void @func(...)` or `call fastcc {} @func(...)` depending on function signature — must match callee declaration exactly

## Phase 3: Method Dispatch (5 items)

- [ ] 3.1 Create `compiler-tml/src/codegen/emit_method.tml` — `MethodCallEmitter`: resolves method to mangled name, selects dispatch strategy
- [ ] 3.2 Implement inherent method dispatch: `obj.method(args)` → mangle to `@TypeName__method`, emit direct call — highest priority in dispatch order
- [ ] 3.3 Implement behavior (trait) dispatch: `obj.behavior_method(args)` → look up concrete impl type in method registry, mangle to `@ConcreteType__BehaviorName__method`, emit direct call
- [ ] 3.4 Implement auto-deref dispatch: `ref_to_obj.method(args)` → emit `load` to get value, then dispatch as inherent — matches C++ `method.cpp` auto-deref chain
- [ ] 3.5 Implement virtual dispatch via vtable pointer: behavior object `(ptr data, ptr vtable)` → `%fn_ptr = getelementptr vtable, i32 0, i32 slot`, `call ptr %fn_ptr(ptr %data, ...)`

## Phase 4: Generic Instantiation (4 items)

- [ ] 4.1 Create `compiler-tml/src/codegen/emit_generic.tml` — `GenericInstantiator`: tracks emitted generic instantiations to avoid duplicate definitions
- [ ] 4.2 Implement generic struct instantiation: when a generic method is called (e.g., `List[I64]::push`), emit the concrete function definition if not already emitted; mangle as `@List_I64__push`
- [ ] 4.3 Implement generic function instantiation: `sort[I64](list)` → mangle to `@sort_I64`, emit concrete definition substituting `I64` for type param `T` throughout the function body
- [ ] 4.4 Implement instantiation deduplication: `HashMap[Str, Bool]` called from 5 modules → emit `@HashMap_Str_Bool__insert` exactly once; use `HashMap[Str, Bool]` (the set) to track emitted instantiations

## Phase 5: Outcome Method Chains (3 items)

- [ ] 5.1 Implement `Outcome[T,E]` method calls: `result.unwrap()`, `result.map(f)`, `result.and_then(f)` — these are standard inherent methods; mangle and dispatch as regular calls
- [ ] 5.2 Implement `?` operator lowering: `expr?` in the MIR appears as `CheckOutcome(expr, error_bb, ok_val)` — emit: extract discriminant, `icmp eq i32 %disc, 1` (error), `condBr` to error block; in error block extract error value and return it wrapped in `Err`
- [ ] 5.3 Implement `Maybe[T]` method calls: `maybe.unwrap()`, `maybe.map(f)`, `maybe.or_else(f)` — same dispatch as Outcome; discriminant is `i32` at offset 0

## Phase 6: Win64 vs SysV ABI (3 items)

- [ ] 6.1 Implement Win64 ABI: integer/pointer args in `RCX`, `RDX`, `R8`, `R9` (first 4); rest on stack; structs > 8 bytes always by pointer; return struct > 8 bytes via hidden first pointer arg
- [ ] 6.2 Implement SysV ABI: integer args in `RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9` (first 6); struct ≤ 16 bytes split into two eightbytes, passed in registers if possible; return struct ≤ 16 bytes in RAX+RDX
- [ ] 6.3 Select ABI from `CodegenConfig::target_triple`: `x86_64-pc-windows-*` → Win64; `x86_64-*-linux-*` → SysV; ABI selection happens in `abi_class()` (item 1.2)

## Phase 7: Differential Testing (3 items)

- [ ] 7.1 Create `compiler-tml/tests/codegen/calls.test.tml` — unit tests: direct call with sret, byval struct arg, void call, generic instantiation — assert emitted IR string exactly
- [ ] 7.2 Create `compiler-tml/tests/codegen/dispatch.test.tml` — method dispatch tests: inherent, behavior, auto-deref, vtable — assert mangled name and call instruction form
- [ ] 7.3 IR-diff: compile 15 stdlib functions using various call patterns → compare call instructions against C++ `instructions_call.cpp` and `method.cpp` output; zero differences required
