---
name: optimize-ir
description: Optimize LLVM IR quality for a specific codegen pattern. Use when the user says "optimize ir", "otimiza ir", "improve codegen", or wants to make the compiler generate better LLVM IR.
user-invocable: true
argument-hint: "<pattern to optimize — e.g. 'struct construction', 'enum matching', 'method calls'>"
---

**Delegation**: Use the Agent tool to dispatch a `compiler-optimizer` agent with `model: opus` for this task.

## IR Optimization Workflow

### 1. Identify the Pattern

From `$ARGUMENTS`, determine which codegen pattern to optimize.

### 2. Rust-as-Reference Comparison

Follow the mandatory methodology from CLAUDE.md:
1. Write equivalent `.rs` and `.tml` files in `.sandbox/`
2. Compile both to IR (debug and release)
3. Compare function-by-function

### 3. Identify Inefficiencies

Look for:
- Extra `alloca`/`store`/`load` that Rust avoids
- Unnecessary function calls or wrappers
- Suboptimal type layouts (padding, alignment)
- Missing optimizations (insertvalue vs alloca+store)

### 4. Fix the Compiler C++

Edit the relevant codegen files in `compiler/src/codegen/` to produce better IR.

### 5. Verify

- Rebuild compiler (`/build`)
- Re-emit IR and confirm improvement
- Run affected tests (`/verify`)

### 6. Update Tracking

Add findings to `rulebook/tasks/optimize-codegen-like-rust/tasks.md`.
