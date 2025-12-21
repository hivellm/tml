# TML Standard Library: UUID

> `std.uuid` — Universally Unique Identifiers.

## Overview

The uuid package provides types and functions for generating and parsing UUIDs (Universally Unique Identifiers). It supports UUID versions 1, 4, 5, 6, 7, and 8.

**Capability**: `io.random` (for random-based UUIDs)

## Import

```tml
import std.uuid
import std.uuid.{Uuid, UuidVersion}
```

---

## Uuid Type

```tml
/// A 128-bit universally unique identifier
public type Uuid {
    bytes: [U8; 16],
}

extend Uuid {
    /// The nil UUID (all zeros)
    public const NIL: Uuid = Uuid { bytes: [0; 16] }

    /// The max UUID (all ones)
    public const MAX: Uuid = Uuid { bytes: [255; 16] }

    // Constructors

    /// Creates a nil UUID
    public func nil() -> Uuid {
        Uuid.NIL
    }

    /// Creates a random UUID (version 4)
    public func v4() -> Uuid
        caps: [io.random]

    /// Creates a time-based UUID (version 1)
    public func v1(node: &[U8; 6]) -> Uuid
        caps: [io.time, io.random]

    /// Creates a name-based UUID using SHA-1 (version 5)
    public func v5(namespace: &Uuid, name: &[U8]) -> Uuid

    /// Creates a name-based UUID using MD5 (version 3)
    public func v3(namespace: &Uuid, name: &[U8]) -> Uuid

    /// Creates a time-ordered UUID (version 6)
    public func v6(node: &[U8; 6]) -> Uuid
        caps: [io.time, io.random]

    /// Creates a Unix timestamp UUID (version 7)
    public func v7() -> Uuid
        caps: [io.time, io.random]

    /// Creates a custom UUID (version 8)
    public func v8(custom_a: U48, custom_b: U12, custom_c: U62) -> Uuid

    /// Creates from raw bytes
    public func from_bytes(bytes: [U8; 16]) -> Uuid {
        Uuid { bytes }
    }

    /// Creates from byte slice
    public func from_slice(slice: &[U8]) -> Result[Uuid, UuidError] {
        if slice.len() != 16 then {
            return Err(UuidError.InvalidLength)
        }
        var bytes = [0u8; 16]
        bytes.copy_from_slice(slice)
        return Ok(Uuid { bytes })
    }

    /// Parses from string
    public func parse(s: &String) -> Result[Uuid, UuidError]

    /// Parses from hyphenated format
    public func parse_hyphenated(s: &String) -> Result[Uuid, UuidError]

    /// Parses from simple (no hyphens) format
    public func parse_simple(s: &String) -> Result[Uuid, UuidError]

    /// Parses from URN format
    public func parse_urn(s: &String) -> Result[Uuid, UuidError]

    // Accessors

    /// Returns the raw bytes
    public func as_bytes(this) -> &[U8; 16] {
        &this.bytes
    }

    /// Returns the UUID version
    public func version(this) -> UuidVersion {
        let v = (this.bytes[6] >> 4) & 0x0F
        UuidVersion.from_u8(v).unwrap_or(UuidVersion.Unknown)
    }

    /// Returns the UUID variant
    public func variant(this) -> UuidVariant {
        let v = this.bytes[8]
        if (v & 0x80) == 0 then {
            return UuidVariant.Ncs
        } else if (v & 0xC0) == 0x80 then {
            return UuidVariant.Rfc4122
        } else if (v & 0xE0) == 0xC0 then {
            return UuidVariant.Microsoft
        } else {
            return UuidVariant.Future
        }
    }

    /// Returns true if this is the nil UUID
    public func is_nil(this) -> Bool {
        this.bytes == [0; 16]
    }

    /// Returns true if this is the max UUID
    public func is_max(this) -> Bool {
        this.bytes == [255; 16]
    }

    /// Returns the timestamp for time-based UUIDs (v1, v6, v7)
    public func timestamp(this) -> Option[I64]

    // Formatting

    /// Formats as hyphenated lowercase (standard format)
    public func to_string(this) -> String {
        this.to_hyphenated_lower()
    }

    /// Formats as hyphenated lowercase
    public func to_hyphenated_lower(this) -> String
    // "550e8400-e29b-41d4-a716-446655440000"

    /// Formats as hyphenated uppercase
    public func to_hyphenated_upper(this) -> String
    // "550E8400-E29B-41D4-A716-446655440000"

    /// Formats as simple lowercase (no hyphens)
    public func to_simple_lower(this) -> String
    // "550e8400e29b41d4a716446655440000"

    /// Formats as simple uppercase
    public func to_simple_upper(this) -> String

    /// Formats as URN
    public func to_urn(this) -> String
    // "urn:uuid:550e8400-e29b-41d4-a716-446655440000"

    /// Formats as braced (Microsoft style)
    public func to_braced(this) -> String
    // "{550e8400-e29b-41d4-a716-446655440000}"

    // Conversion

    /// Converts to u128
    public func to_u128(this) -> U128

    /// Creates from u128
    public func from_u128(value: U128) -> Uuid

    /// Returns the high and low 64-bit parts
    public func to_u64_pair(this) -> (U64, U64)

    /// Creates from high and low 64-bit parts
    public func from_u64_pair(high: U64, low: U64) -> Uuid
}

/// UUID version
public type UuidVersion =
    | V1    // Time-based
    | V2    // DCE Security
    | V3    // Name-based (MD5)
    | V4    // Random
    | V5    // Name-based (SHA-1)
    | V6    // Time-ordered
    | V7    // Unix timestamp
    | V8    // Custom
    | Unknown

extend UuidVersion {
    public func from_u8(v: U8) -> Option[UuidVersion] {
        when v {
            1 -> Some(V1),
            2 -> Some(V2),
            3 -> Some(V3),
            4 -> Some(V4),
            5 -> Some(V5),
            6 -> Some(V6),
            7 -> Some(V7),
            8 -> Some(V8),
            _ -> None,
        }
    }

    public func as_u8(this) -> U8 {
        when this {
            V1 -> 1,
            V2 -> 2,
            V3 -> 3,
            V4 -> 4,
            V5 -> 5,
            V6 -> 6,
            V7 -> 7,
            V8 -> 8,
            Unknown -> 0,
        }
    }
}

/// UUID variant
public type UuidVariant = Ncs | Rfc4122 | Microsoft | Future

/// UUID error
public type UuidError =
    | InvalidLength
    | InvalidCharacter(Char, U64)
    | InvalidFormat
    | InvalidVersion
```

