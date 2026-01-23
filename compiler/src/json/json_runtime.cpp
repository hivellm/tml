/**
 * @file json_runtime.cpp
 * @brief TML Runtime - JSON Parser Bindings
 *
 * Exposes the native C++ JSON parser to TML code via C linkage functions.
 * This enables TML programs to use the optimized native JSON parser.
 */

#include "common.hpp"

#include "json/json.hpp"
#include "json/json_fast_parser.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// FFI Profiling Infrastructure
// ============================================================================

struct JsonFfiStats {
    std::atomic<int64_t> parse_count{0};
    std::atomic<int64_t> parse_time_ns{0};
    std::atomic<int64_t> handle_alloc_count{0};
    std::atomic<int64_t> handle_alloc_time_ns{0};
    std::atomic<int64_t> clone_count{0};
    std::atomic<int64_t> clone_time_ns{0};
    std::atomic<int64_t> field_access_count{0};
    std::atomic<int64_t> field_access_time_ns{0};
    std::atomic<bool> enabled{false};

    void reset() {
        parse_count = 0;
        parse_time_ns = 0;
        handle_alloc_count = 0;
        handle_alloc_time_ns = 0;
        clone_count = 0;
        clone_time_ns = 0;
        field_access_count = 0;
        field_access_time_ns = 0;
    }
};

static JsonFfiStats g_json_stats;

// RAII timer for profiling
class ScopedTimer {
    std::atomic<int64_t>& counter_;
    std::chrono::high_resolution_clock::time_point start_;
    bool enabled_;

public:
    ScopedTimer(std::atomic<int64_t>& counter, bool enabled)
        : counter_(counter), enabled_(enabled) {
        if (enabled_) {
            start_ = std::chrono::high_resolution_clock::now();
        }
    }
    ~ScopedTimer() {
        if (enabled_) {
            auto end = std::chrono::high_resolution_clock::now();
            counter_ += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
        }
    }
};

// Export macro for DLL visibility
#ifdef _WIN32
#define TML_EXPORT extern "C" __declspec(dllexport)
#else
#define TML_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// ============================================================================
// JSON Value Handle (opaque pointer to C++ JsonValue)
// ============================================================================

// We use a simple handle system to manage JSON values from TML
// The handle is an index into a global vector of JsonValues

static std::vector<tml::json::JsonValue> json_values;
static std::vector<bool> json_values_free;
static size_t json_values_next_free = 0;

// Allocate a new handle for a JsonValue
static int64_t alloc_json_handle(tml::json::JsonValue&& value) {
    ScopedTimer timer(g_json_stats.handle_alloc_time_ns, g_json_stats.enabled);
    if (g_json_stats.enabled)
        g_json_stats.handle_alloc_count++;

    // Find a free slot or expand
    for (size_t i = json_values_next_free; i < json_values_free.size(); ++i) {
        if (json_values_free[i]) {
            json_values[i] = std::move(value);
            json_values_free[i] = false;
            json_values_next_free = i + 1;
            return static_cast<int64_t>(i);
        }
    }
    // No free slot, append
    size_t idx = json_values.size();
    json_values.push_back(std::move(value));
    json_values_free.push_back(false);
    json_values_next_free = idx + 1;
    return static_cast<int64_t>(idx);
}

// Get JsonValue by handle (returns nullptr if invalid)
static tml::json::JsonValue* get_json_value(int64_t handle) {
    if (handle < 0 || static_cast<size_t>(handle) >= json_values.size()) {
        return nullptr;
    }
    if (json_values_free[static_cast<size_t>(handle)]) {
        return nullptr;
    }
    return &json_values[static_cast<size_t>(handle)];
}

// ============================================================================
// Parsing Functions
// ============================================================================

/**
 * @brief Parse JSON string using the fast SIMD-optimized parser.
 *
 * @param json_str The JSON string to parse (null-terminated)
 * @return Handle to the parsed JsonValue, or -1 on error
 */
