# Memory Leak Fix Tasks

**Status**: Pending (1027 leaks, 49.5 KB)

## Phase 1: Critical HTTP Module (34.5 KB - 70% of problem)

- [ ] 1.1 Investigate HttpResponse.text() implementation
- [ ] 1.2 Implement proper string cleanup in Response
- [ ] 1.3 Add resp.destroy() calls to client tests
- [ ] 1.4 Verify http/client.test.tml leaks reduced to <5

## Phase 2: Network Parsers (IPv6, IPv4, Socket - 556 B)

- [ ] 2.1 Fix IPv6 parser string allocations
- [ ] 2.2 Fix IPv4 parser string allocations
- [ ] 2.3 Fix socket parser string allocations
- [ ] 2.4 Verify parser test leaks reduced to <10 each

## Phase 3: Collections/HashMap (629 B)

- [ ] 3.1 Review HashMap.destroy() implementation
- [ ] 3.2 Ensure string keys properly freed
- [ ] 3.3 Add HashMap.destroy() calls to tests
- [ ] 3.4 Verify HashMap tests leak <10

## Phase 4: URL/JSON/Crypto Modules (2 KB)

- [ ] 4.1 Fix URL parsing string leaks
- [ ] 4.2 Fix JSON serialization leaks
- [ ] 4.3 Fix Crypto key cleanup
- [ ] 4.4 Verify each module <10 leaks

## Phase 5: Test Infrastructure & Misc (200 B)

- [ ] 5.1 Clean up event name strings in tests
- [ ] 5.2 Verify total leaks <100
- [ ] 5.3 Verify total memory <5 KB
- [ ] 5.4 Run full test suite to confirm

## Success Metrics
- Total leaks: 1027 → <100
- Total memory: 49.5 KB → <5 KB
- Largest single test: 34.5 KB → <1 KB
