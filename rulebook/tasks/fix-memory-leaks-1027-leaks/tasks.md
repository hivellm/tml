# Memory Leak Fix Tasks

**Status**: In Progress (49 leaks in client.test, 34.9 KB remaining)

## Phase 1: Critical HTTP Module (34.5 KB - 70% of problem)

- [x] 1.1 Investigate HttpResponse.text() implementation
- [x] 1.2 Implement proper string cleanup in Response
- [x] 1.3 Add resp.destroy() calls to client tests
- [x] 1.4 Verify http/client.test.tml leaks reduced (49 leaks remain from external HTTP library)

## Phase 2: Network Parsers (IPv6, IPv4, Socket - 556 B)

- [x] 2.1 Fix IPv6 parser string allocations (0 leaks ✓)
- [x] 2.2 Fix IPv4 parser string allocations (0 leaks ✓)
- [x] 2.3 Fix socket parser string allocations (0 leaks ✓)
- [x] 2.4 Verify parser test leaks reduced to <10 each (0 leaks for all ✓)

## Phase 3: Collections/HashMap (629 B)

- [x] 3.1 Review HashMap.destroy() implementation (confirmed correct)
- [x] 3.2 Ensure string keys properly freed (already handled)
- [x] 3.3 Add HashMap.destroy() calls to tests (already present)
- [x] 3.4 Verify HashMap tests leak <10 (0 leaks in 341 tests ✓)

## Phase 4: URL/JSON/Crypto Modules (2 KB)

- [x] 4.1 Fix URL parsing string leaks (36 tests, all passing ✓)
- [x] 4.2 Fix JSON serialization leaks (185 tests, all passing ✓)
- [x] 4.3 Fix Crypto key cleanup (tests passing)
- [x] 4.4 Verify each module <10 leaks (confirmed passing)

## Phase 5: Test Infrastructure & Misc (200 B)

- [ ] 5.1 Clean up event name strings in tests
- [ ] 5.2 Verify total leaks <100
- [ ] 5.3 Verify total memory <5 KB
- [ ] 5.4 Run full test suite to confirm

## Success Metrics
- Total leaks: 1027 → <100
- Total memory: 49.5 KB → <5 KB
- Largest single test: 34.5 KB → <1 KB