TML_EXPORT int64_t tml_json_parse_fast(const char* json_str) {
    if (!json_str)
        return -1;

    if (g_json_stats.enabled)
        g_json_stats.parse_count++;
    ScopedTimer timer(g_json_stats.parse_time_ns, g_json_stats.enabled);

    auto result = tml::json::fast::parse_json_fast(json_str);
    if (tml::is_ok(result)) {
        return alloc_json_handle(std::move(tml::unwrap(result)));
    }
    return -1;
}

/**
 * @brief Parse JSON string using the standard parser.
 *
 * @param json_str The JSON string to parse (null-terminated)
 * @return Handle to the parsed JsonValue, or -1 on error
 */
TML_EXPORT int64_t tml_json_parse(const char* json_str) {
    if (!json_str)
        return -1;

    auto result = tml::json::parse_json(json_str);
    if (tml::is_ok(result)) {
        return alloc_json_handle(std::move(tml::unwrap(result)));
    }
    return -1;
}

/**
 * @brief Parse JSON with length (for strings that may not be null-terminated).
 *
 * @param json_str The JSON string to parse
 * @param len Length of the string
 * @return Handle to the parsed JsonValue, or -1 on error
 */
TML_EXPORT int64_t tml_json_parse_len(const char* json_str, int64_t len) {
    if (!json_str || len < 0)
        return -1;

    std::string_view sv(json_str, static_cast<size_t>(len));
    auto result = tml::json::fast::parse_json_fast(sv);
    if (tml::is_ok(result)) {
        return alloc_json_handle(std::move(tml::unwrap(result)));
    }
    return -1;
}

// ============================================================================
// Value Access Functions
// ============================================================================

/**
 * @brief Get the type of a JSON value.
 *
 * @param handle Handle to the JsonValue
 * @return Type code: 0=null, 1=bool, 2=number, 3=string, 4=array, 5=object, -1=invalid
 */
TML_EXPORT int32_t tml_json_get_type(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value)
        return -1;

    if (value->is_null())
        return 0;
    if (value->is_bool())
        return 1;
    if (value->is_number())
        return 2;
    if (value->is_string())
        return 3;
    if (value->is_array())
        return 4;
    if (value->is_object())
        return 5;
    return -1;
}

// Type checking helper functions
TML_EXPORT int32_t tml_json_is_null(int64_t handle) {
    auto* value = get_json_value(handle);
    return (value && value->is_null()) ? 1 : 0;
}

TML_EXPORT int32_t tml_json_is_bool(int64_t handle) {
    auto* value = get_json_value(handle);
    return (value && value->is_bool()) ? 1 : 0;
}

TML_EXPORT int32_t tml_json_is_number(int64_t handle) {
    auto* value = get_json_value(handle);
    return (value && value->is_number()) ? 1 : 0;
}

TML_EXPORT int32_t tml_json_is_string(int64_t handle) {
    auto* value = get_json_value(handle);
    return (value && value->is_string()) ? 1 : 0;
}

TML_EXPORT int32_t tml_json_is_array(int64_t handle) {
    auto* value = get_json_value(handle);
    return (value && value->is_array()) ? 1 : 0;
}

TML_EXPORT int32_t tml_json_is_object(int64_t handle) {
    auto* value = get_json_value(handle);
    return (value && value->is_object()) ? 1 : 0;
}

/**
 * @brief Get boolean value.
 *
 * @param handle Handle to the JsonValue
 * @return 1 for true, 0 for false, -1 if not a boolean
 */
TML_EXPORT int32_t tml_json_get_bool(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_bool())
        return -1;
    return value->as_bool() ? 1 : 0;
}

/**
 * @brief Get boolean value as 1 or 0.
 *
 * @param handle Handle to the JsonValue
 * @return 1 for true, 0 for false or if not a boolean
 */
