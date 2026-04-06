# Proposal: IA Model Loading (safetensors + GGUF)

## Why
Load pre-trained models from HuggingFace (safetensors) and quantized local models (GGUF/llama.cpp format). Essential for inference.

## What Changes
- safetensors parser (header + mmap tensor data)
- GGUF parser (metadata + quantized tensor blocks)
- Quantization types (Q4_0, Q4_K_M, Q5_K_M, Q8_0)
- Dequantize operations
- VarMap (named tensor storage), load_into[M: Module]
- HuggingFace Hub download (HTTP GET)

## Impact
- Affected code: lib/ia/src/model/ (new), lib/ia/src/quant/ (new)
- Breaking change: NO
- User benefit: Load LLaMA, Mistral, Phi models directly
