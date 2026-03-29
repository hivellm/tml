# Tasks: AI Library — Model Loading (safetensors + GGUF)

**Status**: Planning. 0% (0/12).
**Depends on**: phase9_ia-tensor-core, phase9d_ia-cuda

## Phase 1: safetensors Format

- [ ] 1.1 `ia/model/safetensors.tml` — Parse safetensors header (JSON metadata + offsets)
- [ ] 1.2 Memory-map tensor data (zero-copy loading)
- [ ] 1.3 Load into VarMap (name -> Tensor)

## Phase 2: GGUF Format

- [ ] 2.1 `ia/model/gguf.tml` — Parse GGUF header (metadata key-value pairs)
- [ ] 2.2 `ia/quant/types.tml` — Quantized types (Q4_0, Q4_K_M, Q5_K_M, Q8_0)
- [ ] 2.3 `ia/quant/quantize.tml` — Dequantize blocks to F32/F16
- [ ] 2.4 Load quantized tensors into VarMap

## Phase 3: VarMap + Hub

- [ ] 3.1 `ia/model/varmap.tml` — VarMap { tensors: HashMap[Str, Tensor] }
- [ ] 3.2 load_into[M: Module](varmap, model) — load weights into model
- [ ] 3.3 `ia/model/hub.tml` — Download from HuggingFace Hub (HTTP GET)
- [ ] 3.4 `ia/model/mod.tml` — Model loading exports

## Phase 4: Tests

- [ ] 4.1 Tests: load real safetensors file
- [ ] 4.2 Tests: load GGUF file, verify dequantized values