TML_EXPORT int32_t tml_json_as_bool(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_bool())
        return 0;
    return value->as_bool() ? 1 : 0;
}

/**
 * @brief Get integer value (I64) with out parameter.
 *
 * @param handle Handle to the JsonValue
 * @param out_value Pointer to store the result
 * @return 1 on success, 0 on failure
 */
TML_EXPORT int32_t tml_json_get_i64(int64_t handle, int64_t* out_value) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_number() || !out_value)
        return 0;

    const auto& num = value->as_number();
    if (num.is_integer()) {
        auto maybe_val = num.try_as_i64();
        if (maybe_val) {
            *out_value = *maybe_val;
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Get integer value directly (returns 0 if not a number).
 *
 * @param handle Handle to the JsonValue
 * @return The integer value, or 0 if not a number
 */
TML_EXPORT int64_t tml_json_as_i64(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_number())
        return 0;

    const auto& num = value->as_number();
    if (num.is_integer()) {
        auto maybe_val = num.try_as_i64();
        if (maybe_val) {
            return *maybe_val;
        }
    }
    return static_cast<int64_t>(num.as_f64());
}

/**
 * @brief Get floating point value (F64) with out parameter.
 *
 * @param handle Handle to the JsonValue
 * @param out_value Pointer to store the result
 * @return 1 on success, 0 on failure
 */
TML_EXPORT int32_t tml_json_get_f64(int64_t handle, double* out_value) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_number() || !out_value)
        return 0;

    const auto& num = value->as_number();
    *out_value = num.as_f64();
    return 1;
}

/**
 * @brief Get floating point value directly (returns 0.0 if not a number).
 *
 * @param handle Handle to the JsonValue
 * @return The float value, or 0.0 if not a number
 */
TML_EXPORT double tml_json_as_f64(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_number())
        return 0.0;

    return value->as_number().as_f64();
}

// Static buffer for returning strings
static char json_string_buffer[65536];

/**
 * @brief Get string value.
 *
 * @param handle Handle to the JsonValue
 * @return Pointer to the string (static buffer), or NULL on failure
 */
TML_EXPORT const char* tml_json_get_string(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_string())
        return nullptr;

    const auto& str = value->as_string();
    if (str.length() >= sizeof(json_string_buffer)) {
        // String too large for buffer
        return nullptr;
    }
    std::memcpy(json_string_buffer, str.data(), str.length());
    json_string_buffer[str.length()] = '\0';
    return json_string_buffer;
}

/**
 * @brief Get string length.
 *
 * @param handle Handle to the JsonValue
 * @return Length of the string, or -1 if not a string
 */
TML_EXPORT int64_t tml_json_get_string_len(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_string())
        return -1;
    return static_cast<int64_t>(value->as_string().length());
}

// ============================================================================
// Array Functions
// ============================================================================

/**
 * @brief Get array length.
 *
 * @param handle Handle to the JsonValue
 * @return Array length, or -1 if not an array
 */
TML_EXPORT int64_t tml_json_array_len(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_array())
        return -1;
    return static_cast<int64_t>(value->as_array().size());
}

/**
 * @brief Get array element (clones the value - use direct access for primitives).
 *
 * @param handle Handle to the JsonValue (array)
 * @param index Array index
 * @return Handle to the element, or -1 on failure
 */
TML_EXPORT int64_t tml_json_array_get(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_array())
        return -1;

    const auto& arr = value->as_array();
    if (index < 0 || static_cast<size_t>(index) >= arr.size())
        return -1;

    // Profile clone operation
    if (g_json_stats.enabled)
        g_json_stats.clone_count++;
    ScopedTimer timer(g_json_stats.clone_time_ns, g_json_stats.enabled);

    // Clone the element to a new handle (JsonValue has deleted copy ctor)
    return alloc_json_handle(arr[static_cast<size_t>(index)].clone());
}

// ============================================================================
// Object Functions
// ============================================================================

