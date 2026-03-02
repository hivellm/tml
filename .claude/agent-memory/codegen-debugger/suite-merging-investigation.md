# Suite Merging Codegen Bug Investigation (2026-03-01)

## Two Bugs Found

### Bug 1: %struct.This / %struct.T (DEFUNCT)
- **Origin**: Old suite-merging test system (deleted tester/ directory)
- **Artifacts**: `.sandbox/failed_ir_dump.ll`, `.sandbox/full_cov_stderr.txt`
- **Root cause**: Shared LLVMIRGen across files -> stale current_impl_type_ and current_type_subs_
- **Fallback path**: llvm_types.cpp:755-759 produces %struct.This when current_impl_type_ empty
- **No longer reproducible**: v3 test system uses per-file QueryContext

### Bug 2: toowned_assoc.test.tml (ACTIVE)
- **Error**: `'%this' defined with type 'i32' but expected 'ptr'`
- **Root cause**: Primitive this override at generate.cpp:1006-1011 ignores `ref This` declaration
- **Conflation chain**:
  - Library: `to_owned(this)` -> i32 %this -> body: ret i32 %this
  - Local: `to_owned(this: ref This)` -> should be ptr %this -> body: load i32, ptr %this
  - Codegen: forces i32 %this (from primitive override) + body uses load i32, ptr %this (BROKEN)
- **Fix location**: generate.cpp:1006-1011, impl.cpp:234-246
- **Fix**: Check `method.params[i].type->is<parser::RefType>()` before applying primitive by-value

## User's Hypotheses Assessment
- (a) current_impl_type_ empty: Correct for old Bug 1, not for Bug 2
- (b) current_type_subs_ stale: Correct for %struct.T in old system
- (c) Symbol collision in suite merging: Not applicable to current system
- (d) force_internal_linkage/lazy ordering: Not the root cause

## Key Files Examined
- llvm_types.cpp:755-759 (This/Self fallback)
- impl.cpp:190 (current_impl_type_ set), 285-292 (lazy deferred path)
- generate.cpp:943-1097 (local impl codegen with this override)
- runtime_modules.cpp:1061-1175 (lazy deferred body generation)
- llvm_utils.cpp:372-382 (builtin_modules for name mangling)
- llvm_utils.cpp:420-433 (mangle_impl_method)
- parser_decl_impl.cpp:814-815 (bare this -> NamedType("This"))
