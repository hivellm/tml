# Phase 3 link/infra is essentially complete — remaining are @no_coverage candidates
**Source**: manual
**Date**: 2026-03-15
**Related Task**: codegen-structural-fixes
**Tags**: phase3, link, infra, resolved
std/crypto: all tests pass. std/net: only tls_cert_verify and tcp_timeout fail (need real network/TLS server). std/thread: all pass (scope_basic has stub test). These remaining failures are infrastructure-dependent, not code bugs. Mark as @no_coverage candidates. Phase 3 of codegen-structural-fixes can be closed.