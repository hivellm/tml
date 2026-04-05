# 1. Introduction

The application of Large Language Models to software engineering has expanded rapidly from code completion [1] to autonomous multi-step development workflows [2, 3]. While code generation quality has received substantial attention, the *debugging behavior* of LLMs -- how they diagnose and fix errors in complex systems -- remains poorly understood. This gap is especially acute for systems programming tasks like compiler development, where bugs span multiple abstraction layers (lexer, parser, type checker, intermediate representations, machine code generation) and require structured diagnostic reasoning.

Compiler development presents a uniquely challenging debugging domain. A single bug may manifest as a runtime crash, but its root cause could lie in type inference, intermediate representation (IR) construction, optimization passes, or code generation. Effective debugging requires navigating these layers systematically -- a task that tests whether LLMs can execute structured multi-step diagnostic workflows rather than relying on trial-and-error iteration.

The Model Context Protocol (MCP) [4] provides a standardized interface between LLMs and development tools, enabling fine-grained instrumentation of tool usage. By logging every MCP tool invocation during organic compiler development, we can observe LLM debugging behavior in situ -- without the artificial constraints of benchmarks or the confounds of contrived tasks.

This paper makes the following contributions:

1. **The first empirical dataset** of LLM debugging tool usage in production compiler development: 3,251 calls across 300 sessions, 17 distinct tools, spanning 30 days of organic development on the TML compiler.

2. **Quantitative analysis of tool usage patterns**, revealing that LLMs are overwhelmingly test-centric (52.7%), underutilize IR diagnostics (7.5%), and strongly prefer fine-grained over comprehensive testing (74.9% single-file tests).

3. **Evidence that prompt-based interventions measurably change LLM debugging behavior**, with type-checking adoption accelerating from 8.8% to 25.3% over 10 days following explicit rules with quantitative justification.

4. **Design recommendations for MCP tool ecosystems**, including default-on diagnostics, auto-suggestion of pre-validation steps, and latency reduction as the primary lever for tool adoption.
