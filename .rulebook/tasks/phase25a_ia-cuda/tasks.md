# Tasks: AI Library — CUDA Backend

**Status**: Planning. 0% (0/20).
**Depends on**: phase9_ia-tensor-core
**Reference**: docs/analyses/ia/03-cuda-integration-design.md

## Phase 1: CUDA Runtime FFI

- [ ] 1.1 `ia/cuda/ffi/runtime.tml` — cudaMalloc, cudaFree, cudaMemcpy, cudaMemcpyAsync
- [ ] 1.2 cudaGetDeviceCount, cudaSetDevice, cudaGetDeviceProperties
- [ ] 1.3 cudaStreamCreate, cudaStreamDestroy, cudaStreamSynchronize
- [ ] 1.4 cudaEventCreate, cudaEventRecord, cudaEventElapsedTime
- [ ] 1.5 cudaGetLastError, cudaGetErrorString

## Phase 2: Safe Wrappers

- [ ] 2.1 `ia/cuda/buffer.tml` — CudaBuffer (RAII: auto cudaFree on drop)
- [ ] 2.2 `ia/cuda/stream.tml` — CudaStream wrapper
- [ ] 2.3 `ia/cuda/device_info.tml` — GPU detection, compute capability query
- [ ] 2.4 `ia/cuda/pool.tml` — GPU memory pool (caching allocator)

## Phase 3: cuBLAS FFI

- [ ] 3.1 `ia/cuda/ffi/cublas.tml` — cublasCreate, cublasDestroy, cublasSetStream
- [ ] 3.2 cublasSgemm, cublasDgemm, cublasHgemm (FP32/FP64/FP16 matmul)
- [ ] 3.3 cublasGemmEx (mixed precision), cublasSgemmStridedBatched (attention)

## Phase 4: cuDNN FFI

- [ ] 4.1 `ia/cuda/ffi/cudnn.tml` — cudnnCreate, tensor descriptors
- [ ] 4.2 cudnnConvolutionForward, cudnnActivationForward
- [ ] 4.3 cudnnSoftmaxForward, cudnnBatchNormalizationForwardInference

## Phase 5: Device Abstraction

- [ ] 5.1 `ia/device/device.tml` — Device enum (Cpu, Cuda(id))
- [ ] 5.2 `ia/device/cpu.tml` — CPU backend dispatch
- [ ] 5.3 `ia/device/cuda.tml` — CUDA backend dispatch
- [ ] 5.4 Tensor.to_device(Cuda(0)) — host-to-device transfer

## Phase 6: Tests

- [ ] 6.1 Tests: GPU memory alloc/free, memcpy host<->device
- [ ] 6.2 Tests: cuBLAS GEMM correctness (compare with CPU)
- [ ] 6.3 Tests: device transfer round-trip
