# 6. Discussion

## 6.1 Implications for Tool Design

Our findings suggest several principles for designing MCP tools for LLM consumers:

**Default-on diagnostics outperform opt-in.** The contrast between `structured=true` adoption (95.7%, no rule needed) and `debug_layers` adoption (11.1%, explicit rule required) demonstrates that LLMs adopt features that are on by default but rarely opt into features that require explicit activation. Tool designers should make diagnostic output the default on failure, not an optional parameter.

**Latency drives tool selection.** The LLM's strong preference for `test` (52.7%) over `check` (17.4%) persists despite `check` being 10x faster and explicitly promoted. We attribute this to the LLM's preference for *definitive* feedback (test passes or fails) over *partial* feedback (type-checks but may still crash at runtime). Reducing test latency via JIT execution would address this preference directly.

**Fine-grained tools are preferred.** The 74.9% single-file test rate suggests LLMs strongly prefer targeted over comprehensive feedback. Tool designers should optimize for fine-grained invocation (single function, single file) rather than batch operations.

**Documentation tools are underutilized but highly reliable.** With error rates of 0.0-0.7%, documentation tools are the most reliable in the toolkit, yet they represent only 13.4% of calls. Better integration (e.g., auto-suggesting relevant docs when a type error occurs) could increase adoption.

## 6.2 Implications for Prompt Engineering

Our longitudinal data provides evidence for several prompt engineering principles:

**Quantitative justification accelerates adoption.** The check-before-test rule included "check is 10x faster than test," providing a concrete reason. This rule produced compounding adoption (8.8% -> 25.3% over 10 days). Rules without quantitative justification (e.g., debug_layers) produced slower, linear growth.

**Rules produce compounding, not transient, behavioral change.** The check adoption curve is *accelerating* (period-over-period growth of +36%, +46%, +45%), not plateauing. This suggests the LLM internalizes the rationale and applies it more consistently with experience, rather than merely complying when the rule is salient.

**Cognitive friction determines adoption ceiling.** Features requiring simple sequencing changes (check before test) achieve higher adoption than features requiring mode switches (debug_layers on failure). Prompt engineers should minimize the cognitive steps between rule recognition and rule execution.

**New tool mentions produce rapid but shallow adoption.** Five new tools added to the system prompt (`build`, `debug`, `profile`, `inspect`, `docs_resolve`) appeared in logs within 1-2 sessions, but at very low rates (1-16 calls total). Initial tool awareness is high; sustained usage requires worked examples and clear triggers.

## 6.3 The Shift from Trial-and-Error to Diagnostic Reasoning

The most significant finding is the structural shift in debugging strategy over the observation period. The LLM's behavior evolved from a test-dominated "run and see" pattern (60% test, 8.8% check) toward a diagnostic "analyze then verify" pattern (44% test, 25.3% check, 9.2% emit-ir in recent sessions).

This shift mirrors the novice-to-expert trajectory observed in human debugging studies [12, 13]: novice programmers default to trial-and-error, while experts use systematic diagnosis. The LLM's trajectory suggests that prompt engineering and domain experience can drive this transition, though it occurs over days rather than the months or years typical of human skill development.

The SIMD case study (Section 5.4) demonstrates the endpoint of this trajectory: sessions where `check` exceeds `test` in frequency, and the LLM follows structured multi-tool diagnostic workflows. Whether this behavior persists across different task domains remains an open question.

## 6.4 Limitations

This study has several important limitations:

1. **Single project**: All data comes from one compiler project (TML). Tool usage patterns may differ for web development, embedded systems, or other domains.
2. **Single LLM**: All data comes from Claude Opus 4.6 [17]. Other models (GPT-4 [18], Gemini [19], open-source models) may exhibit different debugging behaviors.
3. **Organic data**: Without controlled experiments, we cannot establish causal relationships between interventions and behavioral changes. The observed trends may be confounded by task variation, learning effects, or prompt accumulation.
4. **Observer effect**: The LLM is aware that tools exist and that the system prompt promotes certain usage patterns. This awareness may influence behavior independently of the tools' intrinsic utility.
5. **Token estimation**: We do not have exact token counts for tool responses, limiting our ability to analyze information density per call.
