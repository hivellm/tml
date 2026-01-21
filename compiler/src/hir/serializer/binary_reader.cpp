//! # HIR Binary Reader
//!
//! This file reads HIR modules from the compact binary format produced by
//! `HirBinaryWriter`. It performs the inverse transformation, reconstructing
//! the full HIR tree from serialized bytes.
//!
//! ## Reading Process
//!
//! ```text
//! 1. verify_header()    - Check magic, version, extract content hash
//! 2. read module meta   - Name, source path
//! 3. read type defs     - Structs, enums (in dependency order)
//! 4. read interfaces    - Behaviors, implementations
//! 5. read functions     - With body expressions
//! 6. read constants     - Module-level constants
//! 7. read imports       - Dependency list
//! ```
//!
//! ## Error Handling
//!
//! The reader uses a **soft error model**:
//!
//! - Errors set `has_error_` flag and `error_` message
//! - Reading continues to collect as much data as possible
//! - Caller should check `has_error()` after `read_module()`
//!
//! This design allows partial recovery and better error reporting.
//!
//! ## Type Reconstruction
//!
//! Types are stored as strings (e.g., "I32", "Point") and reconstructed:
//!
//! | Stored String | Reconstruction               |
//! |---------------|------------------------------|
//! | "I32"         | `types::make_i32()`          |
//! | "Bool"        | `types::make_bool()`         |
//! | "()"          | `types::make_unit()`         |
//! | "MyStruct"    | `NamedType{name, "", {}}`    |
//!
//! For full type fidelity, the type registry from the original compilation
//! would be needed. This simplified approach works for most caching scenarios.
//!
//! ## Example
//!
//! ```cpp
//! std::ifstream file("module.hir", std::ios::binary);
//! HirBinaryReader reader(file);
//! HirModule module = reader.read_module();
//!
//! if (reader.has_error()) {
//!     std::cerr << "Error: " << reader.error_message() << "\n";
//! }
//!
//! // Check cache validity
//! ContentHash stored_hash = reader.content_hash();
//! ```
//!
//! ## See Also
//!
//! - `binary_writer.cpp` - Writes the format this reads
//! - `hir_serialize.hpp` - Public API
//! - `serializer_internal.hpp` - Tag definitions

#include "hir/hir_serialize.hpp"
#include "serializer_internal.hpp"
#include "types/type.hpp"

