# 9. Conclusion

## 9.1 Summary of Findings

This paper presented the first empirical study of LLM debugging tool usage in production compiler development. From 3,251 tool calls across 300 sessions, we identified five key findings:

1. **LLMs are overwhelmingly test-centric** (52.7% of calls), preferring definitive pass/fail feedback over diagnostic analysis. This mirrors the trial-and-error pattern observed in novice human programmers.

2. **Prompt-based interventions produce measurable, compounding behavioral change.** The check-before-test rule drove adoption from 8.8% to 25.3% over 10 days, with an accelerating (not plateauing) trajectory.

3. **Feature adoption correlates inversely with cognitive friction.** Default-on features (structured output) achieve 95.7% adoption. Opt-in features requiring behavioral change (debug_layers) reach only 11.1% despite explicit rules.

4. **A structural shift from trial-and-error to diagnostic reasoning is underway**, with test share declining from 60% to 44% as check and emit-ir usage increase. Advanced sessions show check exceeding test in frequency.

5. **Tool latency is the primary development bottleneck.** Test execution at 37 seconds per call dominates computation time. JIT execution (projected 2-second latency) would transform the development velocity.

## 9.2 Recommendations

For **MCP tool designers**: Make diagnostic output default-on for failure cases. Optimize for fine-grained invocation. Reduce latency as the highest-leverage improvement.

For **prompt engineers**: Include quantitative justification in behavioral rules. Minimize cognitive friction between rule recognition and execution. Expect compounding adoption over days, not immediate compliance.

For **researchers**: Instrument MCP servers for longitudinal studies. Compare tool usage across LLM families, programming domains, and developer experience levels.

## 9.3 Future Work

This study opens several directions for future research:

1. **Controlled experiments**: Curating a bug set stratified by compilation layer and measuring resolution time under different diagnostic conditions (baseline, debug-layers, auto-suggest).
2. **Cross-model comparison**: Replicating the study with GPT-4, Gemini, and open-source models to identify model-specific debugging preferences.
3. **Multi-project study**: Extending instrumentation to web development, embedded systems, and DevOps workflows to test generalizability.
4. **Adaptive prompting**: Using real-time tool usage data to dynamically adjust system prompts, reinforcing effective patterns and correcting ineffective ones.
5. **JIT impact measurement**: Quantifying how reduced test latency (via JIT execution) changes the tool distribution and debugging strategy.

[To be updated with additional data as the study continues.]
