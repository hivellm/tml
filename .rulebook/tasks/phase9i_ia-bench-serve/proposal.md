# Proposal: IA Benchmarks + HTTP Serving

## Why
Benchmark infrastructure for performance validation and HTTP serving for deploying LLMs as API endpoints. Integration with std::http.

## What Changes
- BenchmarkRunner with GPU event timing
- Benchmarks: GEMM, inference tok/s, training throughput, memory
- HTTP serving: mount_llm_endpoint, SSE streaming, OpenAI-compatible API
- Cross-language benchmarks (vs candle, PyTorch, llama.cpp)

## Impact
- Affected code: lib/ia/src/bench/ (new), lib/ia/src/infer/serve.tml
- Breaking change: NO
- User benefit: Deploy LLMs as HTTP APIs, performance data
