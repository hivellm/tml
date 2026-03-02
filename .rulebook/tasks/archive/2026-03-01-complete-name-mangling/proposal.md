# Proposal: complete-name-mangling

## Why

The previous `implement-name-mangling` task (archived 2026-02-28) implemented Itanium-style mangling **only for free module-level functions**. All other symbol categories still use flat `tml_<Type>_<method>` naming with no module path, creating guaranteed symbol collisions:

### Current state (after Phase 1-7 of previous task):

| Category | Current symbol | Has module path? | Collision risk |
|----------|---------------|-----------------|----------------|
| Module free functions | `@tml_N4core3str5splitE_SS` | YES | None |
| Generic library functions | `@tml_N4core4iter4takeE__I32_h7f8a9b1c` | YES | None |
| Script-level functions | `@tml_foo` or `@tml_s<N>_foo` | NO | HIGH |
| Impl methods (non-generic) | `@tml_Type_method` | NO | HIGH |
| Impl methods (generic) | `@tml_Type__T_method` | NO | HIGH |
| Class methods | `@tml_Type_method` (same codegen as impl) | NO | HIGH |
| Closures | `@tml_closure_<N>` | NO | MEDIUM |

### Concrete collision scenarios:

1. **Two scripts** with `func process()` both emit `@tml_process` → linker error
2. **Two modules** defining `struct Parser` + `impl Parser { func parse() }` both emit `@tml_Parser_parse` → linker error or silent wrong dispatch
3. **Class methods** have identical problem to impl methods — same codegen path (`impl.cpp` line 274)
4. **Closures** use a global counter `closure_<N>` — when linking multiple compilation units, counters overlap

### Incomplete items from previous task:

- [ ] 1.3 Remove `max_per_suite=1` workaround from `exe_suite_runner.cpp`
- [ ] 1.4 Remove `has_compiler_tests` workaround from `suite_execution.cpp`
- [ ] 1.5 Run `tml test --coverage --no-cache` and confirm 0 failures in suite mode
- [ ] 2.13 Run full test suite (`--no-cache`) and confirm valid IR
- [ ] 3.8 Run full test suite (`--no-cache`) and confirm valid IR
- [ ] 7.5 Fix `iter_chain`/`iter_cycle` etc. — user-defined impl methods in generic contexts

## What Changes

### Phase 1: Impl Method Mangling (Non-Generic)

Apply Itanium-style mangling to impl methods. The symbol should encode the module of the type's origin:

```
Before:  @tml_Str_split          (ambiguous — which module defines Str?)
After:   @tml_N4core3str3Str5splitE_SS
```

Changes in `impl.cpp`:
- Line 274: `func_llvm_name = "tml_" + type_name + "_" + method.name` → use `mangle_tml_symbol(module, type_name + "::" + method.name, params)`
- Update all call sites that resolve impl methods: `method_impl.cpp`, `method_generic.cpp`, `call_user.cpp`
- Update `functions_` map registration keys to use mangled names

### Phase 2: Generic Impl Method Mangling

Apply same scheme to generic instantiated methods:

```
Before:  @tml_List__I32_push
After:   @tml_N4core4list4List4pushE__I32_h<hash>
```

Changes in `impl.cpp`:
- Line 830: `func_llvm_name = "tml_" + suite_prefix + mangled_type_name + "_" + full_method_name` → include module path

### Phase 3: Script-Level Function Mangling — DEFERRED

Script functions are already disambiguated by existing mechanisms:
- Suite mode uses `get_suite_prefix()` to produce `@tml_s<N>_foo` (unique per test index)
- Individual mode compiles each file to its own DLL with `internal` linkage
- Standalone builds are single compilation units with no collision risk

Filename-based namespace would break function lookup in 15+ codegen call sites and test harness discovery. Revisit only if multi-script linking without suite mode becomes a requirement.

### Phase 4: Closure Mangling

Include parent function context in closure names:

```
Before:  @tml_closure_0     (global counter, collides across compilation units)
After:   @tml_N4core3str5splitE_closure_0   (scoped to parent function)
```

### Phase 5: Cleanup and Validation

- Remove `max_per_suite=1` workaround
- Remove `has_compiler_tests` workaround
- Run full suite in suite mode (8 per DLL) with 0 failures
- Run coverage mode with 0 failures
- Fix `iter_chain`/`iter_cycle` generic context resolution

### Phase 6: Unify Mangling Systems

Currently two separate type-to-string systems coexist:
- `mangle_type_code` (Itanium single-letter: `i`, `l`, `S`) — used for free functions
- `mangle_type` (PascalCase: `I32`, `List__I32`) — used for struct/impl names

Unify into a single consistent scheme.

## Impact

- **Affected specs**: `docs/specs/08-IR.md` (mangling spec section)
- **Affected code**:
  - `compiler/src/codegen/llvm/decl/impl.cpp` — impl method name generation (PRIMARY)
  - `compiler/src/codegen/llvm/decl/func.cpp` — script function names
  - `compiler/src/codegen/llvm/expr/method_impl.cpp` — method call resolution
  - `compiler/src/codegen/llvm/expr/method_generic.cpp` — generic method call resolution
  - `compiler/src/codegen/llvm/expr/call_user.cpp` — call site resolution
  - `compiler/src/codegen/llvm/core/llvm_utils.cpp` — mangling utilities
  - `compiler/src/codegen/llvm/core/llvm_types.cpp` — type mangling unification
  - `compiler/src/codegen/llvm/core/generic.cpp` — generic instantiation naming
  - `compiler/src/codegen/llvm/expr/closure.cpp` — closure naming
- **Breaking change**: YES — all impl method LLVM symbol names change (internal only, no user-visible API impact)
- **User benefit**: Eliminates symbol collisions for multi-file projects, enables suite mode without workarounds, unblocks future module system improvements

## Success Criteria

- ALL `define` statements in LLVM IR use Itanium-style mangled names (except `@no_mangle` and `@extern`)
- `tml test --coverage --no-cache` passes with `max_per_suite=8` (no workaround)
- Two scripts defining identical function names can be linked together without collision
- Two modules defining identical struct names + impl methods produce distinct symbols
- `tml demangle` can decode all symbol categories (free functions, impl methods, generics, closures)