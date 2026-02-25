# Memory Leak Analysis & Fix Strategy

**Total Leaks**: 1027 leaks, 49.5 KB
**Status**: Identified, awaiting implementation

## Leak Distribution by Category

### 1. HTTP Module (142 leaks, 35.2 KB) - **CRITICAL**
- **client.test.tml**: 52 leaks, 34.5 KB (97% from HTTP)
  - Root cause: `resp.text()` creates Str that's never destroyed
  - Affected: HttpResponse.text() method returns unmanaged string
  - Impact: Each HTTP test creates response with body, body string leaks

- **cookie.test.tml**: 11 leaks, 153 B
- **router_wildcard.test.tml**: 18 leaks, 143 B
- **router_params.test.tml**: 32 leaks, 139 B
- **router_mixed.test.tml**: 17 leaks, 106 B
- **headers.test.tml**: 6 leaks, 75 B
- **router_basic.test.tml**: 12 leaks, 58 B

**Fix Strategy**:
1. HttpResponse needs explicit cleanup for body string
2. Add `destroy()` method call in tests after using response
3. Consider RAII pattern or automatic cleanup hook

---

### 2. Network Parsers (169 leaks, 556 B)
- **net/parser_ipv6.test.tml**: 60 leaks, 235 B
  - Root cause: IPv6 address string parsing creates intermediate strings

- **net/parser_socket.test.tml**: 61 leaks, 186 B
  - Root cause: Socket address parsing (IP:port format)

- **net/parser_ipv4.test.tml**: 48 leaks, 125 B
  - Root cause: IPv4 address string parsing

**Fix Strategy**:
1. Review `std::net::parser` implementations
2. Ensure temporary strings in parsing are freed
3. Use stack allocation where possible instead of heap

---

### 3. Collections/HashMap (166 leaks, 629 B)
- **hashmap_dynamic_keys.test.tml**: 50 leaks, 312 B
- **hashmap_str_str.test.tml**: 53 leaks, 202 B
- **hashmap_scale.test.tml**: 30 leaks, 183 B
- **hashmap_i64_str.test.tml**: 22 leaks, 84 B
- **hashmap_str_bool.test.tml**: 6 leaks, 31 B
- **hashmap_mixed_int.test.tml**: 5 leaks, 14 B
- **collections.test.tml**: 10 leaks, 38 B

**Fix Strategy**:
1. HashMap.destroy() not properly cleaning up string keys/values
2. Review HashMap implementation's destructor
3. Ensure all HashMap instances are explicitly destroyed in tests

---

### 4. URL/Query Parsing (72 leaks, 627 B)
- **url_getters.test.tml**: 13 leaks, 159 B
- **url_parse.test.tml**: 11 leaks, 148 B
- **url_query.test.tml**: 18 leaks, 137 B
- **url_methods.test.tml**: 10 leaks, 123 B
- **url_format.test.tml**: 11 leaks, 157 B
- **url_join.test.tml**: 9 leaks, 102 B

**Fix Strategy**:
1. URL parsing creates intermediate strings
2. Query parameter parsing allocates component strings
3. Ensure URL.destroy() is called in tests

---

### 5. Crypto (58 leaks, 2.7 KB)
- **crypto/key.test.tml**: 12 leaks, 2.1 KB
- **crypto/x509.test.tml**: 29 leaks, 595 B
- **crypto/cipher_enum.test.tml**: 10 leaks, 400 B
- **crypto/cipher_helpers.test.tml**: 4 leaks, 291 B
- **crypto/cipher.test.tml**: 4 leaks, 192 B
- **crypto/rsa.test.tml**: 3 leaks, 110 B
- **crypto/x509_test_minimal.test.tml**: 2 leaks, 8 B

**Fix Strategy**:
1. Key material (private keys) creates internal string buffers
2. X.509 certificate parsing allocates memory
3. Ensure Crypto objects have proper cleanup

---

### 6. JSON (58 leaks, 638 B)
- **json_serialize_coverage.test.tml**: 13 leaks, 151 B
- **json_advanced.test.tml**: 13 leaks, 150 B
- **json_serialize.test.tml**: 10 leaks, 100 B
- **json_methods.test.tml**: 13 leaks, 47 B
- **json_from_json.test.tml**: 5 leaks, 45 B
- **json.test.tml**: 4 leaks, 41 B
- **json_pretty_arr.test.tml**: 2 leaks, 4 B

**Fix Strategy**:
1. JSON parsing creates string nodes
2. JSON serialization allocates buffer strings
3. Ensure Value.destroy() called after use

---

### 7. Events (36 leaks, 172 B) - **LOW PRIORITY**
String event names ("data", "error", etc.) - small leaks from test infrastructure
Not a real issue but fixable by clearing event registrations

---

### 8. Minor Leaks (<30 leaks each) - LOW PRIORITY
- **str_coverage.test.tml**: 19 leaks (string operations)
- **io_text.test.tml**: 6 leaks, 262 B
- **regex_basic.test.tml**: 16 leaks, 32 B
- **mime_methods.test.tml**: 23 leaks, 188 B
- **cli_mixed.test.tml**: 19 leaks, 117 B
- etc.

**Fix Strategy**:
1. Most are string allocations in test setup
2. Low impact (total ~2 KB)
3. Fix only after critical modules

---

## Implementation Priority

### Phase 1: Critical Fixes (Will reduce leaks by ~70%)
1. **HTTP Response cleanup** - `client.test.tml` (34.5 KB)
   - Add resp.destroy() or manage resp.text() lifecycle

### Phase 2: Network Parser Fixes (~600 B)
2. **IPv6/IPv4/Socket parsing** - Parser implementations
   - Fix intermediate string cleanup

### Phase 3: Collection/HashMap Fixes (~600 B)
3. **HashMap cleanup** - HashMap.destroy() implementation
   - Ensure all string keys freed

### Phase 4: URL/JSON/Crypto Fixes (~2 KB total)
4. Individual module cleanup methods

### Phase 5: Test Infrastructure (~200 B)
5. Events and misc string leaks

---

## Success Criteria
- [ ] Reduce http/client.test.tml from 52 leaks to <5 leaks
- [ ] Reduce total leaks from 1027 to <100
- [ ] Reduce total memory from 49.5 KB to <5 KB
- [ ] All major test suites leak <10 individual allocations
