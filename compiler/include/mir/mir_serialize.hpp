// MIR Serialization
//
// Provides serialization and deserialization of MIR modules.
// Supports both binary and text formats.

#pragma once

#include "mir/mir.hpp"

#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace tml::mir {

// ============================================================================
// Serialization Options
// ============================================================================

struct SerializeOptions {
    bool include_comments = false; // Include debug comments
    bool compact = false;          // Minimize whitespace in text format
};

// ============================================================================
// Binary Format
// ============================================================================

// Magic number for MIR binary format: "TMLMIR" + version
constexpr uint32_t MIR_MAGIC = 0x544D4952; // "TMIR" in little-endian
constexpr uint16_t MIR_VERSION_MAJOR = 1;
constexpr uint16_t MIR_VERSION_MINOR = 0;

// Binary serialization
class MirBinaryWriter {
public:
    explicit MirBinaryWriter(std::ostream& out);

    void write_module(const Module& module);

private:
    std::ostream& out_;

    void write_header();
    void write_u8(uint8_t value);
    void write_u16(uint16_t value);
    void write_u32(uint32_t value);
    void write_u64(uint64_t value);
    void write_i64(int64_t value);
    void write_f64(double value);
    void write_string(const std::string& str);
    void write_type(const MirTypePtr& type);
    void write_value(const Value& value);
    void write_instruction(const InstructionData& inst);
    void write_terminator(const Terminator& term);
    void write_block(const BasicBlock& block);
    void write_function(const Function& func);
    void write_struct(const StructDef& s);
    void write_enum(const EnumDef& e);
};

// Binary deserialization
class MirBinaryReader {
public:
    explicit MirBinaryReader(std::istream& in);

    auto read_module() -> Module;
    auto has_error() const -> bool {
        return has_error_;
    }
    auto error_message() const -> std::string {
        return error_;
    }

private:
    std::istream& in_;
    bool has_error_ = false;
    std::string error_;

    void set_error(const std::string& msg);
    auto read_u8() -> uint8_t;
    auto read_u16() -> uint16_t;
    auto read_u32() -> uint32_t;
    auto read_u64() -> uint64_t;
    auto read_i64() -> int64_t;
    auto read_f64() -> double;
    auto read_string() -> std::string;
    auto read_type() -> MirTypePtr;
    auto read_value() -> Value;
    auto read_instruction() -> InstructionData;
    auto read_terminator() -> Terminator;
    auto read_block() -> BasicBlock;
    auto read_function() -> Function;
    auto read_struct() -> StructDef;
    auto read_enum() -> EnumDef;
    auto verify_header() -> bool;
};

// ============================================================================
// Text Format (for debugging)
// ============================================================================

// Text serialization (uses the existing pretty printer)
class MirTextWriter {
public:
    explicit MirTextWriter(std::ostream& out, SerializeOptions options = {});

    void write_module(const Module& module);

private:
    std::ostream& out_;
    SerializeOptions options_;
};

// Text deserialization (parser for MIR text format)
class MirTextReader {
public:
    explicit MirTextReader(std::istream& in);

    auto read_module() -> Module;
    auto has_error() const -> bool {
        return has_error_;
    }
    auto error_message() const -> std::string {
        return error_;
    }

private:
    std::istream& in_;
    std::string current_line_;
    size_t line_num_ = 0;
    size_t pos_ = 0;
    bool has_error_ = false;
    std::string error_;

    void set_error(const std::string& msg);
    auto next_line() -> bool;
    void skip_whitespace();
    auto peek_char() -> char;
    auto read_char() -> char;
    auto read_identifier() -> std::string;
    auto read_number() -> int64_t;
    auto read_string_literal() -> std::string;
    auto expect(char c) -> bool;
    auto expect(const std::string& s) -> bool;

    auto parse_type() -> MirTypePtr;
    auto parse_value() -> Value;
    auto parse_instruction() -> InstructionData;
    auto parse_terminator() -> Terminator;
    auto parse_block() -> BasicBlock;
    auto parse_function() -> Function;
    auto parse_struct() -> StructDef;
    auto parse_enum() -> EnumDef;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Serialize to binary
auto serialize_binary(const Module& module) -> std::vector<uint8_t>;

// Deserialize from binary
auto deserialize_binary(const std::vector<uint8_t>& data) -> Module;

// Serialize to text (uses pretty printer)
auto serialize_text(const Module& module, SerializeOptions options = {}) -> std::string;

// Deserialize from text
auto deserialize_text(const std::string& text) -> Module;

// File I/O helpers
auto write_mir_file(const Module& module, const std::string& path, bool binary = true) -> bool;
auto read_mir_file(const std::string& path) -> Module;

} // namespace tml::mir
