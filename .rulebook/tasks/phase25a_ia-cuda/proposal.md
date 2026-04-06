# Proposal: IA CUDA Backend

## Why
GPU acceleration via CUDA/cuBLAS/cuDNN is essential for practical ML performance. FFI bindings to NVIDIA libraries enable 10-100x speedup.

## What Changes
- CUDA Runtime FFI (cudaMalloc, cudaFree, cudaMemcpy, streams, events)
- cuBLAS FFI (SGEMM, HGEMM, GemmEx, batched GEMM)
- cuDNN FFI (conv, activation, softmax, batchnorm)
- Safe wrappers: CudaBuffer (RAII), CudaStream, CudaMemoryPool
- Device abstraction: Device enum (Cpu, Cuda(id))
- Tensor.to_device(Cuda(0)) support

## Impact
- Affected code: lib/ia/src/cuda/ (new), lib/ia/src/device/ (new)
- Breaking change: NO
- User benefit: GPU-accelerated tensor operations and training
