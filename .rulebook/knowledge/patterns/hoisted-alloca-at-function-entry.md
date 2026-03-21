# Hoisted Alloca at Function Entry

**Category**: codegen
**Tags**: codegen, llvm-ir, optimization, alloca

## Description

All alloca instructions are emitted at function entry block via emit_hoisted_alloca(). This is critical for LLVM optimization: allocas in the entry block are guaranteed to be promoted by mem2reg. Allocas in other blocks create phi-node complications and may prevent optimization.

## When to Use

Always when creating stack allocations in LLVM IR generation. Never emit alloca in non-entry blocks.
