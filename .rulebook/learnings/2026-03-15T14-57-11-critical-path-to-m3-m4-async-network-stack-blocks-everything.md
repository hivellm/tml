# Critical path to M3/M4: async-network-stack blocks everything
**Source**: manual
**Date**: 2026-03-15
**Related Task**: async-network-stack
**Tags**: roadmap, async, critical-path, planning
The async-network-stack task (currently 0%) is the single biggest strategic gate. Everything in Milestone 3 (HTTP, WebSocket) and Milestone 4 (Promise, Observable) depends on it. The async event loop foundation exists (TimerWheel, EventLoop, Poller), but missing pieces are: Future behavior with real poll() state machine codegen, Waker/Context types, block_on() executor. This is weeks of work. Recommendation: start Phase 1 investigation/design in parallel with current codegen bug fixes rather than waiting for M1/M2 to fully close.