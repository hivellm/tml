# Tasks: AI Library — Benchmarks + HTTP Serving

**Status**: Planning. 0% (0/12).
**Depends on**: phase9f_ia-inference, phase9d_ia-cuda

## Phase 1: Benchmark Infrastructure

- [ ] 1.1 `ia/bench/runner.tml` — BenchmarkRunner with GPU event timing
- [ ] 1.2 `ia/bench/matmul_bench.tml` — GEMM benchmarks (128² to 4096²)
- [ ] 1.3 `ia/bench/inference_bench.tml` — Token generation (tok/s, latency)
- [ ] 1.4 `ia/bench/training_bench.tml` — Training throughput
- [ ] 1.5 `ia/bench/memory_bench.tml` — GPU memory profiling

## Phase 2: HTTP Serving

- [ ] 2.1 `ia/infer/serve.tml` — mount_llm_endpoint(app, model, config)
- [ ] 2.2 POST /generate with streaming SSE response
- [ ] 2.3 OpenAI-compatible /v1/chat/completions endpoint

## Phase 3: Cross-Language Benchmarks

- [ ] 3.1 Equivalent candle (Rust) benchmarks
- [ ] 3.2 Equivalent PyTorch benchmarks
- [ ] 3.3 Equivalent llama.cpp benchmarks
- [ ] 3.4 Comparison report

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
