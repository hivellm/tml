//! # JSON Memory Allocator
//!
//! Arena-based memory allocator for high-performance JSON parsing.
//! Reduces allocation overhead by pooling memory for JSON values.
//!
//! ## Features
//!
//! - **Arena allocation**: Bulk memory allocation with O(1) individual allocations
//! - **Small string optimization**: Pooled storage for short strings (< 32 bytes)
//! - **String interning**: Deduplicated storage for common JSON keys
//! - **Bump allocator**: Fast pointer-bump allocation within blocks
//!
//! ## Usage
//!
//! ```cpp
//! JsonArena arena;
//! auto* str = arena.alloc_string("hello");
//! auto* arr = arena.alloc_array();
//! // All memory freed when arena is destroyed
//! ```

#pragma once

#include "common.hpp"

#include "json/json_value.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tml::json {

// ============================================================================
// Arena Block
// ============================================================================

/// A single memory block in the arena
struct ArenaBlock {
    static constexpr size_t DEFAULT_SIZE = 64 * 1024; // 64KB blocks

    std::unique_ptr<char[]> data;
    size_t size;
    size_t used;

    explicit ArenaBlock(size_t block_size = DEFAULT_SIZE)
        : data(std::make_unique<char[]>(block_size)), size(block_size), used(0) {}

    /// Returns pointer to allocated memory, or nullptr if not enough space
    [[nodiscard]] auto alloc(size_t bytes, size_t alignment = 8) -> void* {
        // Align the current position
        size_t aligned_used = (used + alignment - 1) & ~(alignment - 1);
        if (aligned_used + bytes > size) {
            return nullptr;
        }
        void* ptr = data.get() + aligned_used;
        used = aligned_used + bytes;
        return ptr;
    }

    /// Returns available space in this block
    [[nodiscard]] auto available() const -> size_t {
        return size - used;
    }

    /// Reset the block for reuse (does not free memory)
    void reset() {
        used = 0;
    }
};

// ============================================================================
// String Pool Entry
// ============================================================================

/// An interned string stored in the arena
struct InternedString {
    const char* data; // Pointer into arena
    size_t length;

    [[nodiscard]] auto view() const -> std::string_view {
        return {data, length};
    }

    [[nodiscard]] auto str() const -> std::string {
        return std::string(data, length);
    }

    bool operator==(const InternedString& other) const {
        return length == other.length && std::memcmp(data, other.data, length) == 0;
    }
};

// ============================================================================
// String Intern Table
// ============================================================================

