# stdlib C dependency tiers for migration planning
**Source**: manual
**Date**: 2026-03-15
**Tags**: stdlib, migration, tiers, planning
Tier 1 (Pure TML, no migration needed): fmt, iter, cmp, ops, convert, result, option, error, encoding, collections (List/HashMap/etc), stream, text, events, cli, url, semver, regex, random, types. Tier 2 (TML + memory intrinsics only): str body, alloc/heap, alloc/shared, hash, num, ptr, slice, simd, unicode, cell, array, pin, mem. Tier 3 (Few @extern to libc): str (strlen/memcmp/memchr - 3 calls), core::sync (atomic_* - 9 calls), math (libm). Tier 4 (OS wrappers): thread, sync/mutex/rwlock, net/sys, file, os, time. Tier 5 (External libraries): crypto (OpenSSL/CNG), zlib/brotli/zstd (compression libs), sqlite, json parser, search (BM25/HNSW).