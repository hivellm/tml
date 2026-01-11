//! # HIR Binary Writer
//!
//! Serializes HIR modules to a compact binary format optimized for fast I/O
//! and incremental compilation caching.
//!
//! ## Binary Format Overview
//!
//! The format consists of a fixed-size header followed by variable-length module data:
//!
//! ```text
//! +------------------+-------------------+
//! | Header (16 bytes)| Module Data       |
//! +------------------+-------------------+
//!
//! Header Layout:
//!   Offset  Size  Field
//!   0       4     magic (0x52494854 = "THIR")
//!   4       2     version_major
//!   6       2     version_minor
//!   8       8     content_hash (FNV-1a)
//!
//! Module Layout:
//!   - name: string
//!   - source_path: string
//!   - structs[]: count(u32) + [HirStruct...]
//!   - enums[]: count(u32) + [HirEnum...]
//!   - behaviors[]: count(u32) + [HirBehavior...]
//!   - impls[]: count(u32) + [HirImpl...]
//!   - functions[]: count(u32) + [HirFunction...]
//!   - constants[]: count(u32) + [HirConst...]
//!   - imports[]: count(u32) + [string...]
//! ```
//!
//! ## Data Encoding
//!
//! | Type      | Encoding                              |
//! |-----------|---------------------------------------|
//! | string    | u32 length + bytes[length]            |
//! | bool      | u8 (0 = false, 1 = true)              |
//! | optional  | u8 present + value (if present)       |
//! | array     | u32 count + elements[count]           |
//! | expr      | u8 tag + u64 id + fields + type + span|
//! | pattern   | u8 tag + u64 id + fields + type + span|
//! | type      | u8 tag + string representation        |
//!
//! ## Expression Tags
//!
//! Each expression is prefixed with a tag byte identifying its kind:
//!
//! | Tag | Expression Type     |
//! |-----|---------------------|
//! | 0   | Literal             |
//! | 1   | Var (variable ref)  |
//! | 2   | Binary operation    |
//! | 3   | Unary operation     |
//! | 4   | Function call       |
//! | ... | (see ExprTag enum)  |
//!
//! ## Source Spans
//!
//! When `options.include_spans` is true, each node includes source location:
//! - start: line(u32) + column(u32) + offset(u32)
//! - end: line(u32) + column(u32) + offset(u32)
//!
//! ## Design Decisions
//!
//! 1. **Little-endian encoding**: Native on x86/ARM, no conversion needed
//! 2. **Length-prefixed strings**: O(1) skip without scanning for null
//! 3. **Tagged unions**: Single byte tag enables fast dispatch
//! 4. **Content hash in header**: Quick cache validation without full parse
//!
//! ## See Also
//!
//! - `hir_serialize.hpp` - Public API and format constants
//! - `binary_reader.cpp` - Corresponding deserialization
//! - `serialize_utils.cpp` - Content hash computation

#include "hir/hir_serialize.hpp"
#include "serializer_internal.hpp"
#include "types/type.hpp"

#include <cstring>
#include <functional>

