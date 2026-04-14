# llvm.assume for for-in range loop bounds

**Category**: codegen
**Tags**: codegen, llvm, for-in, vectorization, LICM, bounds-check, optimization, loop

## Description

For `for i in 0 to n` loops, emit `llvm.assume(i ult n)` at the start of the loop body to inform LLVM the index is always in [0, n). This enables LICM of loop-invariant loads inside inlined methods (e.g. List.get's stride/data_addr header loads) and allows the auto-vectorizer to treat the loop as safe without needing a bounds check in the callee.

## Example

// In gen_for(), after body block entry, before gen_expr(*for_expr.body):
if (range_start == "0" && range_type != "i1") {
    std::string assume_i = fresh_reg();
    emit_line("  " + assume_i + " = load " + range_type + ", ptr " + var_alloca);
    std::string assume_cond = fresh_reg();
    emit_line("  " + assume_cond + " = icmp ult " + range_type + " " + assume_i + ", " + range_end);
    emit_line("  call void @llvm.assume(i1 " + assume_cond + ")");
}

## When to Use

Any for-in range loop starting from 0 in the AST codegen path. The condition is always true (loop header ensures i slt n and i starts at 0), so no false positives.

## When NOT to Use

Do not apply when range_start != 0 or when range_type == "i1". Non-zero starts need different analysis; i1 ranges are trivial and don't need vectorization hints.
