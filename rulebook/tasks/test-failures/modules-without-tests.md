# Modules with 0% Coverage - Complete Analysis

**Date**: 2026-02-12
**Current Coverage**: 49.42% (2,009/4,065 functions)
**Tests**: 6,417 tests in 257 files

## Executive Summary

Of **62 modules with 0% coverage** (596 functions):

- ✅ **6 modules** (29 functions) - Behavior definitions only, **don't need tests**
- ⏸️ **5 modules** (48 functions) - **Not implemented**, no code exists
- ❌ **51 modules** (519 functions) - **IMPLEMENTED but no tests** ← FOCUS HERE

---

## 1. Behavior Definitions Only (6 modules, 29 functions)

**Don't need tests** - these are just interface definitions:

1. `num/overflow` - 19 functions (CheckedAdd, CheckedSub, SaturatingAdd, WrappingAdd, etc.)
2. `iter/traits/double_ended` - 5 functions
3. `iter/traits/exact_size` - 2 functions
4. `iter/traits/extend` - 1 function
5. `iter/traits/from_iterator` - 1 function
6. `iter/traits/into_iterator` - 1 function

**Action**: None. Behaviors are tested through their implementations.

---

## 2. Not Implemented Modules (5 modules, 48 functions)

**No code implemented**, so nothing to test:

1. `crypto/constants` - 14 functions
2. `coverage` - 12 functions
3. `future` - 12 functions
4. `runner` - 9 functions
5. `precompiled_symbols` - 1 function

**Action**: Wait for implementation or remove from library.

---

## 3. 🎯 IMPLEMENTED BUT NO TESTS (51 modules, 519 functions)

**HIGH PRIORITY** - These modules have implemented code but 0% coverage.

### 3.1 Crypto (7 modules, 172 functions) ⚠️ HIGH IMPACT

1. **`crypto/key`** - 59 functions
   - KeyType, KeyPair, PublicKey, PrivateKey
   - Operations: generate, import/export, sign/verify

2. **`crypto/ecdh`** - 26 functions
   - Elliptic Curve Diffie-Hellman
   - Key exchange, shared secret

3. **`crypto/dh`** - 24 functions
   - Classic Diffie-Hellman
   - Key exchange

4. **`crypto/kdf`** - 23 functions
   - Key Derivation Functions
   - PBKDF2, HKDF

5. **`crypto/rsa`** - 18 functions
   - RSA encryption/decryption
   - Signing/verification

6. **`crypto/x509_test_minimal`** - 8 functions
   - Minimal X.509 certificate tests

7. **`crypto/constants`** - 14 functions (not implemented)

**Impact**: 172 crypto functions without tests = SECURITY RISK

---

### 3.2 Network/UDP (5 modules, 128 functions) ⚠️ HIGH IMPACT

1. **`net/pending/udp`** - 38 functions
   - Async UDP socket

2. **`net/udp`** - 32 functions
   - Sync UDP socket

3. **`net/pending/async_tcp`** - 25 functions
   - Async TCP

4. **`net/pending/parser`** - 19 functions
   - HTTP/protocol parsing

5. **`net/pending/async_udp`** - 14 functions
   - UDP async variants

**Impact**: 128 network functions without tests = PRODUCTION BUG RISK

---

### 3.3 Iterators (29 modules, 124 functions) 📊 MEDIUM IMPACT

**Adapters** (19 modules, 48 functions):
- `iter/adapters/peekable` - 7 functions
- `iter/adapters/rev` - 4 functions
- `iter/adapters/chain` - 3 functions
- `iter/adapters/cloned` - 3 functions
- `iter/adapters/copied` - 3 functions
- `iter/adapters/cycle` - 3 functions
- `iter/adapters/enumerate` - 3 functions
- `iter/adapters/fuse` - 3 functions
- `iter/adapters/inspect` - 3 functions
- `iter/adapters/zip` - 3 functions
- `iter/adapters/filter_map` - 2 functions
- `iter/adapters/flat_map` - 2 functions
- `iter/adapters/flatten` - 2 functions
- `iter/adapters/intersperse` - 2 functions
- `iter/adapters/map_while` - 2 functions
- `iter/adapters/scan` - 2 functions
- `iter/adapters/skip_while` - 2 functions
- `iter/adapters/step_by` - 2 functions
- `iter/adapters/take_while` - 2 functions

