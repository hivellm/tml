# BLOCKER #1 root cause: ThirMirBuilder::build_closure is an unimplemented stub
**Source**: manual
**Date**: 2026-03-15
**Related Task**: stdlib-essentials
**Tags**: blocker, codegen, closure, mir, critical
The #1 blocking bug is at thir_mir_builder_expr.cpp:202-203: build_closure() returns const_unit() — a complete stub. No ClosureInitInst is emitted, no closure function generated. This causes lambda args to become '{} zeroinitializer' and cascades into 'ret void' for functions that should return i32. THREE bugs form a chain: (1) build_closure stub, (2) type checker rejects Closure→func conversion, (3) emit_closure_init_inst at instructions_misc.cpp:627-631 has TODO for capturing closures. Fix approach: implement build_closure using HirMirBuilder version at hir_expr_control.cpp:328-431 as reference. This single fix unblocks ALL of stdlib-essentials Phase 2 (Vec::retain, Vec::from_iter, HashSet::from_iter, Distribution).