# State leakage is the #1 codegen bug category
**Source**: manual
**Date**: 2026-03-15
**Related Task**: codegen-structural-fixes
**Tags**: codegen, bugs, state-leakage, legacy
LLVMIRGen has 50+ mutable fields with no scope discipline. The bee67287 fix (last_semantic_type_ leaking across function boundaries) is a symptom, not a cure. Root cause: fields like last_expr_type_, last_semantic_type_, expected_enum_type_ are set by one codegen function and read by another with no guarantee the reader is in the same scope. The correct fix is decomposing LLVMIRGen into scoped context objects passed explicitly as function parameters. Until then, every new codegen feature risks introducing another state-leak bug.