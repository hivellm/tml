# BLOCKER #4 subsumed by #1 — closure capture works in AST path
**Source**: manual
**Date**: 2026-03-15
**Tags**: blocker, closure, capture, subsumed
Deep analysis confirms closure capture of function parameters works correctly in AST/legacy codegen. collect_codegen_captures at closure.cpp:42-115 handles IdentExpr, BinaryExpr, CallExpr, BlockExpr, IfExpr, MethodCallExpr, WhenExpr — missing IndexExpr, RangeExpr, TupleExpr, StructExpr, RefExpr, DerefExpr, CastExpr, AssignExpr, LoopExpr but these are rarely hit because the type checker's captured_vars list (line 164) is the primary capture source. The REAL issue is MIR codegen has NO closure support at all (build_closure stub). Fix #1 first; capture refinements are low priority.