/// Hash function for string_view (for intern table)
struct StringViewHash {
    size_t operator()(std::string_view sv) const {
        // FNV-1a hash
        size_t hash = 14695981039346656037ULL;
        for (char c : sv) {
            hash ^= static_cast<size_t>(static_cast<unsigned char>(c));
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

/// String intern table for deduplication
class StringInternTable {
public:
    /// Maximum length of strings to intern (longer strings bypass interning)
    static constexpr size_t MAX_INTERN_LENGTH = 64;

    /// Common JSON keys that are pre-interned
    static constexpr const char* COMMON_KEYS[] = {
        "type",   "id",     "name",    "value",   "data",  "error",  "result",
        "method", "params", "jsonrpc", "message", "code",  "status", "version",
        "true",   "false",  "null",    "key",     "index", "count",  "items",
        "text",   "title",  "content", "url",     "path",  "size",   "length",
    };

    StringInternTable() = default;

    /// Look up or insert a string
    [[nodiscard]] auto intern(std::string_view str, ArenaBlock& arena) -> const InternedString* {
        if (str.length() > MAX_INTERN_LENGTH) {
            return nullptr; // Too long to intern
        }

        // Check if already interned
        auto it = table_.find(str);
        if (it != table_.end()) {
            return &it->second;
        }

        // Allocate in arena and intern
        char* data = static_cast<char*>(arena.alloc(str.length() + 1));
        if (!data) {
            return nullptr;
        }
        std::memcpy(data, str.data(), str.length());
        data[str.length()] = '\0';

        InternedString interned{data, str.length()};
        auto [inserted_it, _] = table_.emplace(std::string_view(data, str.length()), interned);
        return &inserted_it->second;
    }

    /// Pre-intern common keys
    void intern_common_keys(ArenaBlock& arena) {
        for (const char* key : COMMON_KEYS) {
            (void)intern(key, arena);
        }
    }

    /// Get count of interned strings
    [[nodiscard]] auto count() const -> size_t {
        return table_.size();
    }

    /// Clear the intern table (does not free arena memory)
    void clear() {
        table_.clear();
    }

private:
    std::unordered_map<std::string_view, InternedString, StringViewHash> table_;
};

// ============================================================================
// JSON Arena
// ============================================================================

/// Arena allocator for JSON values
///
/// Provides fast allocation for JSON parsing by using a bump allocator
/// within large memory blocks. All memory is freed when the arena is destroyed.
///
/// ## Benefits
///
/// 1. **Reduced allocation overhead**: Single large allocation vs many small ones
/// 2. **Cache locality**: Related values are stored together
/// 3. **Fast cleanup**: Single deallocation frees all memory
/// 4. **String interning**: Deduplicates common keys
class JsonArena {
public:
    /// Default block size (64KB)
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024;

    /// Small string threshold (strings <= this use small string pool)
    static constexpr size_t SMALL_STRING_THRESHOLD = 32;

    explicit JsonArena(size_t initial_block_size = DEFAULT_BLOCK_SIZE)
        : block_size_(initial_block_size) {
        // Allocate initial block
        blocks_.emplace_back(block_size_);
        current_block_ = &blocks_.back();

        // Pre-intern common keys
        intern_table_.intern_common_keys(*current_block_);
    }

    /// Allocate raw bytes with alignment
    [[nodiscard]] auto alloc(size_t bytes, size_t alignment = 8) -> void* {
        void* ptr = current_block_->alloc(bytes, alignment);
        if (ptr) {
            total_allocated_ += bytes;
            return ptr;
        }

        // Current block full, allocate new block
        size_t new_block_size = std::max(block_size_, bytes + alignment);
        blocks_.emplace_back(new_block_size);
        current_block_ = &blocks_.back();
        block_count_++;

        ptr = current_block_->alloc(bytes, alignment);
        if (ptr) {
            total_allocated_ += bytes;
        }
        return ptr;
    }

    /// Allocate a string (may be interned for common keys)
    [[nodiscard]] auto alloc_string(std::string_view str) -> std::string_view {
        // Try to intern short strings
        if (str.length() <= StringInternTable::MAX_INTERN_LENGTH) {
            if (auto* interned = intern_table_.intern(str, *current_block_)) {
                return interned->view();
            }
        }

        // Allocate without interning
        char* data = static_cast<char*>(alloc(str.length() + 1, 1));
        if (!data) {
            return {}; // Allocation failed
        }
        std::memcpy(data, str.data(), str.length());
        data[str.length()] = '\0';
        return {data, str.length()};
    }

    /// Intern a string (returns existing if already interned)
    [[nodiscard]] auto intern_string(std::string_view str) -> std::string_view {
        if (auto* interned = intern_table_.intern(str, *current_block_)) {
            return interned->view();
        }
        // Fall back to regular allocation if interning fails
        return alloc_string(str);
    }

    /// Reset the arena (reuse blocks without freeing)
    void reset() {
        for (auto& block : blocks_) {
            block.reset();
        }
        current_block_ = &blocks_.front();
        intern_table_.clear();

        // Re-intern common keys
        intern_table_.intern_common_keys(*current_block_);

        total_allocated_ = 0;
    }

    /// Get total memory allocated (across all blocks)
    [[nodiscard]] auto total_capacity() const -> size_t {
        size_t total = 0;
        for (const auto& block : blocks_) {
            total += block.size;
        }
        return total;
    }

    /// Get total bytes used
    [[nodiscard]] auto total_used() const -> size_t {
        return total_allocated_;
    }

    /// Get number of blocks
    [[nodiscard]] auto block_count() const -> size_t {
        return block_count_;
    }

    /// Get number of interned strings
    [[nodiscard]] auto interned_count() const -> size_t {
        return intern_table_.count();
    }

private:
    std::vector<ArenaBlock> blocks_;
    ArenaBlock* current_block_ = nullptr;
    size_t block_size_;
    size_t block_count_ = 1;
    size_t total_allocated_ = 0;
    StringInternTable intern_table_;
};

// ============================================================================
// JSON Document (Arena-backed)
// ============================================================================

/// A JSON document with its own arena allocator
///
/// `JsonDocument` owns both the parsed JSON value and the arena used to
/// allocate its strings. This provides optimal memory locality and fast
/// cleanup.
///
/// ## Example
///
/// ```cpp
/// auto doc = JsonDocument::parse(json_string);
/// if (doc) {
///     const auto& root = doc->root();
///     // ... use root
/// }
/// // All memory freed when doc goes out of scope
/// ```
class JsonDocument {
public:
    JsonDocument() = default;

    /// Create a document with a pre-allocated arena
    explicit JsonDocument(size_t arena_size) : arena_(arena_size) {}

    /// Get the root value
    [[nodiscard]] auto root() -> JsonValue& {
        return root_;
    }

    [[nodiscard]] auto root() const -> const JsonValue& {
        return root_;
    }

    /// Set the root value
    void set_root(JsonValue value) {
        root_ = std::move(value);
    }

    /// Get the arena
    [[nodiscard]] auto arena() -> JsonArena& {
        return arena_;
    }

    [[nodiscard]] auto arena() const -> const JsonArena& {
        return arena_;
    }

    /// Parse JSON into a document
    [[nodiscard]] static auto parse(std::string_view input) -> std::optional<JsonDocument>;

    /// Parse JSON with a specific arena size
    [[nodiscard]] static auto parse(std::string_view input, size_t arena_size)
        -> std::optional<JsonDocument>;

private:
    JsonArena arena_;
    JsonValue root_;
};

// ============================================================================
// Copy-on-Write String
// ============================================================================

/// A copy-on-write string optimized for JSON keys and values
///
/// `CowString` stores strings in one of three ways:
/// 1. Small string optimization (SSO): Strings <= 23 bytes stored inline
/// 2. Shared: Reference-counted shared storage
/// 3. View: Non-owning view into external storage (arena or input)
///
/// Copying a `CowString` is O(1) - it just increments the reference count.
/// Mutation triggers a copy only if the string is shared.
class CowString {
public:
    /// Maximum inline string length (SSO)
    static constexpr size_t SSO_CAPACITY = 23;

    /// Default constructor creates empty string
    CowString() : size_(0), mode_(Mode::SSO) {
        sso_[0] = '\0';
    }

    /// Construct from string_view (copies data)
    explicit CowString(std::string_view str) {
        if (str.length() <= SSO_CAPACITY) {
            // Use SSO
            mode_ = Mode::SSO;
            size_ = str.length();
            std::memcpy(sso_, str.data(), str.length());
            sso_[str.length()] = '\0';
        } else {
            // Allocate shared storage
            mode_ = Mode::Shared;
            size_ = str.length();
            shared_ = new SharedData(str);
        }
    }

    /// Construct as non-owning view
    static CowString view(std::string_view str) {
        CowString result;
        result.mode_ = Mode::View;
        result.size_ = str.length();
        result.view_ = str.data();
        return result;
    }

    /// Copy constructor (O(1) for shared strings)
    CowString(const CowString& other) : size_(other.size_), mode_(other.mode_) {
        switch (mode_) {
        case Mode::SSO:
            std::memcpy(sso_, other.sso_, size_ + 1);
            break;
        case Mode::Shared:
            shared_ = other.shared_;
            ++shared_->ref_count;
            break;
        case Mode::View:
            view_ = other.view_;
            break;
        }
    }

    /// Move constructor
    CowString(CowString&& other) noexcept : size_(other.size_), mode_(other.mode_) {
        switch (mode_) {
        case Mode::SSO:
            std::memcpy(sso_, other.sso_, size_ + 1);
            break;
        case Mode::Shared:
            shared_ = other.shared_;
            other.shared_ = nullptr;
            other.mode_ = Mode::SSO;
            other.size_ = 0;
            other.sso_[0] = '\0';
            break;
        case Mode::View:
            view_ = other.view_;
            break;
        }
    }

    /// Copy assignment
    CowString& operator=(const CowString& other) {
        if (this != &other) {
            release();
            size_ = other.size_;
            mode_ = other.mode_;
            switch (mode_) {
            case Mode::SSO:
                std::memcpy(sso_, other.sso_, size_ + 1);
                break;
            case Mode::Shared:
                shared_ = other.shared_;
                ++shared_->ref_count;
                break;
            case Mode::View:
                view_ = other.view_;
                break;
            }
        }
        return *this;
    }

    /// Move assignment
    CowString& operator=(CowString&& other) noexcept {
        if (this != &other) {
            release();
            size_ = other.size_;
            mode_ = other.mode_;
            switch (mode_) {
            case Mode::SSO:
                std::memcpy(sso_, other.sso_, size_ + 1);
                break;
            case Mode::Shared:
                shared_ = other.shared_;
                other.shared_ = nullptr;
                other.mode_ = Mode::SSO;
                other.size_ = 0;
                other.sso_[0] = '\0';
                break;
            case Mode::View:
                view_ = other.view_;
                break;
            }
        }
        return *this;
    }

    /// Destructor
    ~CowString() {
        release();
    }

    /// Get string view (always valid)
    [[nodiscard]] std::string_view view() const {
        switch (mode_) {
        case Mode::SSO:
            return {sso_, size_};
        case Mode::Shared:
            return {shared_->data, size_};
        case Mode::View:
            return {view_, size_};
        }
        return {};
    }

    /// Get C string (null-terminated)
    [[nodiscard]] const char* c_str() const {
        switch (mode_) {
        case Mode::SSO:
            return sso_;
        case Mode::Shared:
            return shared_->data;
        case Mode::View:
            // View may not be null-terminated - need to convert
            return view_; // Caller must ensure null-termination
        }
        return "";
    }

    /// Get length
    [[nodiscard]] size_t length() const {
        return size_;
    }

    /// Check if empty
    [[nodiscard]] bool empty() const {
        return size_ == 0;
    }

    /// Convert to std::string
    [[nodiscard]] std::string str() const {
        return std::string(view());
    }

    /// Check if this string is shared with others
    [[nodiscard]] bool is_shared() const {
        return mode_ == Mode::Shared && shared_->ref_count > 1;
    }

    /// Make a unique copy if shared
    void make_unique() {
        if (mode_ == Mode::Shared && shared_->ref_count > 1) {
            auto new_shared = new SharedData(view());
            release();
            shared_ = new_shared;
        } else if (mode_ == Mode::View) {
            // Convert view to owned
            std::string_view v{view_, size_};
            if (size_ <= SSO_CAPACITY) {
                mode_ = Mode::SSO;
                std::memcpy(sso_, v.data(), size_);
                sso_[size_] = '\0';
            } else {
                mode_ = Mode::Shared;
                shared_ = new SharedData(v);
            }
        }
    }

    /// Comparison operators
    bool operator==(const CowString& other) const {
        return view() == other.view();
    }

    bool operator!=(const CowString& other) const {
        return !(*this == other);
    }

    bool operator<(const CowString& other) const {
        return view() < other.view();
    }

private:
    /// Storage mode
    enum class Mode : uint8_t {
        SSO,    // Small string optimization (inline)
        Shared, // Reference-counted heap allocation
        View,   // Non-owning view
    };

    /// Shared storage with reference count
    struct SharedData {
        std::atomic<size_t> ref_count{1};
        char data[1]; // Flexible array member

        SharedData() = default;

        static SharedData* create(std::string_view str) {
            void* mem = ::operator new(sizeof(SharedData) + str.length());
            auto* ptr = new (mem) SharedData();
            std::memcpy(ptr->data, str.data(), str.length());
            ptr->data[str.length()] = '\0';
            return ptr;
        }

        explicit SharedData(std::string_view str) {
            std::memcpy(data, str.data(), str.length());
            data[str.length()] = '\0';
        }
    };

    /// Release current storage
    void release() {
        if (mode_ == Mode::Shared && shared_) {
            if (--shared_->ref_count == 0) {
                delete shared_;
            }
            shared_ = nullptr;
        }
    }

    size_t size_;
    Mode mode_;

    union {
        char sso_[SSO_CAPACITY + 1]; // +1 for null terminator
        SharedData* shared_;
        const char* view_;
    };
};

} // namespace tml::json
