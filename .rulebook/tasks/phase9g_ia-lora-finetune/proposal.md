# Proposal: IA LoRA/QLoRA Fine-Tuning

**Task**: phase9g_ia-lora-finetune
**Status**: Planning (0/8)
**Priority**: P2
**Estimated effort**: 5–8 days
**Risk**: High

## Problem

Training large language models from scratch requires tens of thousands of GPU-hours.
Fine-tuning a pre-trained model on a custom dataset is far cheaper, but even that
requires loading the full model into GPU memory and computing gradients for all
parameters. A 7B-parameter model in FP32 needs ~28 GB of VRAM for weights alone —
exceeding every consumer GPU. Without parameter-efficient fine-tuning (PEFT), TML's AI
training library is limited to toy models and cannot be used to adapt real LLMs to
domain-specific tasks.

## Proposed Solution

Implement LoRA (Low-Rank Adaptation) and QLoRA (Quantized LoRA) as composable layers
in `lib/ia/src/train/`.

**LoRA**: A `LoraLinear` layer wraps a frozen `Linear` layer with two trainable
low-rank matrices `lora_a` (d_in × r) and `lora_b` (r × d_out). During the forward
pass the output is `base(x) + (x @ lora_a @ lora_b) * (alpha / r)`. Only lora_a and
lora_b are updated by the optimizer; the base weights stay frozen. `apply_lora(model,
config)` traverses the module tree and replaces matching `Linear` layers with
`LoraLinear`. `merge_lora(model)` folds the LoRA deltas into the base weights for
zero-overhead inference after training.

**QLoRA**: `QloraLinear` stores the base weights in NF4 (4-bit NormalFloat) and
dequantizes on-the-fly before the matrix multiply. The LoRA adapters remain in FP16.
This reduces base weight VRAM from 2 bytes/param (FP16) to 0.5 bytes/param (NF4).

**Mixed precision**: A `GradScaler` accumulates gradients in FP32 while the forward
pass runs in FP16 (`autocast` context), preventing underflow in small gradients.

## Key Decisions

- **r=8 default, configurable to 4/16/32/64** — higher r gives more capacity but more
  trainable parameters; r=8 is the standard recommendation for instruction tuning.
- **alpha scaling** — `alpha/r` normalization keeps effective learning rate stable as r
  changes; alpha is typically set equal to r or 2r.
- **Target module selection by name pattern** — `LoraConfig.target_modules` is a list
  of name substrings (e.g., `["q_proj", "v_proj"]`); `apply_lora` only wraps modules
  whose name matches, allowing selective adaptation.
- **NF4 quantization for QLoRA** — NF4 is information-theoretically optimal for
  normally distributed weights (which pre-trained LLMs have); preferred over INT4 or
  FP4 for fine-tuning quality.
- **GradScaler uses dynamic scale** — starts at 2^16, halves on overflow, doubles
  every 2000 steps; no manual tuning needed.
- **Requires CUDA backend** — LoRA matrix multiplies and NF4 dequantization use CUDA
  kernels from phase9d; CPU fallback is too slow to be useful for real models.

## Files to Create/Modify

- `lib/ia/src/train/lora.tml` — LoraConfig, LoraLinear, apply_lora(), merge_lora()
- `lib/ia/src/train/qlora.tml` — NF4Tensor, QloraLinear, quantize_model()
- `lib/ia/src/train/mixed_precision.tml` — GradScaler, autocast context type

## Success Criteria

- All 8 checklist items marked done
- `apply_lora(model, LoraConfig { r: 8, alpha: 16, target_modules: ["q", "v"] })`
  replaces exactly the specified Linear layers and leaves all others unchanged
- Trainable parameter count after apply_lora is ≤ 1% of total parameters for a
  standard transformer
- `merge_lora(model)` produces numerically identical forward pass output to the
  unmerged version (max absolute error < 1e-5)
- QloraLinear forward pass output is within 1% relative error of FP16 baseline
- GradScaler correctly detects gradient overflow and skips the optimizer step
- A 7B-parameter model with QLoRA fits in 12 GB of VRAM (verified empirically)
- Unit tests for LoraLinear, QloraLinear, and GradScaler all pass

## Dependencies

- **Depends on**: CUDA backend (phase9d, must be complete), base Linear/Module types
  in lib/ia/src/nn/ (phase9a), optimizer (phase9e, AdamW)
- **Blocks**: any fine-tuning workflow, instruction tuning demos, domain adaptation
  examples in the TML documentation