namespace tml::hir {

// ============================================================================
// Constructor
// ============================================================================

HirBinaryReader::HirBinaryReader(std::istream& in) : in_(in) {}

// ============================================================================
// Module Reading
// ============================================================================

/// Reads a complete HIR module from the input stream.
///
/// The read order must exactly match the write order in binary_writer.cpp:
/// 1. Header (magic, version, hash)
/// 2. Module metadata (name, source_path)
/// 3. Structs, enums (type definitions)
/// 4. Behaviors, impls (interfaces)
/// 5. Functions, constants
/// 6. Imports
///
/// On error, returns a partial module. Check has_error() afterward.
auto HirBinaryReader::read_module() -> HirModule {
    HirModule module;

    // Verify file format before reading data
    if (!verify_header()) {
        return module;
    }

    // Read module identification
    module.name = read_string();
    module.source_path = read_string();

    // Read structs
    uint32_t struct_count = read_u32();
    module.structs.reserve(struct_count);
    for (uint32_t i = 0; i < struct_count && !has_error_; ++i) {
        module.structs.push_back(read_struct());
    }

    // Read enums
    uint32_t enum_count = read_u32();
    module.enums.reserve(enum_count);
    for (uint32_t i = 0; i < enum_count && !has_error_; ++i) {
        module.enums.push_back(read_enum());
    }

    // Read behaviors
    uint32_t behavior_count = read_u32();
    module.behaviors.reserve(behavior_count);
    for (uint32_t i = 0; i < behavior_count && !has_error_; ++i) {
        module.behaviors.push_back(read_behavior());
    }

    // Read impls
    uint32_t impl_count = read_u32();
    module.impls.reserve(impl_count);
    for (uint32_t i = 0; i < impl_count && !has_error_; ++i) {
        module.impls.push_back(read_impl());
    }

    // Read functions
    uint32_t func_count = read_u32();
    module.functions.reserve(func_count);
    for (uint32_t i = 0; i < func_count && !has_error_; ++i) {
        module.functions.push_back(read_function());
    }

    // Read constants
    uint32_t const_count = read_u32();
    module.constants.reserve(const_count);
    for (uint32_t i = 0; i < const_count && !has_error_; ++i) {
        module.constants.push_back(read_const());
    }

    // Read imports
    uint32_t import_count = read_u32();
    module.imports.reserve(import_count);
    for (uint32_t i = 0; i < import_count && !has_error_; ++i) {
        module.imports.push_back(read_string());
    }

    return module;
}

// ============================================================================
// Header Verification
// ============================================================================

/// Verifies the binary file header and extracts metadata.
///
/// Header layout (16 bytes total):
/// - [0..4)   Magic number: 0x52494854 ("THIR" in ASCII)
/// - [4..6)   Major version: Breaking changes increment this
/// - [6..8)   Minor version: Compatible additions increment this
/// - [8..16)  Content hash: FNV-1a hash of module content
///
/// Version compatibility:
/// - Different major version: Error (incompatible format)
/// - Higher minor version: Warning (may miss new features)
/// - Same version: Full compatibility
auto HirBinaryReader::verify_header() -> bool {
    // Check magic number to identify file type
    uint32_t magic = read_u32();
    if (magic != HIR_MAGIC) {
        set_error("Invalid HIR file magic number");
        return false;
    }

    // Check version compatibility
    uint16_t major = read_u16();
    uint16_t minor = read_u16();
    (void)minor; // Minor version differences are OK

    if (major != HIR_VERSION_MAJOR) {
        set_error("Incompatible HIR version: " + std::to_string(major) + "." +
                  std::to_string(minor));
        return false;
    }

    // Extract content hash for cache validation
    content_hash_ = read_u64();
    return true;
}

void HirBinaryReader::set_error(const std::string& msg) {
    has_error_ = true;
    error_ = msg;
}

// ============================================================================
// Primitive Type Reading
// ============================================================================
// These functions read raw bytes from the stream in native (little-endian)
// byte order. They must match the corresponding write_* functions exactly.

auto HirBinaryReader::read_u8() -> uint8_t {
    uint8_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 1);
    return value;
}

auto HirBinaryReader::read_u16() -> uint16_t {
    uint16_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 2);
    return value;
}

auto HirBinaryReader::read_u32() -> uint32_t {
    uint32_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 4);
    return value;
}

auto HirBinaryReader::read_u64() -> uint64_t {
    uint64_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 8);
    return value;
}

auto HirBinaryReader::read_i64() -> int64_t {
    int64_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 8);
    return value;
}

auto HirBinaryReader::read_f64() -> double {
    double value = 0;
    in_.read(reinterpret_cast<char*>(&value), 8);
    return value;
}

auto HirBinaryReader::read_bool() -> bool {
    return read_u8() != 0;
}

// ============================================================================
// String and Span Reading
// ============================================================================

/// Reads a length-prefixed string.
/// Format: u32 length + bytes[length] (no null terminator)
auto HirBinaryReader::read_string() -> std::string {
    uint32_t len = read_u32();
    if (len == 0) {
        return "";
    }
    std::string str(len, '\0');
    in_.read(str.data(), len);
    return str;
}

/// Reads source location span (12 bytes total when enabled).
/// Each location contains: line (1-based), column (1-based), offset (0-based)
auto HirBinaryReader::read_span() -> SourceSpan {
    SourceSpan span;
    // Read start location (line, column, byte offset)
    span.start.line = read_u32();
    span.start.column = read_u32();
    span.start.offset = read_u32();
    // Read end location
    span.end.line = read_u32();
    span.end.column = read_u32();
    span.end.offset = read_u32();
    return span;
}

// ============================================================================
// Type Reconstruction
// ============================================================================

