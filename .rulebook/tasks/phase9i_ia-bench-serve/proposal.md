# Proposal: AI Library — Benchmarks + HTTP Serving

**Task**: phase9i_ia-bench-serve
**Status**: Planning (0/12)
**Priority**: P2
**Estimated effort**: 3–4 days
**Risk**: Medium — depends on CUDA backend; benchmark reproducibility requires careful GPU warm-up

## Problem

The AI library has no performance validation infrastructure. There is no way to measure GEMM
throughput, inference token rate, or training throughput objectively. Additionally, trained models
cannot be deployed as HTTP endpoints, blocking any real-world usage of TML-based LLMs.

## Proposed Solution

Two parallel tracks:

**Track 1 — Benchmark infrastructure**: A `BenchmarkRunner` in `ia/bench/runner.tml` that uses
GPU event timing (CUDA events, not wall-clock) for accurate measurements. Separate benchmark
files for GEMM (128² to 4096²), inference tok/s, training throughput, and GPU memory profiling.
Cross-language comparison scripts targeting candle (Rust), PyTorch, and llama.cpp.

**Track 2 — HTTP serving**: A `mount_llm_endpoint(app, model, config)` function in
`ia/infer/serve.tml` that registers routes on a `std::http::App`. POST /generate streams
token-by-token output via SSE. The /v1/chat/completions endpoint follows the OpenAI API
schema for compatibility with existing client libraries.

## Key Decisions

- GPU event timing over wall-clock: CUDA events measure device-side execution, eliminating
  PCIe transfer latency from benchmark numbers.
- SSE streaming: token-by-token output avoids buffering the full response, enabling low
  time-to-first-token metrics.
- OpenAI API compatibility: clients written for GPT-4/Claude can target TML serving with
  zero changes, maximizing ecosystem reuse.
- Cross-language benchmarks in `.sandbox/`: reproducible scripts, not integrated into the
  test suite, to avoid CI dependency on external runtimes.

## Files to Create/Modify

- `lib/ia/src/bench/runner.tml` — BenchmarkRunner, GPU event timing, result formatting
- `lib/ia/src/bench/matmul_bench.tml` — GEMM benchmarks (128² to 4096² matrix sizes)
- `lib/ia/src/bench/inference_bench.tml` — Token generation throughput and latency
- `lib/ia/src/bench/training_bench.tml` — Forward + backward pass throughput
- `lib/ia/src/bench/memory_bench.tml` — GPU memory allocation and bandwidth profiling
- `lib/ia/src/infer/serve.tml` — mount_llm_endpoint, SSE streaming, OpenAI schema

## Success Criteria

- BenchmarkRunner runs GEMM at 128²/512²/1024²/4096² and reports TFLOP/s per size
- Inference benchmark reports tok/s and p50/p99 latency for a 7B-parameter model shape
- `mount_llm_endpoint(app, model, config)` compiles and registers routes on std::http::App
- POST /generate returns SSE stream with `data: {"token":"..."}` events
- GET /v1/chat/completions returns JSON matching OpenAI response schema
- Cross-language comparison script runs candle + PyTorch + llama.cpp and emits a table

## Dependencies

- Depends on: phase9f_ia-inference (model forward pass), phase9d_ia-cuda (GPU backend)
- Blocks: nothing (leaf task)