namespace tml::hir {

// ============================================================================
// Constructor
// ============================================================================

HirBinaryWriter::HirBinaryWriter(std::ostream& out, HirSerializeOptions options)
    : out_(out), options_(options) {}

// ============================================================================
// Module Serialization
// ============================================================================

/// Serializes a complete HIR module to the output stream.
///
/// The serialization order must match the reader exactly:
/// 1. Header with content hash (computed first)
/// 2. Module metadata (name, source_path)
/// 3. Type definitions (structs, enums)
/// 4. Behaviors and implementations
/// 5. Functions and constants
/// 6. Import list
void HirBinaryWriter::write_module(const HirModule& module) {
    // Compute content hash first - needed for header
    content_hash_ = compute_hir_hash(module);

    // Write 16-byte header
    write_header(content_hash_);

    // Module identification
    write_string(module.name);
    write_string(module.source_path);

    // Type definitions (structs before enums for dependency ordering)
    write_u32(static_cast<uint32_t>(module.structs.size()));
    for (const auto& s : module.structs) {
        write_struct(s);
    }

    write_u32(static_cast<uint32_t>(module.enums.size()));
    for (const auto& e : module.enums) {
        write_enum(e);
    }

    // Interface definitions
    write_u32(static_cast<uint32_t>(module.behaviors.size()));
    for (const auto& b : module.behaviors) {
        write_behavior(b);
    }

    write_u32(static_cast<uint32_t>(module.impls.size()));
    for (const auto& impl : module.impls) {
        write_impl(impl);
    }

    // Executable code
    write_u32(static_cast<uint32_t>(module.functions.size()));
    for (const auto& f : module.functions) {
        write_function(f);
    }

    // Module-level constants
    write_u32(static_cast<uint32_t>(module.constants.size()));
    for (const auto& c : module.constants) {
        write_const(c);
    }

    // Dependencies
    write_u32(static_cast<uint32_t>(module.imports.size()));
    for (const auto& imp : module.imports) {
        write_string(imp);
    }
}

// ============================================================================
// Header Writing
// ============================================================================

/// Writes the 16-byte binary format header.
///
/// Header layout:
/// - [0..4)   magic number (identifies file type)
/// - [4..6)   major version (breaking changes)
/// - [6..8)   minor version (compatible additions)
/// - [8..16)  content hash (cache validation)
void HirBinaryWriter::write_header(ContentHash hash) {
    write_u32(HIR_MAGIC);         // "THIR" in ASCII
    write_u16(HIR_VERSION_MAJOR); // Breaking changes increment this
    write_u16(HIR_VERSION_MINOR); // Compatible additions increment this
    write_u64(hash);              // For cache invalidation
}

// ============================================================================
// Primitive Type Writing
// ============================================================================
// All values written in native (little-endian on x86/ARM) byte order.
// This is faster than converting to network order and matches most platforms.

void HirBinaryWriter::write_u8(uint8_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 1);
}

void HirBinaryWriter::write_u16(uint16_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 2);
}

void HirBinaryWriter::write_u32(uint32_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 4);
}

void HirBinaryWriter::write_u64(uint64_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 8);
}

void HirBinaryWriter::write_i64(int64_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 8);
}

void HirBinaryWriter::write_f64(double value) {
    out_.write(reinterpret_cast<const char*>(&value), 8);
}

void HirBinaryWriter::write_bool(bool value) {
    write_u8(value ? 1 : 0);
}

// ============================================================================
// String Writing
// ============================================================================

/// Writes a length-prefixed string.
///
/// Format: u32 length + bytes[length] (no null terminator)
///
/// This encoding allows:
/// - O(1) skip without scanning for null
/// - Embedded null characters in strings
/// - Known size for buffer allocation
void HirBinaryWriter::write_string(const std::string& str) {
    write_u32(static_cast<uint32_t>(str.size()));
    if (!str.empty()) {
        out_.write(str.data(), str.size());
    }
}

// ============================================================================
// Source Location Writing
// ============================================================================

/// Writes source location span for error reporting and debugging.
///
/// Each SourceSpan contains start and end locations.
/// Each location has: line (1-based), column (1-based), byte offset (0-based).
///
/// This is conditionally written based on options.include_spans.
/// Disabling spans reduces file size by ~20% but loses source mapping.
void HirBinaryWriter::write_span(const SourceSpan& span) {
    if (!options_.include_spans) {
        return; // Skip to reduce file size
    }
    // Start location (6 bytes each = 12 bytes total)
    write_u32(span.start.line);
    write_u32(span.start.column);
    write_u32(span.start.offset);
    // End location
    write_u32(span.end.line);
    write_u32(span.end.column);
    write_u32(span.end.offset);
}

// ============================================================================
// Type Writing
// ============================================================================

/// Serializes a type as tag + string representation.
///
/// Format:
/// - Tag byte: Unknown (null) or Named
/// - For Named: string with type_to_string() output
///
/// This simplified encoding uses the type's string representation
/// rather than full structural encoding. This is sufficient for
/// HIR caching where the type system context is available.
///
/// For full type fidelity without context, a more complex encoding
/// would be needed (see MIR serialization for comparison).
void HirBinaryWriter::write_type(const HirType& type) {
    if (!type) {
        // Null type - write tag only
        write_u8(static_cast<uint8_t>(detail::TypeTag::Unknown));
        return;
    }

    // Named type - write tag + string representation
    write_u8(static_cast<uint8_t>(detail::TypeTag::Named));
    write_string(types::type_to_string(type));
}