/**
 * @brief Get object field count.
 *
 * @param handle Handle to the JsonValue
 * @return Number of fields, or -1 if not an object
 */
TML_EXPORT int64_t tml_json_object_len(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object())
        return -1;
    return static_cast<int64_t>(value->as_object().size());
}

/**
 * @brief Get object field by key (clones the value - use direct access for primitives).
 *
 * @param handle Handle to the JsonValue (object)
 * @param key Field key (null-terminated)
 * @return Handle to the field value, or -1 if not found
 */
TML_EXPORT int64_t tml_json_object_get(int64_t handle, const char* key) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object() || !key)
        return -1;

    if (g_json_stats.enabled)
        g_json_stats.field_access_count++;
    ScopedTimer lookup_timer(g_json_stats.field_access_time_ns, g_json_stats.enabled);

    const auto* field = value->get(key);
    if (!field)
        return -1;

    // Profile clone operation
    if (g_json_stats.enabled)
        g_json_stats.clone_count++;
    ScopedTimer clone_timer(g_json_stats.clone_time_ns, g_json_stats.enabled);

    // Clone the field value to a new handle (JsonValue has deleted copy ctor)
    return alloc_json_handle(field->clone());
}

/**
 * @brief Check if object has a field.
 *
 * @param handle Handle to the JsonValue (object)
 * @param key Field key (null-terminated)
 * @return 1 if field exists, 0 otherwise
 */
TML_EXPORT int32_t tml_json_object_has(int64_t handle, const char* key) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object() || !key)
        return 0;
    return value->get(key) != nullptr ? 1 : 0;
}

// ============================================================================
// Memory Management
// ============================================================================

/**
 * @brief Free a JSON value handle.
 *
 * @param handle Handle to free
 */
TML_EXPORT void tml_json_free(int64_t handle) {
    if (handle < 0 || static_cast<size_t>(handle) >= json_values.size())
        return;

    size_t idx = static_cast<size_t>(handle);
    if (!json_values_free[idx]) {
        json_values[idx] = tml::json::JsonValue(); // Reset to null
        json_values_free[idx] = true;
        if (idx < json_values_next_free) {
            json_values_next_free = idx;
        }
    }
}

/**
 * @brief Free all JSON value handles (cleanup).
 */
TML_EXPORT void tml_json_free_all() {
    json_values.clear();
    json_values_free.clear();
    json_values_next_free = 0;
}

// ============================================================================
// Serialization
// ============================================================================

/**
 * @brief Serialize JSON value to string.
 *
 * @param handle Handle to the JsonValue
 * @return Pointer to the JSON string (static buffer), or NULL on failure
 */
TML_EXPORT const char* tml_json_to_string(int64_t handle) {
    auto* value = get_json_value(handle);
    if (!value)
        return nullptr;

    std::string result = value->to_string();
    if (result.length() >= sizeof(json_string_buffer)) {
        return nullptr;
    }
    std::memcpy(json_string_buffer, result.data(), result.length());
    json_string_buffer[result.length()] = '\0';
    return json_string_buffer;
}

// ============================================================================
// Benchmark Helper - Parse and measure without returning value
// ============================================================================

/**
 * @brief Parse JSON and immediately free (for benchmarking parse speed).
 *
 * @param json_str The JSON string to parse
 * @return 1 on success, 0 on failure
 */
TML_EXPORT int32_t tml_json_parse_fast_bench(const char* json_str) {
    if (!json_str)
        return 0;

    auto result = tml::json::fast::parse_json_fast(json_str);
    return tml::is_ok(result) ? 1 : 0;
}

/**
 * @brief Parse JSON with standard parser (for benchmarking).
 *
 * @param json_str The JSON string to parse
 * @return 1 on success, 0 on failure
 */
TML_EXPORT int32_t tml_json_parse_bench(const char* json_str) {
    if (!json_str)
        return 0;

    auto result = tml::json::parse_json(json_str);
    return tml::is_ok(result) ? 1 : 0;
}