---

## Namespaces

Standard namespaces for name-based UUIDs.

```tml
/// Standard UUID namespaces
public module namespace {
    /// DNS namespace
    public const DNS: Uuid = Uuid.parse("6ba7b810-9dad-11d1-80b4-00c04fd430c8").unwrap()

    /// URL namespace
    public const URL: Uuid = Uuid.parse("6ba7b811-9dad-11d1-80b4-00c04fd430c8").unwrap()

    /// OID namespace
    public const OID: Uuid = Uuid.parse("6ba7b812-9dad-11d1-80b4-00c04fd430c8").unwrap()

    /// X.500 DN namespace
    public const X500: Uuid = Uuid.parse("6ba7b814-9dad-11d1-80b4-00c04fd430c8").unwrap()
}
```

---

## Generator

Stateful UUID generator for high-performance scenarios.

```tml
/// UUID generator with state
public type UuidGenerator {
    counter: AtomicU64,
    node: [U8; 6],
    clock_seq: U16,
}

extend UuidGenerator {
    /// Creates a new generator
    public func new() -> UuidGenerator
        caps: [io.random]

    /// Creates a generator with specific node ID
    public func with_node(node: [U8; 6]) -> UuidGenerator
        caps: [io.random]

    /// Generates a v1 UUID
    public func generate_v1(mut this) -> Uuid
        caps: [io.time]

    /// Generates a v4 UUID
    public func generate_v4(this) -> Uuid
        caps: [io.random]

    /// Generates a v6 UUID
    public func generate_v6(mut this) -> Uuid
        caps: [io.time]

    /// Generates a v7 UUID
    public func generate_v7(mut this) -> Uuid
        caps: [io.time, io.random]

    /// Generates multiple v4 UUIDs efficiently
    public func generate_v4_batch(this, count: U64) -> Vec[Uuid]
        caps: [io.random]
}
```

---

## Traits