// ============================================================================
// Expression Writing
// ============================================================================

/// Serializes an expression tree recursively.
///
/// Each expression is written as:
/// 1. Tag byte (identifies expression kind)
/// 2. HIR ID (u64, unique identifier)
/// 3. Expression-specific fields
/// 4. Result type
/// 5. Source span (if enabled)
///
/// The visitor pattern handles all expression variants.
/// New expression types must be added here and in the reader.
void HirBinaryWriter::write_expr(const HirExpr& expr) {
    std::visit(
        [this](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            // ----------------------------------------------------------------
            // Literals - compile-time constant values
            // ----------------------------------------------------------------
            if constexpr (std::is_same_v<T, HirLiteralExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Literal));
                write_u64(e.id);

                // Write literal value with type tag
                std::visit(
                    [this](const auto& val) {
                        using V = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<V, int64_t>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Int64));
                            write_i64(val);
                        } else if constexpr (std::is_same_v<V, uint64_t>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::UInt64));
                            write_u64(val);
                        } else if constexpr (std::is_same_v<V, double>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Float64));
                            write_f64(val);
                        } else if constexpr (std::is_same_v<V, bool>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Bool));
                            write_bool(val);
                        } else if constexpr (std::is_same_v<V, char>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Char));
                            write_u8(static_cast<uint8_t>(val));
                        } else if constexpr (std::is_same_v<V, std::string>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::String));
                            write_string(val);
                        }
                    },
                    e.value);

                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Variable reference
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirVarExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Var));
                write_u64(e.id);
                write_string(e.name);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Binary operations (arithmetic, comparison, logical)
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Binary));
                write_u64(e.id);
                write_u8(static_cast<uint8_t>(detail::binop_to_tag(e.op)));
                write_expr_ptr(e.left);  // Recursive
                write_expr_ptr(e.right); // Recursive
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Unary operations (negation, not, ref, deref)
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Unary));
                write_u64(e.id);
                write_u8(static_cast<uint8_t>(detail::unaryop_to_tag(e.op)));
                write_expr_ptr(e.operand);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Function call
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirCallExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Call));
                write_u64(e.id);
                write_string(e.func_name);
                // Type arguments for generic functions
                write_u32(static_cast<uint32_t>(e.type_args.size()));
                for (const auto& ta : e.type_args) {
                    write_type(ta);
                }
                // Call arguments
                write_u32(static_cast<uint32_t>(e.args.size()));
                for (const auto& arg : e.args) {
                    write_expr_ptr(arg);
                }
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Method call (receiver.method(args))
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::MethodCall));
                write_u64(e.id);
                write_expr_ptr(e.receiver);
                write_string(e.method_name);
                write_u32(static_cast<uint32_t>(e.type_args.size()));
                for (const auto& ta : e.type_args) {
                    write_type(ta);
                }
                write_u32(static_cast<uint32_t>(e.args.size()));
                for (const auto& arg : e.args) {
                    write_expr_ptr(arg);
                }
                write_type(e.receiver_type); // Type of receiver for dispatch
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Field access (obj.field)
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Field));
                write_u64(e.id);
                write_expr_ptr(e.object);
                write_string(e.field_name);
                write_u32(static_cast<uint32_t>(e.field_index)); // Pre-computed index
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Index access (arr[idx])
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Index));
                write_u64(e.id);
                write_expr_ptr(e.object);
                write_expr_ptr(e.index);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Tuple construction
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Tuple));
                write_u64(e.id);
                write_u32(static_cast<uint32_t>(e.elements.size()));
                for (const auto& elem : e.elements) {
                    write_expr_ptr(elem);
                }
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Array literal
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Array));
                write_u64(e.id);
                write_u32(static_cast<uint32_t>(e.elements.size()));
                for (const auto& elem : e.elements) {
                    write_expr_ptr(elem);
                }
                write_type(e.element_type);
                write_u64(e.size);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Array repeat ([value; count])
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirArrayRepeatExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::ArrayRepeat));
                write_u64(e.id);
                write_expr_ptr(e.value);
                write_u64(e.count);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Struct construction
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirStructExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Struct));
                write_u64(e.id);
                write_string(e.struct_name);
                write_u32(static_cast<uint32_t>(e.type_args.size()));
                for (const auto& ta : e.type_args) {
                    write_type(ta);
                }
                // Named fields
                write_u32(static_cast<uint32_t>(e.fields.size()));
                for (const auto& [name, val] : e.fields) {
                    write_string(name);
                    write_expr_ptr(val);
                }
                write_optional_expr(e.base); // Functional update base
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Enum variant construction
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Enum));
                write_u64(e.id);
                write_string(e.enum_name);
                write_string(e.variant_name);
                write_u32(static_cast<uint32_t>(e.variant_index));
                write_u32(static_cast<uint32_t>(e.type_args.size()));
                for (const auto& ta : e.type_args) {
                    write_type(ta);
                }
                // Variant payload
                write_u32(static_cast<uint32_t>(e.payload.size()));
                for (const auto& p : e.payload) {
                    write_expr_ptr(p);
                }
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Block expression
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Block));
                write_u64(e.id);
                write_u32(static_cast<uint32_t>(e.stmts.size()));
                for (const auto& s : e.stmts) {
                    write_stmt_ptr(s);
                }
                write_optional_expr(e.expr); // Final expression (block value)
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // If expression
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirIfExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::If));
                write_u64(e.id);
                write_expr_ptr(e.condition);
                write_expr_ptr(e.then_branch);
                write_optional_expr(e.else_branch);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // When (match) expression
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirWhenExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::When));
                write_u64(e.id);
                write_expr_ptr(e.scrutinee);
                // Match arms
                write_u32(static_cast<uint32_t>(e.arms.size()));
                for (const auto& arm : e.arms) {
                    write_pattern_ptr(arm.pattern);
                    write_optional_expr(arm.guard);
                    write_expr_ptr(arm.body);
                    write_span(arm.span);
                }
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Loop expressions (loop, while, for)
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Loop));
                write_u64(e.id);
                // Optional label for break/continue targeting
                write_bool(e.label.has_value());
                if (e.label) {
                    write_string(*e.label);
                }
                write_expr_ptr(e.body);
                write_type(e.type);
                write_span(e.span);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::While));
                write_u64(e.id);
                write_bool(e.label.has_value());
                if (e.label) {
                    write_string(*e.label);
                }
                write_expr_ptr(e.condition);
                write_expr_ptr(e.body);
                write_type(e.type);
                write_span(e.span);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::For));
                write_u64(e.id);
                write_bool(e.label.has_value());
                if (e.label) {
                    write_string(*e.label);
                }
                write_pattern_ptr(e.pattern);
                write_expr_ptr(e.iter);
                write_expr_ptr(e.body);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Control flow (return, break, continue)
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Return));
                write_u64(e.id);
                write_optional_expr(e.value);
                write_span(e.span);
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Break));
                write_u64(e.id);
                write_bool(e.label.has_value());
                if (e.label) {
                    write_string(*e.label);
                }
                write_optional_expr(e.value);
                write_span(e.span);
            } else if constexpr (std::is_same_v<T, HirContinueExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Continue));
                write_u64(e.id);
                write_bool(e.label.has_value());
                if (e.label) {
                    write_string(*e.label);
                }
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Closure expression
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Closure));
                write_u64(e.id);
                // Parameters
                write_u32(static_cast<uint32_t>(e.params.size()));
                for (const auto& [name, type] : e.params) {
                    write_string(name);
                    write_type(type);
                }
                write_expr_ptr(e.body);
                // Captured variables
                write_u32(static_cast<uint32_t>(e.captures.size()));
                for (const auto& cap : e.captures) {
                    write_string(cap.name);
                    write_type(cap.type);
                    write_bool(cap.is_mut);
                    write_bool(cap.by_move);
                }
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Type cast
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirCastExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Cast));
                write_u64(e.id);
                write_expr_ptr(e.expr);
                write_type(e.target_type);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Try and await (async support)
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirTryExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Try));
                write_u64(e.id);
                write_expr_ptr(e.expr);
                write_type(e.type);
                write_span(e.span);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Await));
                write_u64(e.id);
                write_expr_ptr(e.expr);
                write_type(e.type);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Assignment expressions
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirAssignExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Assign));
                write_u64(e.id);
                write_expr_ptr(e.target);
                write_expr_ptr(e.value);
                write_span(e.span);
            } else if constexpr (std::is_same_v<T, HirCompoundAssignExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::CompoundAssign));
                write_u64(e.id);
                write_u8(static_cast<uint8_t>(detail::compoundop_to_tag(e.op)));
                write_expr_ptr(e.target);
                write_expr_ptr(e.value);
                write_span(e.span);
            }
            // ----------------------------------------------------------------
            // Lowlevel (unsafe) block
            // ----------------------------------------------------------------
            else if constexpr (std::is_same_v<T, HirLowlevelExpr>) {
                write_u8(static_cast<uint8_t>(detail::ExprTag::Lowlevel));
                write_u64(e.id);
                write_u32(static_cast<uint32_t>(e.stmts.size()));
                for (const auto& s : e.stmts) {
                    write_stmt_ptr(s);
                }
                write_optional_expr(e.expr);
                write_type(e.type);
                write_span(e.span);
            }
        },
        expr.kind);
}

