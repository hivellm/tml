# 7. Threats to Validity

## 7.1 Internal Validity

**Prompt accumulation**: The system prompt grew from approximately 2,000 to 8,000 words over the study period. Later sessions have more behavioral guidance, confounding comparisons between early and late periods. We mitigate this by tracking adoption rates relative to intervention dates rather than absolute time.

**Task confounding**: Different development tasks naturally produce different tool distributions. Codegen debugging sessions (e.g., SIMD work) produce more `emit-ir` calls, while library development sessions produce more `test` calls. The case study approach (Section 5.4) partially controls for this by analyzing within-task patterns.

**Session independence**: Sessions within the same conversation thread share context. A successful debugging pattern in one session may carry over to subsequent sessions, inflating adoption metrics. We mitigate this by computing metrics at the session level and reporting rolling averages.

## 7.2 External Validity

**Single developer**: All sessions involve one developer's workflow. Other developers may use different prompting strategies, task decompositions, or tool preferences.

**Single language**: TML's type system, compilation pipeline, and MCP tool set are specific to this project. Results may not generalize to languages with different error models (e.g., dynamically typed languages) or tool ecosystems.

**Single LLM family**: Claude models may have inherent preferences for certain tool patterns (e.g., preferring structured JSON) that do not transfer to other model families.

## 7.3 Construct Validity

**Error rate interpretation**: A high "error rate" for `check` (30.4%) reflects the tool being used as designed (finding type errors), not tool failure. Our metrics do not distinguish between expected errors (type errors found by check) and unexpected errors (tool crashes).

**Adoption rate as proxy for effectiveness**: We measure adoption (how often a tool is used) but not effectiveness (whether using the tool leads to faster bug resolution). Higher adoption does not necessarily mean better debugging outcomes.