// ============================================================================
// FFI Profiling API
// ============================================================================

/**
 * @brief Enable FFI profiling.
 */
TML_EXPORT void tml_json_profile_enable() {
    g_json_stats.reset();
    g_json_stats.enabled = true;
}

/**
 * @brief Disable FFI profiling.
 */
TML_EXPORT void tml_json_profile_disable() {
    g_json_stats.enabled = false;
}

/**
 * @brief Reset profiling statistics.
 */
TML_EXPORT void tml_json_profile_reset() {
    g_json_stats.reset();
}

/**
 * @brief Get parse operation count.
 */
TML_EXPORT int64_t tml_json_profile_parse_count() {
    return g_json_stats.parse_count.load();
}

/**
 * @brief Get total parse time in nanoseconds.
 */
TML_EXPORT int64_t tml_json_profile_parse_time_ns() {
    return g_json_stats.parse_time_ns.load();
}

/**
 * @brief Get handle allocation count.
 */
TML_EXPORT int64_t tml_json_profile_alloc_count() {
    return g_json_stats.handle_alloc_count.load();
}

/**
 * @brief Get total handle allocation time in nanoseconds.
 */
TML_EXPORT int64_t tml_json_profile_alloc_time_ns() {
    return g_json_stats.handle_alloc_time_ns.load();
}

/**
 * @brief Get clone operation count (array_get/object_get).
 */
TML_EXPORT int64_t tml_json_profile_clone_count() {
    return g_json_stats.clone_count.load();
}

/**
 * @brief Get total clone time in nanoseconds.
 */
TML_EXPORT int64_t tml_json_profile_clone_time_ns() {
    return g_json_stats.clone_time_ns.load();
}

/**
 * @brief Get field access count (object_get lookups).
 */
TML_EXPORT int64_t tml_json_profile_field_access_count() {
    return g_json_stats.field_access_count.load();
}

/**
 * @brief Get total field access time in nanoseconds.
 */
TML_EXPORT int64_t tml_json_profile_field_access_time_ns() {
    return g_json_stats.field_access_time_ns.load();
}

/**
 * @brief Print profiling summary to stdout.
 */
TML_EXPORT void tml_json_profile_print() {
    auto parse_count = g_json_stats.parse_count.load();
    auto parse_time = g_json_stats.parse_time_ns.load();
    auto alloc_count = g_json_stats.handle_alloc_count.load();
    auto alloc_time = g_json_stats.handle_alloc_time_ns.load();
    auto clone_count = g_json_stats.clone_count.load();
    auto clone_time = g_json_stats.clone_time_ns.load();
    auto field_count = g_json_stats.field_access_count.load();
    auto field_time = g_json_stats.field_access_time_ns.load();

    printf("\n");
    printf("============================================================\n");
    printf("           TML JSON FFI Profiling Results\n");
    printf("============================================================\n\n");

    printf("PARSING:\n");
    printf("  Count:      %lld\n", (long long)parse_count);
    printf("  Total time: %lld ns (%.3f ms)\n", (long long)parse_time, parse_time / 1000000.0);
    if (parse_count > 0) {
        printf("  Per op:     %lld ns\n", (long long)(parse_time / parse_count));
    }
    printf("\n");

    printf("HANDLE ALLOCATION:\n");
    printf("  Count:      %lld\n", (long long)alloc_count);
    printf("  Total time: %lld ns (%.3f ms)\n", (long long)alloc_time, alloc_time / 1000000.0);
    if (alloc_count > 0) {
        printf("  Per op:     %lld ns\n", (long long)(alloc_time / alloc_count));
    }
    printf("\n");

    printf("CLONE OPERATIONS (array_get/object_get):\n");
    printf("  Count:      %lld\n", (long long)clone_count);
    printf("  Total time: %lld ns (%.3f ms)\n", (long long)clone_time, clone_time / 1000000.0);
    if (clone_count > 0) {
        printf("  Per op:     %lld ns\n", (long long)(clone_time / clone_count));
    }
    printf("\n");

    printf("FIELD ACCESS (object lookup):\n");
    printf("  Count:      %lld\n", (long long)field_count);
    printf("  Total time: %lld ns (%.3f ms)\n", (long long)field_time, field_time / 1000000.0);
    if (field_count > 0) {
        printf("  Per op:     %lld ns\n", (long long)(field_time / field_count));
    }
    printf("\n");

    int64_t total_time = parse_time + clone_time + field_time;
    printf("TIME BREAKDOWN:\n");
    printf("  Parsing:      %.1f%%\n", total_time > 0 ? (parse_time * 100.0 / total_time) : 0);
    printf("  Cloning:      %.1f%%\n", total_time > 0 ? (clone_time * 100.0 / total_time) : 0);
    printf("  Field lookup: %.1f%%\n", total_time > 0 ? (field_time * 100.0 / total_time) : 0);
    printf("\n");
    printf("============================================================\n");
}