/// Writes an optional expression pointer.
/// Format: bool present + (expr if present)
void HirBinaryWriter::write_expr_ptr(const HirExprPtr& expr) {
    if (expr) {
        write_bool(true);
        write_expr(*expr);
    } else {
        write_bool(false);
    }
}

/// Writes an optional<HirExprPtr>.
/// Format: bool present + (expr if present)
void HirBinaryWriter::write_optional_expr(const std::optional<HirExprPtr>& expr) {
    if (expr && *expr) {
        write_bool(true);
        write_expr(**expr);
    } else {
        write_bool(false);
    }
}

// ============================================================================
// Pattern Writing
// ============================================================================

/// Serializes a pattern (used in let bindings and match arms).
///
/// Patterns follow the same encoding as expressions:
/// tag byte + id + fields + type + span
void HirBinaryWriter::write_pattern(const HirPattern& pattern) {
    std::visit(
        [this](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, HirWildcardPattern>) {
                // Wildcard pattern: _
                write_u8(static_cast<uint8_t>(detail::PatternTag::Wildcard));
                write_u64(p.id);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirBindingPattern>) {
                // Binding pattern: x, mut x
                write_u8(static_cast<uint8_t>(detail::PatternTag::Binding));
                write_u64(p.id);
                write_string(p.name);
                write_bool(p.is_mut);
                write_type(p.type);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirLiteralPattern>) {
                // Literal pattern: 42, "hello", true
                write_u8(static_cast<uint8_t>(detail::PatternTag::Literal));
                write_u64(p.id);
                // Write literal value with type tag
                std::visit(
                    [this](const auto& val) {
                        using V = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<V, int64_t>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Int64));
                            write_i64(val);
                        } else if constexpr (std::is_same_v<V, uint64_t>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::UInt64));
                            write_u64(val);
                        } else if constexpr (std::is_same_v<V, double>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Float64));
                            write_f64(val);
                        } else if constexpr (std::is_same_v<V, bool>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Bool));
                            write_bool(val);
                        } else if constexpr (std::is_same_v<V, char>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::Char));
                            write_u8(static_cast<uint8_t>(val));
                        } else if constexpr (std::is_same_v<V, std::string>) {
                            write_u8(static_cast<uint8_t>(detail::LiteralTag::String));
                            write_string(val);
                        }
                    },
                    p.value);
                write_type(p.type);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirTuplePattern>) {
                // Tuple pattern: (a, b, c)
                write_u8(static_cast<uint8_t>(detail::PatternTag::Tuple));
                write_u64(p.id);
                write_u32(static_cast<uint32_t>(p.elements.size()));
                for (const auto& elem : p.elements) {
                    write_pattern_ptr(elem);
                }
                write_type(p.type);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirStructPattern>) {
                // Struct pattern: Point { x, y }
                write_u8(static_cast<uint8_t>(detail::PatternTag::Struct));
                write_u64(p.id);
                write_string(p.struct_name);
                write_u32(static_cast<uint32_t>(p.fields.size()));
                for (const auto& [name, pat] : p.fields) {
                    write_string(name);
                    write_pattern_ptr(pat);
                }
                write_bool(p.has_rest); // Point { x, .. }
                write_type(p.type);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirEnumPattern>) {
                // Enum pattern: Some(x), None
                write_u8(static_cast<uint8_t>(detail::PatternTag::Enum));
                write_u64(p.id);
                write_string(p.enum_name);
                write_string(p.variant_name);
                write_u32(static_cast<uint32_t>(p.variant_index));
                write_bool(p.payload.has_value());
                if (p.payload) {
                    write_u32(static_cast<uint32_t>(p.payload->size()));
                    for (const auto& pat : *p.payload) {
                        write_pattern_ptr(pat);
                    }
                }
                write_type(p.type);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirOrPattern>) {
                // Or pattern: a | b | c
                write_u8(static_cast<uint8_t>(detail::PatternTag::Or));
                write_u64(p.id);
                write_u32(static_cast<uint32_t>(p.alternatives.size()));
                for (const auto& alt : p.alternatives) {
                    write_pattern_ptr(alt);
                }
                write_type(p.type);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirRangePattern>) {
                // Range pattern: 1..10, 1..=10
                write_u8(static_cast<uint8_t>(detail::PatternTag::Range));
                write_u64(p.id);
                write_bool(p.start.has_value());
                if (p.start) {
                    write_i64(*p.start);
                }
                write_bool(p.end.has_value());
                if (p.end) {
                    write_i64(*p.end);
                }
                write_bool(p.inclusive);
                write_type(p.type);
                write_span(p.span);
            } else if constexpr (std::is_same_v<T, HirArrayPattern>) {
                // Array pattern: [a, b, c], [head, ..tail]
                write_u8(static_cast<uint8_t>(detail::PatternTag::Array));
                write_u64(p.id);
                write_u32(static_cast<uint32_t>(p.elements.size()));
                for (const auto& elem : p.elements) {
                    write_pattern_ptr(elem);
                }
                write_bool(p.rest.has_value());
                if (p.rest) {
                    write_pattern_ptr(*p.rest);
                }
                write_type(p.type);
                write_span(p.span);
            }
        },
        pattern.kind);
}

