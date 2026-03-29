# Proposal: IA LoRA/QLoRA Fine-Tuning

## Why
Parameter-efficient fine-tuning enables adapting large models with minimal GPU memory. LoRA adds trainable low-rank matrices; QLoRA uses 4-bit quantized base.

## What Changes
- LoraConfig, LoraLinear (frozen base + trainable lora_a/lora_b)
- apply_lora/merge_lora for injection and deployment
- QLoraLinear (4-bit base + FP16 LoRA adapters)
- GradScaler and autocast for mixed precision training

## Impact
- Affected code: lib/ia/src/train/lora.tml, qlora.tml, mixed_precision.tml
- Breaking change: NO
- User benefit: Fine-tune 7B models on consumer GPUs (RTX 3090)