```tml
implement Eq for Uuid {
    func eq(this, other: &Uuid) -> Bool {
        this.bytes == other.bytes
    }
}

implement Ord for Uuid {
    func cmp(this, other: &Uuid) -> Ordering {
        this.bytes.cmp(&other.bytes)
    }
}

implement Hash for Uuid {
    func hash(this, hasher: &mut Hasher) {
        hasher.write(&this.bytes)
    }
}

implement Display for Uuid {
    func fmt(this, f: &mut Formatter) -> FmtResult {
        f.write_str(&this.to_hyphenated_lower())
    }
}

implement Debug for Uuid {
    func fmt(this, f: &mut Formatter) -> FmtResult {
        f.write_str("Uuid(")?
        f.write_str(&this.to_hyphenated_lower())?
        f.write_str(")")
    }
}

implement FromStr for Uuid {
    type Err = UuidError

    func from_str(s: &String) -> Result[Uuid, UuidError] {
        Uuid.parse(s)
    }
}
```

---

## Examples

### Generate UUIDs

```tml
import std.uuid.{Uuid, namespace}

func uuid_examples()
    caps: [io.random, io.time]
{
    // Random UUID (most common)
    let id = Uuid.v4()
    print("Random UUID: " + id.to_string())

    // Time-based UUID
    let node = [0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E]
    let time_id = Uuid.v1(&node)
    print("Time-based UUID: " + time_id.to_string())

    // Name-based UUID (deterministic)
    let name_id = Uuid.v5(&namespace.DNS, "example.com".as_bytes())
    print("Name-based UUID: " + name_id.to_string())

    // Modern time-ordered UUID (v7)
    let modern_id = Uuid.v7()
    print("Time-ordered UUID: " + modern_id.to_string())
}
```

### Parse and Format

```tml
import std.uuid.Uuid

func parse_format_examples() {
    // Parse various formats
    let uuid1 = Uuid.parse("550e8400-e29b-41d4-a716-446655440000").unwrap()
    let uuid2 = Uuid.parse("550e8400e29b41d4a716446655440000").unwrap()
    let uuid3 = Uuid.parse("urn:uuid:550e8400-e29b-41d4-a716-446655440000").unwrap()
    let uuid4 = Uuid.parse("{550e8400-e29b-41d4-a716-446655440000}").unwrap()

    assert(uuid1 == uuid2)
    assert(uuid2 == uuid3)
    assert(uuid3 == uuid4)

    // Format in different styles
    print(uuid1.to_hyphenated_lower())  // Standard
    print(uuid1.to_simple_lower())       // No hyphens
    print(uuid1.to_urn())                // URN format
    print(uuid1.to_braced())             // Microsoft style
}
```

### UUID as Database Key

```tml
import std.uuid.Uuid
import std.collections.HashMap

type User {
    id: Uuid,
    name: String,
    email: String,
}

type UserStore {
    users: HashMap[Uuid, User],
}

extend UserStore {
    func new() -> UserStore {
        UserStore { users: HashMap.new() }
    }

    func create_user(mut this, name: String, email: String) -> Uuid
        caps: [io.random]
    {
        let id = Uuid.v4()
        let user = User { id: id, name: name, email: email }
        this.users.insert(id, user)
        return id
    }

    func get_user(this, id: &Uuid) -> Option[&User] {
        this.users.get(id)
    }
}
```

### Batch Generation

```tml
import std.uuid.{Uuid, UuidGenerator}

func batch_example()
    caps: [io.random]
{
    let generator = UuidGenerator.new()

    // Generate 1000 UUIDs efficiently
    let uuids = generator.generate_v4_batch(1000)

    loop uuid in uuids.iter().take(5) {
        print(uuid.to_string())
    }
}
```

---

## Version Comparison

| Version | Based On | Sortable | Privacy |
|---------|----------|----------|---------|
| v1 | Timestamp + MAC | No | Low (leaks MAC) |
| v3 | MD5(namespace + name) | No | High |
| v4 | Random | No | High |
| v5 | SHA1(namespace + name) | No | High |
| v6 | Timestamp (reordered) | Yes | Medium |
| v7 | Unix timestamp + random | Yes | High |
| v8 | Custom | Depends | Depends |

**Recommendations:**
- Use **v4** for general-purpose random IDs
- Use **v7** for sortable IDs (database primary keys)
- Use **v5** for deterministic IDs from names

---

## See Also

- [std.crypto](./05-CRYPTO.md) — Cryptographic functions (MD5, SHA-1)
- [std.datetime](./16-DATETIME.md) — Timestamps