void HirBinaryWriter::write_pattern_ptr(const HirPatternPtr& pattern) {
    if (pattern) {
        write_bool(true);
        write_pattern(*pattern);
    } else {
        write_bool(false);
    }
}

// ============================================================================
// Statement Writing
// ============================================================================

/// Serializes a statement.
///
/// HIR has only two statement types:
/// - Let statement (variable binding)
/// - Expression statement
void HirBinaryWriter::write_stmt(const HirStmt& stmt) {
    std::visit(
        [this](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, HirLetStmt>) {
                write_u8(static_cast<uint8_t>(detail::StmtTag::Let));
                write_u64(s.id);
                write_pattern_ptr(s.pattern);
                write_type(s.type);
                write_optional_expr(s.init);
                write_span(s.span);
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                write_u8(static_cast<uint8_t>(detail::StmtTag::Expr));
                write_u64(s.id);
                write_expr_ptr(s.expr);
                write_span(s.span);
            }
        },
        stmt.kind);
}

void HirBinaryWriter::write_stmt_ptr(const HirStmtPtr& stmt) {
    if (stmt) {
        write_bool(true);
        write_stmt(*stmt);
    } else {
        write_bool(false);
    }
}

// ============================================================================
// Declaration Writing
// ============================================================================

/// Writes a function parameter.
void HirBinaryWriter::write_param(const HirParam& param) {
    write_string(param.name);
    write_type(param.type);
    write_bool(param.is_mut);
    write_span(param.span);
}

