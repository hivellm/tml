# Specification: JSON Builder

## 1. Interface

```cpp
class JsonBuilder {
    struct Frame {
        std::unique_ptr<JsonValue> value;
        bool is_object;
    };
    std::vector<Frame> stack_;

public:
    JsonBuilder();

    // Structure
    auto object() -> JsonBuilder&;
    auto array() -> JsonBuilder&;
    auto end() -> JsonBuilder&;

    // Object fields
    auto null_field(std::string_view key) -> JsonBuilder&;
    auto bool_field(std::string_view key, bool value) -> JsonBuilder&;
    auto int_field(std::string_view key, int64_t value) -> JsonBuilder&;
    auto float_field(std::string_view key, double value) -> JsonBuilder&;
    auto string_field(std::string_view key, std::string_view value) -> JsonBuilder&;
    auto object_field(std::string_view key) -> JsonBuilder&;
    auto array_field(std::string_view key) -> JsonBuilder&;

    // Array items
    auto null_item() -> JsonBuilder&;
    auto bool_item(bool value) -> JsonBuilder&;
    auto int_item(int64_t value) -> JsonBuilder&;
    auto float_item(double value) -> JsonBuilder&;
    auto string_item(std::string_view value) -> JsonBuilder&;
    auto object_item() -> JsonBuilder&;
    auto array_item() -> JsonBuilder&;

    // Finalize
    auto build() -> std::unique_ptr<JsonValue>;
};
```

## 2. Usage Example

```cpp
auto json = JsonBuilder()
    .object()
        .string_field("name", "Alice")
        .int_field("age", 30)
        .array_field("tags")
            .string_item("developer")
            .string_item("rust")
        .end()
    .end()
    .build();
```

## 3. Error Handling

- Calling `end()` with empty stack: runtime error
- Calling field methods outside object context: runtime error
- Calling item methods outside array context: runtime error
- Calling `build()` with non-empty stack: runtime error
