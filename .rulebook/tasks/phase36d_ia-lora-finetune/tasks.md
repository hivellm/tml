# Tasks: AI Library — LoRA/QLoRA Fine-Tuning

**Status**: Planning. 0% (0/8).
**Depends on**: phase9c_ia-training, phase9d_ia-cuda

## Phase 1: LoRA Implementation

- [ ] 1.1 `ia/train/lora.tml` — LoraConfig { r, alpha, dropout, target_modules }
- [ ] 1.2 LoraLinear (base frozen + lora_a/lora_b trainable)
- [ ] 1.3 apply_lora[M: Module](model, config) — inject LoRA layers
- [ ] 1.4 merge_lora() — merge weights into base for deployment

## Phase 2: QLoRA + Mixed Precision

- [ ] 2.1 `ia/train/qlora.tml` — QLoraLinear (4-bit base + FP16 LoRA)
- [ ] 2.2 `ia/train/mixed_precision.tml` — GradScaler, autocast context
- [ ] 2.3 Tests: LoRA fine-tune small model, verify loss decreases
- [ ] 2.4 Tests: QLoRA memory reduction vs full fine-tune

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