/// Writes a struct field.
void HirBinaryWriter::write_field(const HirField& field) {
    write_string(field.name);
    write_type(field.type);
    write_bool(field.is_public);
    write_span(field.span);
}

/// Writes an enum variant.
void HirBinaryWriter::write_variant(const HirVariant& variant) {
    write_string(variant.name);
    write_u32(static_cast<uint32_t>(variant.index));
    // Payload types (for variants with data)
    write_u32(static_cast<uint32_t>(variant.payload_types.size()));
    for (const auto& pt : variant.payload_types) {
        write_type(pt);
    }
    write_span(variant.span);
}

/// Writes a function definition.
void HirBinaryWriter::write_function(const HirFunction& func) {
    write_u64(func.id);
    write_string(func.name);
    write_string(func.mangled_name);

    // Parameters
    write_u32(static_cast<uint32_t>(func.params.size()));
    for (const auto& p : func.params) {
        write_param(p);
    }

    write_type(func.return_type);
    write_optional_expr(func.body); // Body is optional (extern functions)

    // Function attributes
    write_bool(func.is_public);
    write_bool(func.is_async);
    write_bool(func.is_extern);

    // External linkage info
    write_bool(func.extern_abi.has_value());
    if (func.extern_abi) {
        write_string(*func.extern_abi);
    }

    // Custom attributes
    write_u32(static_cast<uint32_t>(func.attributes.size()));
    for (const auto& attr : func.attributes) {
        write_string(attr);
    }

    write_span(func.span);
}

