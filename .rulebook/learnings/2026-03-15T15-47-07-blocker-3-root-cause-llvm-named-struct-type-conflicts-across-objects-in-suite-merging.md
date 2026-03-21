# BLOCKER #3 root cause: LLVM named struct type conflicts across objects in suite merging
**Source**: manual
**Date**: 2026-03-15
**Related Task**: fix-suite-codegen-bug
**Tags**: blocker, codegen, suite-merging, linking, llvm
When max_per_suite > 1, generic instantiation in File A may produce %struct.Repeat__I32 = type { i32 } while File B produces %struct.Repeat__I32 = type { ptr } (due to different generic resolution paths — base type alias vs full instantiation). LLD rejects conflicting definitions. Key file: llvm_struct_decl.cpp:210-295 where the alias path at lines 247-282 copies base type fields which may not match the specialization. Fix options: (A) opaque struct types resolved lazily, (B) always use full instantiation path (no alias shortcut) in suite mode, (C) keep max_per_suite=1 workaround. Option B is safest.