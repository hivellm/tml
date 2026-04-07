# Tasks: AI Library — Multi-GPU Distributed Training

**Status**: Planning. 0% (0/8).
**Depends on**: phase9d_ia-cuda

## Phase 1: NCCL FFI

- [ ] 1.1 `ia/cuda/ffi/nccl.tml` — ncclGetUniqueId, ncclCommInitRank, ncclCommDestroy
- [ ] 1.2 ncclAllReduce, ncclBroadcast, ncclAllGather, ncclReduceScatter

## Phase 2: Distributed Training

- [ ] 2.1 DistributedConfig (world_size, rank, backend)
- [ ] 2.2 NcclCommunicator wrapper — safe allreduce, broadcast
- [ ] 2.3 DistributedDataParallel[M: Module] — auto gradient sync
- [ ] 2.4 Gradient bucketing for efficient NCCL communication

## Phase 3: Tests

- [ ] 3.1 Tests: NCCL allreduce correctness (requires 2+ GPUs)
- [ ] 3.2 Tests: DDP training same result as single-GPU

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
