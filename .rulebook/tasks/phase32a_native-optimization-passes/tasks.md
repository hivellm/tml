## 1. Implementation
- [ ] 1.1 LICM (licm.tml): implement loop detection via back-edge analysis on MIR basic blocks; identify instructions whose operands are all defined outside the loop body; hoist those instructions to the loop pre-header block inserted before the loop entry
- [ ] 1.2 GVN (gvn.tml): implement a hash-based value numbering pass over MIR instructions in RPO order; for each instruction compute a hash of (opcode, operand value-numbers); if hash already seen, replace the instruction with a copy from the earlier result register; handle load instructions conservatively (invalidated by stores)
- [ ] 1.3 Function inlining (inlining.tml): build a call graph from MIR; for each non-recursive callee with instruction count < 20, inline the body at each call site by cloning instructions with fresh register names and substituting parameter registers with argument registers; replace the call instruction with the inlined block
- [ ] 1.4 Dead argument elimination (dead_arg.tml): for each function, compute the set of parameter registers that are never read in the body; remove those parameters from the function signature and from every call site; update the pipeline's function registry
- [ ] 1.5 Tail call optimisation (tailcall.tml): detect return instructions whose value is the result of a call to the same function; replace with an assignment of argument registers and a jump to the function entry block label; this converts linear stack growth into constant stack usage
- [ ] 1.6 Loop unrolling (loop_unroll.tml): detect loops with a single induction variable whose trip count is a compile-time constant <= 8; duplicate the loop body N times with the induction variable substituted by its concrete value for each iteration; remove the loop back-edge
- [ ] 1.7 Benchmark: run five representative programs (fibonacci, matrix-multiply 32x32, string-reverse 10k chars, list-sort 1k elements, recursive-sum) compiled with and without opt passes via native backend and with LLVM -O2; record and assert native-with-opts is within 2x LLVM -O2 wall time

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