/// Reads and reconstructs a type from its string representation.
///
/// The type tag indicates whether this is a null type or a named type.
/// For named types, we parse the string to reconstruct primitive types
/// directly, and create NamedType placeholders for user-defined types.
///
/// This simplified approach works for caching but doesn't preserve full
/// type information (generics, references, etc.).
auto HirBinaryReader::read_type() -> HirType {
    auto tag = static_cast<detail::TypeTag>(read_u8());
    if (tag == detail::TypeTag::Unknown) {
        return nullptr;
    }

    std::string type_str = read_string();
    if (type_str.empty()) {
        return nullptr;
    }

    // Parse simple types - more complex types require type system context
    // For primitive types, we can reconstruct them directly
    if (type_str == "I32")
        return types::make_i32();
    if (type_str == "I64")
        return types::make_i64();
    if (type_str == "F64")
        return types::make_f64();
    if (type_str == "Bool")
        return types::make_bool();
    if (type_str == "Str")
        return types::make_str();
    if (type_str == "()" || type_str == "Unit")
        return types::make_unit();
    if (type_str == "!")
        return types::make_never();

    // For complex types (structs, generics, etc.), create a NamedType
    // This is a simplified version - full type fidelity requires type registry
    auto type = std::make_shared<types::Type>();
    type->kind = types::NamedType{type_str, "", {}};
    return type;
}

// ============================================================================
// Expression Reading
// ============================================================================

