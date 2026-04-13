# Tasks: AI Library — LLM Inference Engine

**Status**: Planning. 0% (0/15).
**Depends on**: phase9b_ia-nn-layers, phase9e_ia-model-loading

## Phase 1: Transformer Architectures

- [ ] 1.1 `ia/transform/config.tml` — ModelConfig (vocab_size, hidden_size, num_layers, num_heads, etc.)
- [ ] 1.2 `ia/transform/llama.tml` — LLaMA architecture (RMSNorm, RoPE, GQA, SwiGLU)
- [ ] 1.3 `ia/transform/mistral.tml` — Mistral architecture (sliding window attention)
- [ ] 1.4 `ia/transform/phi.tml` — Phi architecture
- [ ] 1.5 `ia/transform/mod.tml` — Transform exports

## Phase 2: Inference Engine

- [ ] 2.1 `ia/infer/kv_cache.tml` — KV-cache for autoregressive generation
- [ ] 2.2 `ia/infer/sampling.tml` — Greedy, top-k, top-p, temperature, repetition penalty
- [ ] 2.3 `ia/infer/pipeline.tml` — TextGenerationPipeline (tokenize -> model -> decode)
- [ ] 2.4 `ia/infer/batch.tml` — Continuous batching for concurrent requests
- [ ] 2.5 `ia/infer/mod.tml` — Inference exports

## Phase 3: Tokenizer

- [ ] 3.1 `ia/infer/tokenizer.tml` — BPE tokenizer (load from tokenizer.json)
- [ ] 3.2 encode(text) -> List[I64], decode(tokens) -> Str

## Phase 4: Tests

- [ ] 4.1 Tests: KV-cache update + retrieval
- [ ] 4.2 Tests: sampling strategies produce valid tokens
- [ ] 4.3 Tests: generate text with small model (GPT-2 or Phi-mini)

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