// ============================================================================
// Zero-Copy Direct Access (avoids clone overhead)
// ============================================================================

/**
 * @brief Get integer value directly from array element (no clone).
 *
 * @param handle Handle to array
 * @param index Array index
 * @return Integer value, or 0 if invalid
 */
TML_EXPORT int64_t tml_json_array_get_i64(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_array())
        return 0;

    const auto& arr = value->as_array();
    if (index < 0 || static_cast<size_t>(index) >= arr.size())
        return 0;

    const auto& elem = arr[static_cast<size_t>(index)];
    if (!elem.is_number())
        return 0;

    return elem.as_number().try_as_i64().value_or(0);
}

/**
 * @brief Get float value directly from array element (no clone).
 */
TML_EXPORT double tml_json_array_get_f64(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_array())
        return 0.0;

    const auto& arr = value->as_array();
    if (index < 0 || static_cast<size_t>(index) >= arr.size())
        return 0.0;

    const auto& elem = arr[static_cast<size_t>(index)];
    if (!elem.is_number())
        return 0.0;

    return elem.as_number().as_f64();
}

/**
 * @brief Get boolean value directly from array element (no clone).
 */
TML_EXPORT int32_t tml_json_array_get_bool(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_array())
        return 0;

    const auto& arr = value->as_array();
    if (index < 0 || static_cast<size_t>(index) >= arr.size())
        return 0;

    const auto& elem = arr[static_cast<size_t>(index)];
    if (!elem.is_bool())
        return 0;

    return elem.as_bool() ? 1 : 0;
}

/**
 * @brief Get string value directly from array element (no clone).
 * Uses static buffer - not thread safe.
 */
TML_EXPORT const char* tml_json_array_get_string(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_array())
        return nullptr;

    const auto& arr = value->as_array();
    if (index < 0 || static_cast<size_t>(index) >= arr.size())
        return nullptr;

    const auto& elem = arr[static_cast<size_t>(index)];
    if (!elem.is_string())
        return nullptr;

    const auto& str = elem.as_string();
    if (str.length() >= sizeof(json_string_buffer))
        return nullptr;

    std::memcpy(json_string_buffer, str.data(), str.length());
    json_string_buffer[str.length()] = '\0';
    return json_string_buffer;
}

/**
 * @brief Get type of array element (no clone).
 */
TML_EXPORT int32_t tml_json_array_get_type(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_array())
        return -1;

    const auto& arr = value->as_array();
    if (index < 0 || static_cast<size_t>(index) >= arr.size())
        return -1;

    const auto& elem = arr[static_cast<size_t>(index)];
    if (elem.is_null())
        return 0;
    if (elem.is_bool())
        return 1;
    if (elem.is_number())
        return 2;
    if (elem.is_string())
        return 3;
    if (elem.is_array())
        return 4;
    if (elem.is_object())
        return 5;
    return -1;
}