/// Reads and reconstructs an expression tree recursively.
///
/// Each expression is encoded as:
/// 1. Tag byte (identifies expression kind, see ExprTag enum)
/// 2. HIR ID (u64, unique identifier preserved from original)
/// 3. Expression-specific fields (varies by type)
/// 4. Result type
/// 5. Source span (if options.include_spans was true during write)
///
/// The tag-based dispatch matches the corresponding writer logic.
auto HirBinaryReader::read_expr() -> HirExprPtr {
    auto tag = static_cast<detail::ExprTag>(read_u8());
    HirId id = read_u64();

    switch (tag) {
    case detail::ExprTag::Literal: {
        auto lit_tag = static_cast<detail::LiteralTag>(read_u8());
        std::variant<int64_t, uint64_t, double, bool, char, std::string> value;

        switch (lit_tag) {
        case detail::LiteralTag::Int64:
            value = read_i64();
            break;
        case detail::LiteralTag::UInt64:
            value = read_u64();
            break;
        case detail::LiteralTag::Float64:
            value = read_f64();
            break;
        case detail::LiteralTag::Bool:
            value = read_bool();
            break;
        case detail::LiteralTag::Char:
            value = static_cast<char>(read_u8());
            break;
        case detail::LiteralTag::String:
            value = read_string();
            break;
        }

        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirLiteralExpr{id, value, type, span};
        return expr;
    }

    case detail::ExprTag::Var: {
        std::string name = read_string();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirVarExpr{id, name, type, span};
        return expr;
    }

    case detail::ExprTag::Binary: {
        auto op = detail::tag_to_binop(static_cast<detail::BinOpTag>(read_u8()));
        auto left = read_expr();
        auto right = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirBinaryExpr{id, op, std::move(left), std::move(right), type, span};
        return expr;
    }

    case detail::ExprTag::Unary: {
        auto op = detail::tag_to_unaryop(static_cast<detail::UnaryOpTag>(read_u8()));
        auto operand = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirUnaryExpr{id, op, std::move(operand), type, span};
        return expr;
    }

    case detail::ExprTag::Call: {
        std::string func_name = read_string();
        uint32_t type_arg_count = read_u32();
        std::vector<HirType> type_args;
        for (uint32_t i = 0; i < type_arg_count; ++i) {
            type_args.push_back(read_type());
        }
        uint32_t arg_count = read_u32();
        std::vector<HirExprPtr> args;
        for (uint32_t i = 0; i < arg_count; ++i) {
            args.push_back(read_expr());
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirCallExpr{id, func_name, std::move(type_args), std::move(args), type, span};
        return expr;
    }

    case detail::ExprTag::MethodCall: {
        auto receiver = read_expr();
        std::string method_name = read_string();
        uint32_t type_arg_count = read_u32();
        std::vector<HirType> type_args;
        for (uint32_t i = 0; i < type_arg_count; ++i) {
            type_args.push_back(read_type());
        }
        uint32_t arg_count = read_u32();
        std::vector<HirExprPtr> args;
        for (uint32_t i = 0; i < arg_count; ++i) {
            args.push_back(read_expr());
        }
        HirType receiver_type = read_type();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirMethodCallExpr{id,
                                       std::move(receiver),
                                       method_name,
                                       std::move(type_args),
                                       std::move(args),
                                       receiver_type,
                                       type,
                                       span};
        return expr;
    }

    case detail::ExprTag::Field: {
        auto object = read_expr();
        std::string field_name = read_string();
        int field_index = static_cast<int>(read_u32());
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirFieldExpr{id, std::move(object), field_name, field_index, type, span};
        return expr;
    }

    case detail::ExprTag::Index: {
        auto object = read_expr();
        auto index = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirIndexExpr{id, std::move(object), std::move(index), type, span};
        return expr;
    }

    case detail::ExprTag::Tuple: {
        uint32_t count = read_u32();
        std::vector<HirExprPtr> elements;
        for (uint32_t i = 0; i < count; ++i) {
            elements.push_back(read_expr());
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirTupleExpr{id, std::move(elements), type, span};
        return expr;
    }

    case detail::ExprTag::Array: {
        uint32_t count = read_u32();
        std::vector<HirExprPtr> elements;
        for (uint32_t i = 0; i < count; ++i) {
            elements.push_back(read_expr());
        }
        HirType element_type = read_type();
        size_t size = read_u64();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirArrayExpr{id, std::move(elements), element_type, size, type, span};
        return expr;
    }

    case detail::ExprTag::ArrayRepeat: {
        auto value = read_expr();
        size_t count = read_u64();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirArrayRepeatExpr{id, std::move(value), count, type, span};
        return expr;
    }

    case detail::ExprTag::Struct: {
        std::string struct_name = read_string();
        uint32_t type_arg_count = read_u32();
        std::vector<HirType> type_args;
        for (uint32_t i = 0; i < type_arg_count; ++i) {
            type_args.push_back(read_type());
        }
        uint32_t field_count = read_u32();
        std::vector<std::pair<std::string, HirExprPtr>> fields;
        for (uint32_t i = 0; i < field_count; ++i) {
            std::string name = read_string();
            auto val = read_expr();
            fields.emplace_back(name, std::move(val));
        }
        auto base = read_optional_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirStructExpr{
            id, struct_name, std::move(type_args), std::move(fields), std::move(base), type, span};
        return expr;
    }

    case detail::ExprTag::Enum: {
        std::string enum_name = read_string();
        std::string variant_name = read_string();
        int variant_index = static_cast<int>(read_u32());
        uint32_t type_arg_count = read_u32();
        std::vector<HirType> type_args;
        for (uint32_t i = 0; i < type_arg_count; ++i) {
            type_args.push_back(read_type());
        }
        uint32_t payload_count = read_u32();
        std::vector<HirExprPtr> payload;
        for (uint32_t i = 0; i < payload_count; ++i) {
            payload.push_back(read_expr());
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirEnumExpr{
            id,   enum_name, variant_name, variant_index, std::move(type_args), std::move(payload),
            type, span};
        return expr;
    }

    case detail::ExprTag::Block: {
        uint32_t stmt_count = read_u32();
        std::vector<HirStmtPtr> stmts;
        for (uint32_t i = 0; i < stmt_count; ++i) {
            stmts.push_back(read_stmt());
        }
        auto trailing = read_optional_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirBlockExpr{id, std::move(stmts), std::move(trailing), type, span};
        return expr;
    }

    case detail::ExprTag::If: {
        auto condition = read_expr();
        auto then_branch = read_expr();
        auto else_branch = read_optional_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirIfExpr{
            id, std::move(condition), std::move(then_branch), std::move(else_branch), type, span};
        return expr;
    }

    case detail::ExprTag::When: {
        auto scrutinee = read_expr();
        uint32_t arm_count = read_u32();
        std::vector<HirWhenArm> arms;
        for (uint32_t i = 0; i < arm_count; ++i) {
            auto pattern = read_pattern();
            auto guard = read_optional_expr();
            auto body = read_expr();
            SourceSpan arm_span = read_span();
            arms.push_back({std::move(pattern), std::move(guard), std::move(body), arm_span});
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirWhenExpr{id, std::move(scrutinee), std::move(arms), type, span};
        return expr;
    }

    case detail::ExprTag::Loop: {
        bool has_label = read_bool();
        std::optional<std::string> label;
        if (has_label) {
            label = read_string();
        }
        // Read optional loop variable declaration
        bool has_loop_var = read_bool();
        std::optional<HirLoopVarDecl> loop_var;
        if (has_loop_var) {
            HirLoopVarDecl decl;
            decl.name = read_string();
            decl.type = read_type();
            decl.span = read_span();
            loop_var = std::move(decl);
        }
        auto condition = read_expr();
        auto body = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirLoopExpr{id, label, loop_var, std::move(condition), std::move(body), type, span};
        return expr;
    }

    case detail::ExprTag::While: {
        bool has_label = read_bool();
        std::optional<std::string> label;
        if (has_label) {
            label = read_string();
        }
        auto condition = read_expr();
        auto body = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirWhileExpr{id, label, std::move(condition), std::move(body), type, span};
        return expr;
    }

    case detail::ExprTag::For: {
        bool has_label = read_bool();
        std::optional<std::string> label;
        if (has_label) {
            label = read_string();
        }
        auto pattern = read_pattern();
        auto iter = read_expr();
        auto body = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind =
            HirForExpr{id, label, std::move(pattern), std::move(iter), std::move(body), type, span};
        return expr;
    }

    case detail::ExprTag::Return: {
        auto value = read_optional_expr();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirReturnExpr{id, std::move(value), span};
        return expr;
    }

    case detail::ExprTag::Break: {
        bool has_label = read_bool();
        std::optional<std::string> label;
        if (has_label) {
            label = read_string();
        }
        auto value = read_optional_expr();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirBreakExpr{id, label, std::move(value), span};
        return expr;
    }

    case detail::ExprTag::Continue: {
        bool has_label = read_bool();
        std::optional<std::string> label;
        if (has_label) {
            label = read_string();
        }
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirContinueExpr{id, label, span};
        return expr;
    }

    case detail::ExprTag::Closure: {
        uint32_t param_count = read_u32();
        std::vector<std::pair<std::string, HirType>> params;
        for (uint32_t i = 0; i < param_count; ++i) {
            std::string name = read_string();
            HirType param_type = read_type();
            params.emplace_back(name, param_type);
        }
        auto body = read_expr();
        uint32_t capture_count = read_u32();
        std::vector<HirCapture> captures;
        for (uint32_t i = 0; i < capture_count; ++i) {
            HirCapture cap;
            cap.name = read_string();
            cap.type = read_type();
            cap.is_mut = read_bool();
            cap.by_move = read_bool();
            captures.push_back(cap);
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind =
            HirClosureExpr{id, std::move(params), std::move(body), std::move(captures), type, span};
        return expr;
    }

    case detail::ExprTag::Cast: {
        auto inner = read_expr();
        HirType target_type = read_type();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirCastExpr{id, std::move(inner), target_type, type, span};
        return expr;
    }

    case detail::ExprTag::Try: {
        auto inner = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirTryExpr{id, std::move(inner), type, span};
        return expr;
    }

    case detail::ExprTag::Await: {
        auto inner = read_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirAwaitExpr{id, std::move(inner), type, span};
        return expr;
    }

    case detail::ExprTag::Assign: {
        auto target = read_expr();
        auto value = read_expr();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirAssignExpr{id, std::move(target), std::move(value), span};
        return expr;
    }

    case detail::ExprTag::CompoundAssign: {
        auto op = detail::tag_to_compoundop(static_cast<detail::CompoundOpTag>(read_u8()));
        auto target = read_expr();
        auto value = read_expr();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirCompoundAssignExpr{id, op, std::move(target), std::move(value), span};
        return expr;
    }

    case detail::ExprTag::Lowlevel: {
        uint32_t stmt_count = read_u32();
        std::vector<HirStmtPtr> stmts;
        for (uint32_t i = 0; i < stmt_count; ++i) {
            stmts.push_back(read_stmt());
        }
        auto trailing = read_optional_expr();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirLowlevelExpr{id, std::move(stmts), std::move(trailing), type, span};
        return expr;
    }
    }

    set_error("Unknown expression tag");
    return nullptr;
}

auto HirBinaryReader::read_optional_expr() -> std::optional<HirExprPtr> {
    if (read_bool()) {
        return read_expr();
    }
    return std::nullopt;
}

auto HirBinaryReader::read_pattern() -> HirPatternPtr {
    auto tag = static_cast<detail::PatternTag>(read_u8());
    HirId id = read_u64();

    switch (tag) {
    case detail::PatternTag::Wildcard: {
        SourceSpan span = read_span();
        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirWildcardPattern{id, span};
        return pattern;
    }

    case detail::PatternTag::Binding: {
        std::string name = read_string();
        bool is_mut = read_bool();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirBindingPattern{id, name, is_mut, type, span};
        return pattern;
    }

    case detail::PatternTag::Literal: {
        auto lit_tag = static_cast<detail::LiteralTag>(read_u8());
        std::variant<int64_t, uint64_t, double, bool, char, std::string> value;

        switch (lit_tag) {
        case detail::LiteralTag::Int64:
            value = read_i64();
            break;
        case detail::LiteralTag::UInt64:
            value = read_u64();
            break;
        case detail::LiteralTag::Float64:
            value = read_f64();
            break;
        case detail::LiteralTag::Bool:
            value = read_bool();
            break;
        case detail::LiteralTag::Char:
            value = static_cast<char>(read_u8());
            break;
        case detail::LiteralTag::String:
            value = read_string();
            break;
        }

        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirLiteralPattern{id, value, type, span};
        return pattern;
    }

    case detail::PatternTag::Tuple: {
        uint32_t count = read_u32();
        std::vector<HirPatternPtr> elements;
        for (uint32_t i = 0; i < count; ++i) {
            elements.push_back(read_pattern());
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirTuplePattern{id, std::move(elements), type, span};
        return pattern;
    }

    case detail::PatternTag::Struct: {
        std::string struct_name = read_string();
        uint32_t field_count = read_u32();
        std::vector<std::pair<std::string, HirPatternPtr>> fields;
        for (uint32_t i = 0; i < field_count; ++i) {
            std::string name = read_string();
            auto pat = read_pattern();
            fields.emplace_back(name, std::move(pat));
        }
        bool has_rest = read_bool();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirStructPattern{id, struct_name, std::move(fields), has_rest, type, span};
        return pattern;
    }

    case detail::PatternTag::Enum: {
        std::string enum_name = read_string();
        std::string variant_name = read_string();
        int variant_index = static_cast<int>(read_u32());
        bool has_payload = read_bool();
        std::optional<std::vector<HirPatternPtr>> payload;
        if (has_payload) {
            uint32_t payload_count = read_u32();
            std::vector<HirPatternPtr> pats;
            for (uint32_t i = 0; i < payload_count; ++i) {
                pats.push_back(read_pattern());
            }
            payload = std::move(pats);
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirEnumPattern{
            id, enum_name, variant_name, variant_index, std::move(payload), type, span};
        return pattern;
    }

    case detail::PatternTag::Or: {
        uint32_t count = read_u32();
        std::vector<HirPatternPtr> alternatives;
        for (uint32_t i = 0; i < count; ++i) {
            alternatives.push_back(read_pattern());
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirOrPattern{id, std::move(alternatives), type, span};
        return pattern;
    }

    case detail::PatternTag::Range: {
        bool has_start = read_bool();
        std::optional<int64_t> start;
        if (has_start) {
            start = read_i64();
        }
        bool has_end = read_bool();
        std::optional<int64_t> end;
        if (has_end) {
            end = read_i64();
        }
        bool inclusive = read_bool();
        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirRangePattern{id, start, end, inclusive, type, span};
        return pattern;
    }

    case detail::PatternTag::Array: {
        uint32_t count = read_u32();
        std::vector<HirPatternPtr> elements;
        for (uint32_t i = 0; i < count; ++i) {
            elements.push_back(read_pattern());
        }
        bool has_rest = read_bool();
        std::optional<HirPatternPtr> rest;
        if (has_rest) {
            rest = read_pattern();
        }
        HirType type = read_type();
        SourceSpan span = read_span();

        auto pattern = std::make_unique<HirPattern>();
        pattern->kind = HirArrayPattern{id, std::move(elements), std::move(rest), type, span};
        return pattern;
    }
    }

    set_error("Unknown pattern tag");
    return nullptr;
}

auto HirBinaryReader::read_stmt() -> HirStmtPtr {
    auto tag = static_cast<detail::StmtTag>(read_u8());
    HirId id = read_u64();

    switch (tag) {
    case detail::StmtTag::Let: {
        auto pattern = read_pattern();
        HirType type = read_type();
        auto init = read_optional_expr();
        SourceSpan span = read_span();

        auto stmt = std::make_unique<HirStmt>();
        stmt->kind = HirLetStmt{id, std::move(pattern), type, std::move(init), span};
        return stmt;
    }

    case detail::StmtTag::Expr: {
        auto expr = read_expr();
        SourceSpan span = read_span();

        auto stmt = std::make_unique<HirStmt>();
        stmt->kind = HirExprStmt{id, std::move(expr), span};
        return stmt;
    }
    }

    set_error("Unknown statement tag");
    return nullptr;
}

auto HirBinaryReader::read_param() -> HirParam {
    HirParam param;
    param.name = read_string();
    param.type = read_type();
    param.is_mut = read_bool();
    param.span = read_span();
    return param;
}

auto HirBinaryReader::read_field() -> HirField {
    HirField field;
    field.name = read_string();
    field.type = read_type();
    field.is_public = read_bool();
    field.span = read_span();
    return field;
}

auto HirBinaryReader::read_variant() -> HirVariant {
    HirVariant variant;
    variant.name = read_string();
    variant.index = static_cast<int>(read_u32());
    uint32_t pt_count = read_u32();
    for (uint32_t i = 0; i < pt_count; ++i) {
        variant.payload_types.push_back(read_type());
    }
    variant.span = read_span();
    return variant;
}

auto HirBinaryReader::read_function() -> HirFunction {
    HirFunction func;
    func.id = read_u64();
    func.name = read_string();
    func.mangled_name = read_string();

    uint32_t param_count = read_u32();
    for (uint32_t i = 0; i < param_count; ++i) {
        func.params.push_back(read_param());
    }

    func.return_type = read_type();
    func.body = read_optional_expr();

    func.is_public = read_bool();
    func.is_async = read_bool();
    func.is_extern = read_bool();

    if (read_bool()) {
        func.extern_abi = read_string();
    }

    uint32_t attr_count = read_u32();
    for (uint32_t i = 0; i < attr_count; ++i) {
        func.attributes.push_back(read_string());
    }

    func.span = read_span();
    return func;
}

auto HirBinaryReader::read_struct() -> HirStruct {
    HirStruct s;
    s.id = read_u64();
    s.name = read_string();
    s.mangled_name = read_string();

    uint32_t field_count = read_u32();
    for (uint32_t i = 0; i < field_count; ++i) {
        s.fields.push_back(read_field());
    }

    s.is_public = read_bool();
    s.span = read_span();
    return s;
}

auto HirBinaryReader::read_enum() -> HirEnum {
    HirEnum e;
    e.id = read_u64();
    e.name = read_string();
    e.mangled_name = read_string();

    uint32_t variant_count = read_u32();
    for (uint32_t i = 0; i < variant_count; ++i) {
        e.variants.push_back(read_variant());
    }

    e.is_public = read_bool();
    e.span = read_span();
    return e;
}

auto HirBinaryReader::read_behavior_method() -> HirBehaviorMethod {
    HirBehaviorMethod method;
    method.name = read_string();

    uint32_t param_count = read_u32();
    for (uint32_t i = 0; i < param_count; ++i) {
        method.params.push_back(read_param());
    }

    method.return_type = read_type();
    method.has_default_impl = read_bool();
    method.default_body = read_optional_expr();
    method.span = read_span();
    return method;
}

auto HirBinaryReader::read_behavior() -> HirBehavior {
    HirBehavior b;
    b.id = read_u64();
    b.name = read_string();

    uint32_t method_count = read_u32();
    for (uint32_t i = 0; i < method_count; ++i) {
        b.methods.push_back(read_behavior_method());
    }

    uint32_t super_count = read_u32();
    for (uint32_t i = 0; i < super_count; ++i) {
        b.super_behaviors.push_back(read_string());
    }

    b.is_public = read_bool();
    b.span = read_span();
    return b;
}

auto HirBinaryReader::read_impl() -> HirImpl {
    HirImpl impl;
    impl.id = read_u64();

    if (read_bool()) {
        impl.behavior_name = read_string();
    }

    impl.type_name = read_string();
    impl.self_type = read_type();

    uint32_t method_count = read_u32();
    for (uint32_t i = 0; i < method_count; ++i) {
        impl.methods.push_back(read_function());
    }

    impl.span = read_span();
    return impl;
}

auto HirBinaryReader::read_const() -> HirConst {
    HirConst c;
    c.id = read_u64();
    c.name = read_string();
    c.type = read_type();
    c.value = read_expr();
    c.is_public = read_bool();
    c.span = read_span();
    return c;
}

} // namespace tml::hir
