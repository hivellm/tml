# 8. Related Work

## 8.1 LLM Code Generation and Debugging

Chen et al. [1] introduced Codex and HumanEval, establishing benchmarks for LLM code generation. Subsequent work has improved pass rates through chain-of-thought prompting [8], self-repair [9], and iterative refinement [14]. Our work differs by studying the debugging *process* rather than the final output quality.

Jimenez et al. [15] introduced SWE-bench, a benchmark of real-world GitHub issues requiring multi-file edits. While SWE-bench studies include debugging, the focus is on end-to-end resolution rather than tool usage patterns. Our MCP instrumentation provides finer-grained behavioral data.

## 8.2 LLM Tool Use

Schick et al. [6] demonstrated that LLMs can learn to use tools through few-shot prompting (Toolformer). Qin et al. [7] proposed ToolLLM with a benchmark for complex tool use. These works focus on *capability* (can the LLM use the tool correctly?) rather than *behavior* (how does the LLM choose among tools?). Our study addresses the latter question.

Patil et al. [20] studied tool selection in LLM agents, finding that models tend to over-rely on familiar tools. Our finding that `test` dominates despite cheaper alternatives aligns with this observation.

## 8.3 Automated Program Repair

Le Goues et al. [10] and Monperrus [11] surveyed automated program repair, establishing the generate-and-validate paradigm. Our observation that LLMs follow a similar pattern (edit-test-retry) suggests that LLM debugging shares structural similarities with classical APR, though with the addition of diagnostic tool use.

## 8.4 Human Debugging Behavior

Katz and Anderson [12] studied expert vs. novice debugging strategies, finding that experts use systematic hypothesis testing while novices use trial-and-error. Our finding that LLM behavior shifts from test-dominated to diagnosis-heavy over time parallels this novice-to-expert trajectory.

Ko and Myers [13] identified that debugging difficulty correlates with the distance between symptom and cause. The multi-layer debug output (`--debug-layers`) is designed to reduce this distance by showing all compilation layers simultaneously.
