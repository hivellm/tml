# Proposal: IA LLM Inference Engine

## Why
Complete inference pipeline: load model, tokenize, generate text with KV-cache and sampling strategies. The primary use case for the IA library.

## What Changes
- Transformer architectures: LLaMA, Mistral, Phi (with RoPE, GQA, SwiGLU)
- KV-cache for efficient autoregressive generation
- Sampling: greedy, top-k, top-p, temperature, repetition penalty
- TextGenerationPipeline (tokenize -> model -> decode)
- BPE tokenizer (load from tokenizer.json)
- Continuous batching for concurrent requests

## Impact
- Affected code: lib/ia/src/transform/ (new), lib/ia/src/infer/ (new)
- Breaking change: NO
- User benefit: Run LLMs locally in TML