**Sources** (4 modules, 10 functions):
- `iter/sources/empty` - 3 functions
- `iter/sources/once_with` - 3 functions
- `iter/sources/repeat_with` - 3 functions
- `iter/sources/from_fn` - 2 functions
- `iter/sources/successors` - 2 functions

**Traits** (2 modules, 41 functions):
- `iter/traits/accumulators` - 22 functions
- `iter/traits/iterator` - 19 functions

**Array/Slice** (2 modules, 38 functions):
- `array/iter` - 19 functions
- `slice/iter` - 19 functions

**Impact**: 124 iterator functions = CORE FUNCTIONALITY WITHOUT TESTS

---

### 3.4 JSON/Serialize (1 module, 23 functions) ⚠️ HIGH IMPACT

- **`json/serialize`** - 23 functions
  - Automatic struct/enum serialization to JSON
  - JSON deserialization to TML types

**Impact**: Serialization without tests = POSSIBLE DATA CORRUPTION

---

### 3.5 Slice/Array Operations (3 modules, 30 functions) 📊 MEDIUM IMPACT

1. **`slice/sort`** - 12 functions
   - sort(), sort_by(), sort_by_key()
   - Quicksort implementation

2. **`slice/cmp`** - 9 functions
   - Slice comparison operations

3. **`array/ascii`** - 9 functions
   - ASCII operations on arrays

**Impact**: 30 basic array/slice functions without tests

---

### 3.6 Other Modules (6 modules, 42 functions)

1. **`thread/scope`** - 9 functions
   - Scoped threads

2. **`fmt/num`** - 7 functions
   - Number formatting

3. **`convert`** - 6 functions
   - Type conversions

4. **`cell/lazy`** - 5 functions
   - Lazy cell initialization

5. **`ops/function`** - 3 functions
   - Function call operators

6. **`ops/deref`** - 2 functions
   - Deref trait

7. **`ops/index`** - 2 functions
   - Index trait

8. **`object`** - 1 function
   - Base object functionality

---

## Recommended Action Plan

### Phase 1: Quick Wins (30 functions, ~3 hours)
1. ✅ `slice/sort` - 12 functions (basic sorting tests) [BLOCKED: methods not implemented]
2. `slice/cmp` - 9 functions
3. `array/ascii` - 9 functions

### Phase 2: Core Functionality (124 functions, ~2 days)
1. `iter/adapters/*` - 48 functions (most used adapters)
2. `iter/sources/*` - 10 functions
3. `array/iter` + `slice/iter` - 38 functions
4. `iter/traits/accumulators` - 22 functions

### Phase 3: High Risk Modules (195 functions, ~1 week)
1. `json/serialize` - 23 functions (CRITICAL)
2. `crypto/*` - 172 functions (SECURITY)

### Phase 4: Network (128 functions, ~1 week)
1. `net/udp` + `net/pending/udp` - 70 functions
2. `net/pending/async_tcp` - 25 functions
3. `net/pending/parser` - 19 functions
4. `net/pending/async_udp` - 14 functions

### Phase 5: Remaining (42 functions, ~1 day)
1. `thread/scope` - 9 functions
2. `fmt/num` - 7 functions
3. `convert` - 6 functions
4. `cell/lazy` - 5 functions
5. `ops/*` - 7 functions
6. `object` - 1 function

---

## Estimated Impact

**If all 51 modules are tested:**
- Current coverage: 49.42% (2,009/4,065)
- Additional functions: +519
- **Projected coverage: 62.2% (2,528/4,065)** 🎯

**Gain**: +12.78% absolute coverage

---

## Important Notes

1. **Many modules have compiler bugs** (see tasks.md)
   - Behaviors return `()` instead of correct type
   - Generic methods don't work
   - Some may not be testable until compiler fixes

2. **Prioritize by impact:**
   - 🔴 CRITICAL: crypto, json/serialize, net
   - 🟡 MEDIUM: iterators, slice/sort
   - 🟢 LOW: fmt/num, ops/*

3. **Some "not implemented" modules may be:**
   - Pending future implementation
   - Removed from spec but still in coverage
   - Dead code that should be removed

4. **Test consolidation completed:**
   - Reduced from 509 to 257 test files (-49.5%)
   - Increased tests from 4,799 to 6,417 (+33.7%)
   - Coverage: 49.42% (slight drop from 50% due to library growth)