/// Writes a struct definition.
void HirBinaryWriter::write_struct(const HirStruct& s) {
    write_u64(s.id);
    write_string(s.name);
    write_string(s.mangled_name);

    write_u32(static_cast<uint32_t>(s.fields.size()));
    for (const auto& f : s.fields) {
        write_field(f);
    }

    write_bool(s.is_public);
    write_span(s.span);
}

/// Writes an enum definition.
void HirBinaryWriter::write_enum(const HirEnum& e) {
    write_u64(e.id);
    write_string(e.name);
    write_string(e.mangled_name);

    write_u32(static_cast<uint32_t>(e.variants.size()));
    for (const auto& v : e.variants) {
        write_variant(v);
    }

    write_bool(e.is_public);
    write_span(e.span);
}

/// Writes a behavior method signature.
void HirBinaryWriter::write_behavior_method(const HirBehaviorMethod& method) {
    write_string(method.name);

    write_u32(static_cast<uint32_t>(method.params.size()));
    for (const auto& p : method.params) {
        write_param(p);
    }

    write_type(method.return_type);
    write_bool(method.has_default_impl);
    write_optional_expr(method.default_body);
    write_span(method.span);
}

/// Writes a behavior (trait) definition.
void HirBinaryWriter::write_behavior(const HirBehavior& b) {
    write_u64(b.id);
    write_string(b.name);

    // Methods
    write_u32(static_cast<uint32_t>(b.methods.size()));
    for (const auto& m : b.methods) {
        write_behavior_method(m);
    }

    // Super behaviors (inheritance)
    write_u32(static_cast<uint32_t>(b.super_behaviors.size()));
    for (const auto& sb : b.super_behaviors) {
        write_string(sb);
    }

    write_bool(b.is_public);
    write_span(b.span);
}

/// Writes an impl block.
void HirBinaryWriter::write_impl(const HirImpl& impl) {
    write_u64(impl.id);

    // Behavior being implemented (if any)
    write_bool(impl.behavior_name.has_value());
    if (impl.behavior_name) {
        write_string(*impl.behavior_name);
    }

    write_string(impl.type_name);
    write_type(impl.self_type);

    // Methods in the impl
    write_u32(static_cast<uint32_t>(impl.methods.size()));
    for (const auto& m : impl.methods) {
        write_function(m);
    }

    write_span(impl.span);
}

/// Writes a constant definition.
void HirBinaryWriter::write_const(const HirConst& c) {
    write_u64(c.id);
    write_string(c.name);
    write_type(c.type);
    write_expr_ptr(c.value);
    write_bool(c.is_public);
    write_span(c.span);
}

} // namespace tml::hir
