# Specification: JSON Value Representation

## 1. Number Type Optimization

JSON numbers are stored with type discrimination to optimize for common cases:

```cpp
namespace tml::json {

/// Discriminated number type for optimal storage and precision
struct JsonNumber {
    enum class Kind : uint8_t {
        Int64,      ///< Fits in int64_t (most common for IDs, counts, etc.)
        Uint64,     ///< Fits in uint64_t but not int64_t
        Double,     ///< Floating point or out of integer range
    };

    Kind kind;
    union {
        int64_t  i64;
        uint64_t u64;
        double   f64;
    };

    // Constructors
    static auto from_i64(int64_t v) -> JsonNumber;
    static auto from_u64(uint64_t v) -> JsonNumber;
    static auto from_f64(double v) -> JsonNumber;

    // Type queries
    [[nodiscard]] auto is_integer() const -> bool { return kind != Kind::Double; }
    [[nodiscard]] auto is_signed() const -> bool { return kind == Kind::Int64; }
    [[nodiscard]] auto is_unsigned() const -> bool { return kind == Kind::Uint64; }
    [[nodiscard]] auto is_float() const -> bool { return kind == Kind::Double; }

    // Safe conversions (return false if lossy)
    [[nodiscard]] auto try_as_i64(int64_t& out) const -> bool;
    [[nodiscard]] auto try_as_u64(uint64_t& out) const -> bool;
    [[nodiscard]] auto try_as_i32(int32_t& out) const -> bool;
    [[nodiscard]] auto try_as_u32(uint32_t& out) const -> bool;

    // Always-available conversions
    [[nodiscard]] auto as_f64() const -> double;
    [[nodiscard]] auto as_i64_unchecked() const -> int64_t;
    [[nodiscard]] auto as_u64_unchecked() const -> uint64_t;

    // Comparison
    [[nodiscard]] auto operator==(const JsonNumber& other) const -> bool;
};

} // namespace tml::json
```

## 2. C++ JsonValue

```cpp
namespace tml::json {

class JsonValue {
public:
    using Null   = std::nullptr_t;
    using Bool   = bool;
    using Number = JsonNumber;  // Optimized number type
    using String = std::string;
    using Array  = std::vector<std::unique_ptr<JsonValue>>;
    using Object = std::unordered_map<std::string, std::unique_ptr<JsonValue>>;

private:
    std::variant<Null, Bool, Number, String, Array, Object> data_;

public:
    // Type queries
    [[nodiscard]] auto is_null() const -> bool;
    [[nodiscard]] auto is_bool() const -> bool;
    [[nodiscard]] auto is_number() const -> bool;
    [[nodiscard]] auto is_integer() const -> bool;  // Number that is int64/uint64
    [[nodiscard]] auto is_float() const -> bool;    // Number that is double
    [[nodiscard]] auto is_string() const -> bool;
    [[nodiscard]] auto is_array() const -> bool;
    [[nodiscard]] auto is_object() const -> bool;

    // Number accessors (with type optimization)
    [[nodiscard]] auto as_number() const -> const JsonNumber&;
    [[nodiscard]] auto as_i64() const -> int64_t;       // Throws if not integer or overflow
    [[nodiscard]] auto as_u64() const -> uint64_t;      // Throws if negative or overflow
    [[nodiscard]] auto as_i32() const -> int32_t;       // Throws if overflow
    [[nodiscard]] auto as_u32() const -> uint32_t;      // Throws if negative or overflow
    [[nodiscard]] auto as_f64() const -> double;        // Always works for numbers
    [[nodiscard]] auto try_as_i64() const -> std::optional<int64_t>;
    [[nodiscard]] auto try_as_u64() const -> std::optional<uint64_t>;

    // Other accessors
    [[nodiscard]] auto as_bool() const -> bool;
    [[nodiscard]] auto as_string() const -> const std::string&;
    [[nodiscard]] auto as_array() const -> const Array&;
    [[nodiscard]] auto as_object() const -> const Object&;

    // Mutable accessors
    [[nodiscard]] auto as_array_mut() -> Array&;
    [[nodiscard]] auto as_object_mut() -> Object&;

    // Object access
    [[nodiscard]] auto get(std::string_view key) const -> const JsonValue*;
    [[nodiscard]] auto get_mut(std::string_view key) -> JsonValue*;
    void set(std::string key, std::unique_ptr<JsonValue> value);
    void remove(std::string_view key);

    // Array access
    [[nodiscard]] auto operator[](size_t index) const -> const JsonValue&;
    [[nodiscard]] auto operator[](size_t index) -> JsonValue&;
    void push(std::unique_ptr<JsonValue> value);
    [[nodiscard]] auto size() const -> size_t;

    // Factory methods
    static auto null() -> std::unique_ptr<JsonValue>;
    static auto boolean(bool v) -> std::unique_ptr<JsonValue>;
    static auto integer(int64_t v) -> std::unique_ptr<JsonValue>;
    static auto unsigned_int(uint64_t v) -> std::unique_ptr<JsonValue>;
    static auto floating(double v) -> std::unique_ptr<JsonValue>;
    static auto string(std::string v) -> std::unique_ptr<JsonValue>;
    static auto array() -> std::unique_ptr<JsonValue>;
    static auto object() -> std::unique_ptr<JsonValue>;
};

} // namespace tml::json
```

## 3. Memory Layout

- `JsonValue` uses `std::variant` for type-safe union
- `JsonNumber` uses discriminated union (16 bytes: 8 byte value + kind + padding)
- Arrays use `std::vector<std::unique_ptr<JsonValue>>` for ownership
- Objects use `std::unordered_map<std::string, std::unique_ptr<JsonValue>>`
- No shared ownership - single owner for each value
- Move semantics for efficient transfers

## 4. Number Parsing Strategy

```cpp
// Parser determines number type based on format:
// 1. No decimal point and no exponent -> try integer
// 2. Has decimal point or exponent -> double
// 3. Integer overflow -> promote to double

auto parse_number(std::string_view text) -> JsonNumber {
    bool has_decimal = text.find('.') != std::string_view::npos;
    bool has_exponent = text.find_first_of("eE") != std::string_view::npos;

    if (!has_decimal && !has_exponent) {
        // Try parsing as integer
        bool negative = text[0] == '-';
        if (negative) {
            int64_t val;
            if (try_parse_i64(text, val)) {
                return JsonNumber::from_i64(val);
            }
        } else {
            uint64_t val;
            if (try_parse_u64(text, val)) {
                // Use i64 if it fits, otherwise u64
                if (val <= INT64_MAX) {
                    return JsonNumber::from_i64(static_cast<int64_t>(val));
                }
                return JsonNumber::from_u64(val);
            }
        }
    }
    // Fall back to double
    return JsonNumber::from_f64(std::stod(std::string(text)));
}
```

## 5. Number Serialization Strategy

```cpp
// Serializer preserves integer format when possible:
auto serialize_number(const JsonNumber& num) -> std::string {
    switch (num.kind) {
    case JsonNumber::Kind::Int64:
        return std::to_string(num.i64);
    case JsonNumber::Kind::Uint64:
        return std::to_string(num.u64);
    case JsonNumber::Kind::Double:
        // Use shortest representation that round-trips
        return format_double(num.f64);
    }
}
```
