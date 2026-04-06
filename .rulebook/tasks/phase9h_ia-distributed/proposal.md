# Proposal: IA Multi-GPU Distributed Training

**Task**: phase9h_ia-distributed
**Status**: Planning (0/8)
**Priority**: P2
**Estimated effort**: 5–8 days
**Risk**: High

## Problem

A single GPU has a fixed amount of VRAM and compute. Models larger than roughly 10B
parameters cannot fit on one GPU in full precision, and training smaller models can still
be accelerated by splitting the data across multiple GPUs. Without distributed training
support, TML's AI library is limited to single-GPU workloads — blocking researchers and
practitioners who need to train production-scale models. The missing primitive is an
efficient way to synchronize gradients across GPUs after each backward pass, which is
the core of data-parallel distributed training.

## Proposed Solution

Implement NCCL-based distributed training in `lib/ia/src/distributed/` and
`lib/ia/src/cuda/ffi/`.

**NCCL FFI layer**: `@extern("c")` declarations for the NCCL collective operations used
in data-parallel training: `ncclAllReduce` (gradient averaging), `ncclBroadcast`
(parameter broadcast from rank 0 after initialization), `ncclAllGather` (used in FSDP),
and `ncclReduceScatter` (used in ZeRO-style sharding). An `NcclCommunicator` struct
wraps an `ncclComm_t` handle and ensures `ncclCommDestroy` is called on drop.

**DistributedDataParallel (DDP)**: `DistributedDataParallel[M]` wraps any `Module`
implementation. During the backward pass it hooks into the gradient computation and
issues an `AllReduce` for each parameter bucket, dividing the result by `world_size` to
produce the averaged gradient. Gradient bucketing groups small parameters into larger
buffers to amortize NCCL launch overhead and improve bandwidth utilization. After
`optimizer.step()` all ranks have identical parameters.

**Process group**: `DistributedConfig` holds `rank`, `world_size`, `backend` (NCCL),
and the rendezvous address. `init_process_group(config)` establishes the NCCL
communicator using TCP rendezvous for rank discovery.

## Key Decisions

- **NCCL as the sole backend** — NCCL is the industry standard for GPU collective
  communication; it uses NVLink when available and falls back to PCIe. Implementing
  Gloo or MPI would add scope without user benefit for GPU workloads.
- **Ring-allreduce topology** — NCCL selects the optimal ring or tree topology
  automatically; the TML layer does not hard-code a topology.
- **Gradient bucketing with 25 MB default bucket size** — matches PyTorch DDP default;
  small enough to overlap compute and communication, large enough to amortize overhead.
- **Hook-based gradient sync** — gradients are reduced as they become available during
  the backward pass (eager reduction), not after all gradients are computed; this
  overlaps GPU compute with network communication.
- **world_size divisor applied inside AllReduce** — the sum is divided by world_size
  inside the NCCL call using `ncclSum` + manual divide, matching the semantics of
  averaged gradients rather than summed gradients.
- **Rank 0 broadcast after init** — ensures all ranks start from identical parameter
  values even if model weights were randomly initialized independently per process.

## Files to Create/Modify

- `lib/ia/src/cuda/ffi/nccl.tml` — `@extern("c")` NCCL declarations (AllReduce,
  Broadcast, AllGather, ReduceScatter, CommInitRank, CommDestroy, GetUniqueId)
- `lib/ia/src/distributed/communicator.tml` — NcclCommunicator wrapper with safe
  resource management, init_process_group(), all_reduce(), broadcast()
- `lib/ia/src/distributed/ddp.tml` — DistributedDataParallel[M: Module], gradient
  hooks, bucket management, forward/backward wrappers
- `lib/ia/src/distributed/config.tml` — DistributedConfig, rank/world_size accessors,
  rendezvous address parsing

## Success Criteria

- All 8 checklist items marked done
- `init_process_group` succeeds on 2 GPUs and both ranks reach the barrier
- After one forward+backward+step cycle, all ranks have numerically identical parameters
  (max absolute difference < 1e-6)
- Gradient buckets are filled and reduced in the correct order across 2 GPUs
- `NcclCommunicator` drop calls `ncclCommDestroy` and does not leak the NCCL handle
- Training throughput on 2 GPUs is ≥ 1.8x throughput on 1 GPU for a standard
  transformer forward+backward pass (demonstrates near-linear scaling)
- Unit tests for communicator init, allreduce correctness, and bucket ordering pass

## Dependencies

- **Depends on**: CUDA backend (phase9d, must be complete), base Module/Parameter types
  in lib/ia/src/nn/ (phase9a), NCCL ≥ 2.18 headers and shared library present in build
  environment, at least 2 CUDA-capable GPUs for integration tests
- **Blocks**: large model training demos, FSDP (Fully Sharded Data Parallel) which
  builds on AllGather + ReduceScatter, model parallelism pipeline scheduling