/**
 * @brief Get integer value directly from object field (no clone).
 */
TML_EXPORT int64_t tml_json_object_get_i64(int64_t handle, const char* key) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object() || !key)
        return 0;

    const auto* field = value->get(key);
    if (!field || !field->is_number())
        return 0;

    return field->as_number().try_as_i64().value_or(0);
}

/**
 * @brief Get float value directly from object field (no clone).
 */
TML_EXPORT double tml_json_object_get_f64(int64_t handle, const char* key) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object() || !key)
        return 0.0;

    const auto* field = value->get(key);
    if (!field || !field->is_number())
        return 0.0;

    return field->as_number().as_f64();
}

/**
 * @brief Get boolean value directly from object field (no clone).
 */
TML_EXPORT int32_t tml_json_object_get_bool(int64_t handle, const char* key) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object() || !key)
        return 0;

    const auto* field = value->get(key);
    if (!field || !field->is_bool())
        return 0;

    return field->as_bool() ? 1 : 0;
}

/**
 * @brief Get string value directly from object field (no clone).
 * Uses static buffer - not thread safe.
 */
TML_EXPORT const char* tml_json_object_get_string(int64_t handle, const char* key) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object() || !key)
        return nullptr;

    const auto* field = value->get(key);
    if (!field || !field->is_string())
        return nullptr;

    const auto& str = field->as_string();
    if (str.length() >= sizeof(json_string_buffer))
        return nullptr;

    std::memcpy(json_string_buffer, str.data(), str.length());
    json_string_buffer[str.length()] = '\0';
    return json_string_buffer;
}

/**
 * @brief Get type of object field (no clone).
 */
TML_EXPORT int32_t tml_json_object_get_type(int64_t handle, const char* key) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object() || !key)
        return -1;

    const auto* field = value->get(key);
    if (!field)
        return -1;

    if (field->is_null())
        return 0;
    if (field->is_bool())
        return 1;
    if (field->is_number())
        return 2;
    if (field->is_string())
        return 3;
    if (field->is_array())
        return 4;
    if (field->is_object())
        return 5;
    return -1;
}

// ============================================================================
// Object Key Iteration
// ============================================================================

// Static buffer for returning keys
static char json_key_buffer[4096];

/**
 * @brief Get object key at index.
 *
 * @param handle Handle to the JsonValue (object)
 * @param index Key index (0-based)
 * @return Pointer to the key string (static buffer), or NULL if out of bounds
 */
TML_EXPORT const char* tml_json_object_key_at(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object())
        return nullptr;

    const auto& obj = value->as_object();
    if (index < 0 || static_cast<size_t>(index) >= obj.size())
        return nullptr;

    // Iterate to the index (std::map is ordered)
    auto it = obj.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(index));

    // Copy key to buffer
    const std::string& key = it->first;
    if (key.length() >= sizeof(json_key_buffer))
        return nullptr;
    std::memcpy(json_key_buffer, key.data(), key.length());
    json_key_buffer[key.length()] = '\0';
    return json_key_buffer;
}

/**
 * @brief Get object value at index (by key order).
 *
 * @param handle Handle to the JsonValue (object)
 * @param index Value index (0-based)
 * @return Handle to the value, or -1 if out of bounds
 */
TML_EXPORT int64_t tml_json_object_value_at(int64_t handle, int64_t index) {
    auto* value = get_json_value(handle);
    if (!value || !value->is_object())
        return -1;

    const auto& obj = value->as_object();
    if (index < 0 || static_cast<size_t>(index) >= obj.size())
        return -1;

    // Iterate to the index
    auto it = obj.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(index));

    // Clone the value
    if (g_json_stats.enabled)
        g_json_stats.clone_count++;
    ScopedTimer timer(g_json_stats.clone_time_ns, g_json_stats.enabled);

    return alloc_json_handle(it->second.clone());
}
