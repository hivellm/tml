# Proposal: IA Multi-GPU Distributed Training

## Why
Multi-GPU training via NCCL enables scaling to larger models and datasets. DistributedDataParallel automatically syncs gradients across GPUs.

## What Changes
- NCCL FFI (AllReduce, Broadcast, AllGather, ReduceScatter)
- NcclCommunicator safe wrapper
- DistributedDataParallel[M: Module] with automatic gradient sync
- Gradient bucketing for efficient communication

## Impact
- Affected code: lib/ia/src/cuda/ffi/nccl.tml, lib/ia/src/train/distributed.tml
- Breaking change: NO
- User benefit: Scale training across multiple GPUs
