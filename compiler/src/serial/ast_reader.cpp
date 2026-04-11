//! # AST Binary Deserializer Implementation
//!
//! Mirrors the writer in `lib/std/src/serial/ast.tml`. Every `write_X(w, ...)`
//! function in that file has a corresponding `read_X(c)` here that consumes
//! the same bytes in the same order.

#include "serial/ast_reader.hpp"

#include "common.hpp"
#include "lexer/token.hpp"
#include "parser/ast.hpp"
#include "parser/ast_decls.hpp"
#include "parser/ast_exprs.hpp"
#include "parser/ast_oop.hpp"
#include "parser/ast_patterns.hpp"
#include "parser/ast_stmts.hpp"
#include "parser/ast_types.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tml::serial {

namespace {

using namespace tml::parser;

// ============================================================================
// Constants
// ============================================================================

constexpr uint32_t AST_MAGIC = 0x41535420; // "AST "
constexpr uint8_t AST_VERSION_MAJOR = 1;

// ============================================================================
// Cursor — primitive byte reader
// ============================================================================

struct Cursor {
    const uint8_t* data;
    size_t pos;
    size_t len;
    std::string_view file_path;

    Cursor(const std::vector<uint8_t>& v, std::string_view fp)
        : data(v.data()), pos(0), len(v.size()), file_path(fp) {}

    [[noreturn]] void fail(const char* msg) const {
        throw std::runtime_error(std::string("ast_reader: ") + msg + " at offset " +
                                 std::to_string(pos));
    }

    void need(size_t n) const {
        if (pos + n > len) {
            fail("unexpected end of input");
        }
    }

    uint8_t read_u8() {
        need(1);
        return data[pos++];
    }

    uint32_t read_u32_le() {
        need(4);
        uint32_t v = static_cast<uint32_t>(data[pos]) |
                     (static_cast<uint32_t>(data[pos + 1]) << 8) |
                     (static_cast<uint32_t>(data[pos + 2]) << 16) |
                     (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return v;
    }

    uint64_t read_u64_le() {
        need(8);
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v |= static_cast<uint64_t>(data[pos + i]) << (i * 8);
        }
        pos += 8;
        return v;
    }

    uint64_t read_varint() {
        uint64_t result = 0;
        int shift = 0;
        for (int i = 0; i < 10; i++) {
            need(1);
            uint8_t byte = data[pos++];
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) {
                return result;
            }
            shift += 7;
        }
        fail("varint too long");
    }

    bool read_bool() {
        return read_u8() != 0;
    }

    std::string read_str() {
        uint64_t n = read_varint();
        need(static_cast<size_t>(n));
        std::string s(reinterpret_cast<const char*>(data + pos), static_cast<size_t>(n));
        pos += static_cast<size_t>(n);
        return s;
    }

    // opt_str: writer encodes Nothing as varint(0) — same as empty string.
    // Treat empty as Nothing (matches the lossy round-trip the writer commits to).
    std::optional<std::string> read_opt_str() {
        std::string s = read_str();
        if (s.empty()) {
            return std::nullopt;
        }
        return s;
    }
};

// ============================================================================
// Forward declarations (mutual recursion)
// ============================================================================

SourceLocation read_source_location(Cursor& c);
SourceSpan read_source_span(Cursor& c);
Visibility read_visibility(Cursor& c);
TypePath read_type_path(Cursor& c);

TypePtr read_type(Cursor& c);
PatternPtr read_pattern(Cursor& c);
ExprPtr read_expr(Cursor& c);
StmtPtr read_stmt(Cursor& c);
DeclPtr read_decl(Cursor& c);

FuncDecl read_func_decl_body(Cursor& c);

// ============================================================================
// Source locations
// ============================================================================

SourceLocation read_source_location(Cursor& c) {
    SourceLocation loc;
    loc.file = c.file_path;
    loc.line = c.read_u32_le();
    loc.column = c.read_u32_le();
    loc.offset = c.read_u32_le();
    loc.length = c.read_u32_le();
    return loc;
}

SourceSpan read_source_span(Cursor& c) {
    SourceSpan span;
    span.start = read_source_location(c);
    span.end = read_source_location(c);
    return span;
}

Visibility read_visibility(Cursor& c) {
    uint8_t tag = c.read_u8();
    switch (tag) {
    case 0:
        return Visibility::Private;
    case 1:
        return Visibility::Public;
    case 2:
        return Visibility::PubCrate;
    default:
        c.fail("invalid Visibility tag");
    }
}

TypePath read_type_path(Cursor& c) {
    TypePath path;
    uint64_t n = c.read_varint();
    path.segments.reserve(static_cast<size_t>(n));
    for (uint64_t i = 0; i < n; i++) {
        path.segments.push_back(c.read_str());
    }
    path.span = read_source_span(c);
    return path;
}

// ============================================================================
// Literal kinds — used by literal expressions, literal patterns, range pats
// ============================================================================

// Decoded literal kind. We store enough info to:
//  - Build a LiteralExpr (which needs a lexer::Token)
//  - Build a LiteralPattern (also needs a lexer::Token)
//  - Build endpoints for RangePattern (which uses ExprPtr)
struct LitKind {
    enum Kind { Int, Float, Str, Bool, Char, Null } kind;
    int64_t i64v = 0;
    double f64v = 0.0;
    std::string strv;
    bool boolv = false;
    uint32_t charv = 0;
};

LitKind read_literal_kind(Cursor& c) {
    uint8_t tag = c.read_u8();
    LitKind lk;
    switch (tag) {
    case 0: { // LitInt: u64 (raw bit pattern of i64)
        lk.kind = LitKind::Int;
        uint64_t bits = c.read_u64_le();
        std::memcpy(&lk.i64v, &bits, sizeof(int64_t));
        return lk;
    }
    case 1: { // LitFloat: u64 raw bit pattern of f64
        lk.kind = LitKind::Float;
        uint64_t bits = c.read_u64_le();
        std::memcpy(&lk.f64v, &bits, sizeof(double));
        return lk;
    }
    case 2:
        lk.kind = LitKind::Str;
        lk.strv = c.read_str();
        return lk;
    case 3:
        lk.kind = LitKind::Bool;
        lk.boolv = c.read_bool();
        return lk;
    case 4:
        lk.kind = LitKind::Char;
        lk.charv = c.read_u32_le();
        return lk;
    case 5:
        lk.kind = LitKind::Null;
        return lk;
    default:
        c.fail("invalid LiteralKind tag");
    }
}

// Build a synthetic lexer::Token for a literal value, used by LiteralExpr /
// LiteralPattern reconstruction. The lexeme is left empty — we don't have
// the original source text, but downstream consumers should rely on the
// typed `value` variant rather than the lexeme for literal patterns/exprs.
lexer::Token literal_to_token(const LitKind& lk, SourceSpan span) {
    lexer::Token tok{};
    tok.span = span;
    tok.lexeme = std::string_view{};
    switch (lk.kind) {
    case LitKind::Int: {
        tok.kind = lexer::TokenKind::IntLiteral;
        lexer::IntValue iv;
        iv.value = static_cast<uint64_t>(lk.i64v);
        iv.base = 10;
        iv.suffix = std::string{};
        tok.value = iv;
        break;
    }
    case LitKind::Float: {
        tok.kind = lexer::TokenKind::FloatLiteral;
        lexer::FloatValue fv;
        fv.value = lk.f64v;
        fv.suffix = std::string{};
        tok.value = fv;
        break;
    }
    case LitKind::Str: {
        tok.kind = lexer::TokenKind::StringLiteral;
        lexer::StringValue sv;
        sv.value = lk.strv;
        sv.is_raw = false;
        tok.value = sv;
        break;
    }
    case LitKind::Bool: {
        tok.kind = lexer::TokenKind::BoolLiteral;
        tok.value = lk.boolv;
        break;
    }
    case LitKind::Char: {
        tok.kind = lexer::TokenKind::CharLiteral;
        lexer::CharValue cv;
        cv.value = static_cast<char32_t>(lk.charv);
        tok.value = cv;
        break;
    }
    case LitKind::Null: {
        tok.kind = lexer::TokenKind::NullLiteral;
        tok.value = std::monostate{};
        break;
    }
    }
    return tok;
}

// ============================================================================
// Operator tags
// ============================================================================

UnaryOp read_unary_op(Cursor& c) {
    uint8_t tag = c.read_u8();
    switch (tag) {
    case 0:
        return UnaryOp::Neg;
    case 1:
        return UnaryOp::Not;
    case 2:
        return UnaryOp::BitNot;
    case 3:
        return UnaryOp::Ref;
    case 4:
        return UnaryOp::RefMut;
    case 5:
        return UnaryOp::Deref;
    case 6:
        return UnaryOp::Inc;
    case 7:
        return UnaryOp::Dec;
    default:
        c.fail("invalid UnaryOp tag");
    }
}

BinaryOp read_binary_op(Cursor& c) {
    uint8_t tag = c.read_u8();
    switch (tag) {
    case 0:
        return BinaryOp::Add;
    case 1:
        return BinaryOp::Sub;
    case 2:
        return BinaryOp::Mul;
    case 3:
        return BinaryOp::Div;
    case 4:
        return BinaryOp::Mod;
    case 5:
        return BinaryOp::Eq;
    case 6:
        return BinaryOp::Ne;
    case 7:
        return BinaryOp::Lt;
    case 8:
        return BinaryOp::Gt;
    case 9:
        return BinaryOp::Le;
    case 10:
        return BinaryOp::Ge;
    case 11:
        return BinaryOp::And;
    case 12:
        return BinaryOp::Or;
    case 13:
        return BinaryOp::BitAnd;
    case 14:
        return BinaryOp::BitOr;
    case 15:
        return BinaryOp::BitXor;
    case 16:
        return BinaryOp::Shl;
    case 17:
        return BinaryOp::Shr;
    case 18:
        return BinaryOp::Assign;
    case 19:
        return BinaryOp::AddAssign;
    case 20:
        return BinaryOp::SubAssign;
    case 21:
        return BinaryOp::MulAssign;
    case 22:
        return BinaryOp::DivAssign;
    case 23:
        return BinaryOp::ModAssign;
    case 24:
        return BinaryOp::BitAndAssign;
    case 25:
        return BinaryOp::BitOrAssign;
    case 26:
        return BinaryOp::BitXorAssign;
    case 27:
        return BinaryOp::ShlAssign;
    case 28:
        return BinaryOp::ShrAssign;
    default:
        c.fail("invalid BinaryOp tag");
    }
}

// ============================================================================
// Helpers for optional / list reading
// ============================================================================

std::optional<TypePtr> read_opt_type(Cursor& c) {
    uint8_t tag = c.read_u8();
    if (tag == 0) {
        return std::nullopt;
    }
    if (tag != 1) {
        c.fail("invalid Maybe tag for type");
    }
    return read_type(c);
}

std::optional<ExprPtr> read_opt_expr(Cursor& c) {
    uint8_t tag = c.read_u8();
    if (tag == 0) {
        return std::nullopt;
    }
    if (tag != 1) {
        c.fail("invalid Maybe tag for expr");
    }
    return read_expr(c);
}

std::vector<TypePtr> read_type_list(Cursor& c) {
    uint64_t n = c.read_varint();
    std::vector<TypePtr> out;
    out.reserve(static_cast<size_t>(n));
    for (uint64_t i = 0; i < n; i++) {
        out.push_back(read_type(c));
    }
    return out;
}

// Convert a TML "type_args" payload (Maybe[List[Heap[TmlType]]]) into the
// C++ optional<GenericArgs> representation. Empty list still produces an
// empty GenericArgs because the writer distinguishes Nothing from
// Just(empty list).
std::optional<GenericArgs> read_opt_type_args(Cursor& c, SourceSpan span) {
    uint8_t tag = c.read_u8();
    if (tag == 0) {
        return std::nullopt;
    }
    if (tag != 1) {
        c.fail("invalid Maybe tag for type_args");
    }
    auto types = read_type_list(c);
    GenericArgs ga;
    ga.span = span;
    ga.args.reserve(types.size());
    for (auto& t : types) {
        SourceSpan ts = t->span;
        ga.args.push_back(GenericArg::from_type(std::move(t), ts));
    }
    return ga;
}

// ============================================================================
// Types
// ============================================================================

TypePtr read_type(Cursor& c) {
    uint8_t tag = c.read_u8();
    auto result = std::make_unique<Type>();
    switch (tag) {
    case 0: { // Named
        TypePath path = read_type_path(c);
        std::vector<TypePtr> args = read_type_list(c);
        SourceSpan span = read_source_span(c);
        NamedType nt;
        nt.path = std::move(path);
        nt.span = span;
        if (!args.empty()) {
            GenericArgs ga;
            ga.span = span;
            ga.args.reserve(args.size());
            for (auto& a : args) {
                SourceSpan asp = a->span;
                ga.args.push_back(GenericArg::from_type(std::move(a), asp));
            }
            nt.generics = std::move(ga);
        }
        result->kind = std::move(nt);
        result->span = span;
        break;
    }
    case 1: { // Ref
        TypePtr inner = read_type(c);
        bool is_mut = c.read_bool();
        SourceSpan span = read_source_span(c);
        RefType rt;
        rt.is_mut = is_mut;
        rt.inner = std::move(inner);
        rt.span = span;
        result->kind = std::move(rt);
        result->span = span;
        break;
    }
    case 2: { // Ptr
        TypePtr inner = read_type(c);
        bool is_mut = c.read_bool();
        SourceSpan span = read_source_span(c);
        PtrType pt;
        pt.is_mut = is_mut;
        pt.inner = std::move(inner);
        pt.span = span;
        result->kind = std::move(pt);
        result->span = span;
        break;
    }
    case 3: { // Array
        TypePtr inner = read_type(c);
        ExprPtr size = read_expr(c);
        SourceSpan span = read_source_span(c);
        ArrayType at;
        at.element = std::move(inner);
        at.size = std::move(size);
        at.span = span;
        result->kind = std::move(at);
        result->span = span;
        break;
    }
    case 4: { // Slice
        TypePtr inner = read_type(c);
        SourceSpan span = read_source_span(c);
        SliceType st;
        st.element = std::move(inner);
        st.span = span;
        result->kind = std::move(st);
        result->span = span;
        break;
    }
    case 5: { // Tuple
        std::vector<TypePtr> types = read_type_list(c);
        SourceSpan span = read_source_span(c);
        TupleType tt;
        tt.elements = std::move(types);
        tt.span = span;
        result->kind = std::move(tt);
        result->span = span;
        break;
    }
    case 6: { // Func
        std::vector<TypePtr> params = read_type_list(c);
        std::optional<TypePtr> ret = read_opt_type(c);
        SourceSpan span = read_source_span(c);
        FuncType ft;
        ft.params = std::move(params);
        if (ret.has_value()) {
            ft.return_type = std::move(*ret);
        } else {
            ft.return_type = nullptr;
        }
        ft.span = span;
        result->kind = std::move(ft);
        result->span = span;
        break;
    }
    case 7: { // Infer
        SourceSpan span = read_source_span(c);
        InferType it;
        it.span = span;
        result->kind = std::move(it);
        result->span = span;
        break;
    }
    case 8: { // Dyn
        uint64_t n = c.read_varint();
        std::vector<TypePath> bounds;
        bounds.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            bounds.push_back(read_type_path(c));
        }
        SourceSpan span = read_source_span(c);
        DynType dt;
        if (!bounds.empty()) {
            dt.behavior = std::move(bounds.front());
        } else {
            dt.behavior.span = span;
        }
        dt.is_mut = false;
        dt.span = span;
        result->kind = std::move(dt);
        result->span = span;
        break;
    }
    case 9: { // ImplBehavior
        uint64_t n = c.read_varint();
        std::vector<TypePath> bounds;
        bounds.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            bounds.push_back(read_type_path(c));
        }
        SourceSpan span = read_source_span(c);
        ImplBehaviorType ibt;
        if (!bounds.empty()) {
            ibt.behavior = std::move(bounds.front());
        } else {
            ibt.behavior.span = span;
        }
        ibt.span = span;
        result->kind = std::move(ibt);
        result->span = span;
        break;
    }
    default:
        c.fail("invalid Type tag");
    }
    return result;
}

// ============================================================================
// Patterns
// ============================================================================

PatternPtr read_pattern(Cursor& c) {
    uint8_t tag = c.read_u8();
    auto result = std::make_unique<Pattern>();
    switch (tag) {
    case 0: { // Wildcard
        SourceSpan span = read_source_span(c);
        WildcardPattern wp;
        wp.span = span;
        result->kind = std::move(wp);
        result->span = span;
        break;
    }
    case 1: { // Ident
        std::string name = c.read_str();
        // Sub-pattern: Maybe[Heap[TmlPattern]]
        uint8_t sub_tag = c.read_u8();
        if (sub_tag == 1) {
            // Sub-pattern present — we don't have a place to store it on
            // IdentPattern in the C++ AST, so we drop it (the writer's
            // ip.subpat field has no direct C++ equivalent).
            (void)read_pattern(c);
        } else if (sub_tag != 0) {
            c.fail("invalid Maybe tag for ident subpat");
        }
        SourceSpan span = read_source_span(c);
        IdentPattern ip;
        ip.name = std::move(name);
        ip.is_mut = false;
        ip.span = span;
        result->kind = std::move(ip);
        result->span = span;
        break;
    }
    case 2: { // Literal
        LitKind lk = read_literal_kind(c);
        SourceSpan span = read_source_span(c);
        LiteralPattern lp;
        lp.literal = literal_to_token(lk, span);
        lp.span = span;
        result->kind = std::move(lp);
        result->span = span;
        break;
    }
    case 3: { // Tuple
        uint64_t n = c.read_varint();
        std::vector<PatternPtr> pats;
        pats.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            pats.push_back(read_pattern(c));
        }
        SourceSpan span = read_source_span(c);
        TuplePattern tp;
        tp.elements = std::move(pats);
        tp.span = span;
        result->kind = std::move(tp);
        result->span = span;
        break;
    }
    case 4: { // Struct
        TypePath path = read_type_path(c);
        uint64_t n = c.read_varint();
        std::vector<std::pair<std::string, PatternPtr>> fields;
        fields.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            std::string name = c.read_str();
            PatternPtr pat = read_pattern(c);
            (void)read_source_span(c); // field span — no slot in C++
            fields.emplace_back(std::move(name), std::move(pat));
        }
        SourceSpan span = read_source_span(c);
        StructPattern sp;
        sp.path = std::move(path);
        sp.fields = std::move(fields);
        sp.has_rest = false;
        sp.span = span;
        result->kind = std::move(sp);
        result->span = span;
        break;
    }
    case 5: { // Enum
        TypePath path = read_type_path(c);
        uint64_t n = c.read_varint();
        std::vector<PatternPtr> args;
        args.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            args.push_back(read_pattern(c));
        }
        SourceSpan span = read_source_span(c);
        EnumPattern ep;
        ep.path = std::move(path);
        if (n > 0) {
            ep.payload = std::move(args);
        }
        ep.span = span;
        result->kind = std::move(ep);
        result->span = span;
        break;
    }
    case 6: { // Or
        uint64_t n = c.read_varint();
        std::vector<PatternPtr> alts;
        alts.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            alts.push_back(read_pattern(c));
        }
        SourceSpan span = read_source_span(c);
        OrPattern op;
        op.patterns = std::move(alts);
        op.span = span;
        result->kind = std::move(op);
        result->span = span;
        break;
    }
    case 7: { // Range
        LitKind start = read_literal_kind(c);
        LitKind end = read_literal_kind(c);
        bool inclusive = c.read_bool();
        SourceSpan span = read_source_span(c);
        // Wrap each literal endpoint in a LiteralExpr inside an Expr.
        auto make_lit_expr = [&](const LitKind& lk) -> ExprPtr {
            auto e = std::make_unique<Expr>();
            LiteralExpr le;
            le.token = literal_to_token(lk, span);
            le.span = span;
            e->kind = std::move(le);
            e->span = span;
            return e;
        };
        RangePattern rp;
        rp.start = make_lit_expr(start);
        rp.end = make_lit_expr(end);
        rp.inclusive = inclusive;
        rp.span = span;
        result->kind = std::move(rp);
        result->span = span;
        break;
    }
    case 8: { // Array
        uint64_t n = c.read_varint();
        std::vector<PatternPtr> elems;
        elems.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            elems.push_back(read_pattern(c));
        }
        SourceSpan span = read_source_span(c);
        ArrayPattern ap;
        ap.elements = std::move(elems);
        ap.span = span;
        result->kind = std::move(ap);
        result->span = span;
        break;
    }
    default:
        c.fail("invalid Pattern tag");
    }
    return result;
}

// ============================================================================
// Interpolated string segments
// ============================================================================

InterpolatedSegment read_interpolated_segment(Cursor& c) {
    uint8_t tag = c.read_u8();
    InterpolatedSegment seg;
    if (tag == 0) {
        seg.content = c.read_str();
    } else if (tag == 1) {
        seg.content = read_expr(c);
    } else {
        c.fail("invalid InterpolatedSegment tag");
    }
    seg.span = read_source_span(c);
    return seg;
}

// ============================================================================
// Expressions
// ============================================================================

ExprPtr read_expr(Cursor& c) {
    uint8_t tag = c.read_u8();
    auto result = std::make_unique<Expr>();
    switch (tag) {
    case 0: { // Literal
        LitKind lk = read_literal_kind(c);
        SourceSpan span = read_source_span(c);
        LiteralExpr le;
        le.token = literal_to_token(lk, span);
        le.span = span;
        result->kind = std::move(le);
        result->span = span;
        break;
    }
    case 1: { // Ident
        std::string name = c.read_str();
        SourceSpan span = read_source_span(c);
        IdentExpr ie;
        ie.name = std::move(name);
        ie.span = span;
        result->kind = std::move(ie);
        result->span = span;
        break;
    }
    case 2: { // Unary
        UnaryOp op = read_unary_op(c);
        ExprPtr operand = read_expr(c);
        SourceSpan span = read_source_span(c);
        UnaryExpr ue;
        ue.op = op;
        ue.operand = std::move(operand);
        ue.span = span;
        result->kind = std::move(ue);
        result->span = span;
        break;
    }
    case 3: { // Binary
        BinaryOp op = read_binary_op(c);
        ExprPtr left = read_expr(c);
        ExprPtr right = read_expr(c);
        SourceSpan span = read_source_span(c);
        BinaryExpr be;
        be.op = op;
        be.left = std::move(left);
        be.right = std::move(right);
        be.span = span;
        result->kind = std::move(be);
        result->span = span;
        break;
    }
    case 4: { // Call
        ExprPtr callee = read_expr(c);
        uint64_t n = c.read_varint();
        std::vector<ExprPtr> args;
        args.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            args.push_back(read_expr(c));
        }
        SourceSpan span = read_source_span(c);
        CallExpr ce;
        ce.callee = std::move(callee);
        ce.args = std::move(args);
        ce.span = span;
        result->kind = std::move(ce);
        result->span = span;
        break;
    }
    case 5: { // MethodCall
        ExprPtr receiver = read_expr(c);
        std::string method = c.read_str();
        std::vector<TypePtr> type_args = read_type_list(c);
        uint64_t n = c.read_varint();
        std::vector<ExprPtr> args;
        args.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            args.push_back(read_expr(c));
        }
        bool optional_chain = c.read_bool();
        SourceSpan span = read_source_span(c);
        MethodCallExpr mc;
        mc.receiver = std::move(receiver);
        mc.method = std::move(method);
        mc.type_args = std::move(type_args);
        mc.args = std::move(args);
        mc.optional_chain = optional_chain;
        mc.span = span;
        result->kind = std::move(mc);
        result->span = span;
        break;
    }
    case 6: { // Field
        ExprPtr object = read_expr(c);
        std::string field = c.read_str();
        bool optional_chain = c.read_bool();
        SourceSpan span = read_source_span(c);
        FieldExpr fe;
        fe.object = std::move(object);
        fe.field = std::move(field);
        fe.optional_chain = optional_chain;
        fe.span = span;
        result->kind = std::move(fe);
        result->span = span;
        break;
    }
    case 7: { // Index
        ExprPtr object = read_expr(c);
        ExprPtr index = read_expr(c);
        SourceSpan span = read_source_span(c);
        IndexExpr ie;
        ie.object = std::move(object);
        ie.index = std::move(index);
        ie.span = span;
        result->kind = std::move(ie);
        result->span = span;
        break;
    }
    case 8: { // Tuple
        uint64_t n = c.read_varint();
        std::vector<ExprPtr> elems;
        elems.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            elems.push_back(read_expr(c));
        }
        SourceSpan span = read_source_span(c);
        TupleExpr te;
        te.elements = std::move(elems);
        te.span = span;
        result->kind = std::move(te);
        result->span = span;
        break;
    }
    case 9: { // Array
        uint8_t kind_tag = c.read_u8();
        ArrayExpr ae;
        if (kind_tag == 0) { // Elements
            uint64_t n = c.read_varint();
            std::vector<ExprPtr> elems;
            elems.reserve(static_cast<size_t>(n));
            for (uint64_t i = 0; i < n; i++) {
                elems.push_back(read_expr(c));
            }
            ae.kind = std::move(elems);
        } else if (kind_tag == 1) { // Repeat
            ExprPtr val = read_expr(c);
            ExprPtr cnt = read_expr(c);
            ae.kind = std::make_pair(std::move(val), std::move(cnt));
        } else {
            c.fail("invalid ArrayKind tag");
        }
        SourceSpan span = read_source_span(c);
        ae.span = span;
        result->kind = std::move(ae);
        result->span = span;
        break;
    }
    case 10: { // Struct
        TypePath path = read_type_path(c);
        SourceSpan path_span = path.span;
        auto generics = read_opt_type_args(c, path_span);
        uint64_t n = c.read_varint();
        std::vector<std::pair<std::string, ExprPtr>> fields;
        fields.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            std::string name = c.read_str();
            ExprPtr value = read_expr(c);
            fields.emplace_back(std::move(name), std::move(value));
        }
        std::optional<ExprPtr> base = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        StructExpr se;
        se.path = std::move(path);
        se.generics = std::move(generics);
        se.fields = std::move(fields);
        se.base = std::move(base);
        se.span = span;
        result->kind = std::move(se);
        result->span = span;
        break;
    }
    case 11: { // If
        ExprPtr cond = read_expr(c);
        ExprPtr then_b = read_expr(c);
        std::optional<ExprPtr> else_b = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        IfExpr ie;
        ie.condition = std::move(cond);
        ie.then_branch = std::move(then_b);
        ie.else_branch = std::move(else_b);
        ie.span = span;
        result->kind = std::move(ie);
        result->span = span;
        break;
    }
    case 12: { // Ternary
        ExprPtr cond = read_expr(c);
        ExprPtr tv = read_expr(c);
        ExprPtr fv = read_expr(c);
        SourceSpan span = read_source_span(c);
        TernaryExpr te;
        te.condition = std::move(cond);
        te.true_value = std::move(tv);
        te.false_value = std::move(fv);
        te.span = span;
        result->kind = std::move(te);
        result->span = span;
        break;
    }
    case 13: { // IfLet
        PatternPtr pat = read_pattern(c);
        ExprPtr scrut = read_expr(c);
        ExprPtr then_b = read_expr(c);
        std::optional<ExprPtr> else_b = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        IfLetExpr ile;
        ile.pattern = std::move(pat);
        ile.scrutinee = std::move(scrut);
        ile.then_branch = std::move(then_b);
        ile.else_branch = std::move(else_b);
        ile.span = span;
        result->kind = std::move(ile);
        result->span = span;
        break;
    }
    case 14: { // When
        ExprPtr scrut = read_expr(c);
        uint64_t n = c.read_varint();
        std::vector<WhenArm> arms;
        arms.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            WhenArm arm;
            arm.pattern = read_pattern(c);
            uint8_t guard_tag = c.read_u8();
            if (guard_tag == 1) {
                arm.guard = read_expr(c);
            } else if (guard_tag != 0) {
                c.fail("invalid Maybe tag for when arm guard");
            }
            arm.body = read_expr(c);
            arm.span = read_source_span(c);
            arms.push_back(std::move(arm));
        }
        SourceSpan span = read_source_span(c);
        WhenExpr we;
        we.scrutinee = std::move(scrut);
        we.arms = std::move(arms);
        we.span = span;
        result->kind = std::move(we);
        result->span = span;
        break;
    }
    case 15: { // Loop
        std::optional<std::string> label = c.read_opt_str();
        std::optional<LoopVarDecl> loop_var;
        uint8_t lv_tag = c.read_u8();
        if (lv_tag == 1) {
            LoopVarDecl lvd;
            lvd.name = c.read_str();
            lvd.type = read_type(c);
            lvd.span = read_source_span(c);
            loop_var = std::move(lvd);
        } else if (lv_tag != 0) {
            c.fail("invalid Maybe tag for loop_var");
        }
        ExprPtr cond = read_expr(c);
        ExprPtr body = read_expr(c);
        SourceSpan span = read_source_span(c);
        LoopExpr le;
        le.label = std::move(label);
        le.loop_var = std::move(loop_var);
        le.condition = std::move(cond);
        le.body = std::move(body);
        le.span = span;
        result->kind = std::move(le);
        result->span = span;
        break;
    }
    case 16: { // While
        std::optional<std::string> label = c.read_opt_str();
        ExprPtr cond = read_expr(c);
        ExprPtr body = read_expr(c);
        SourceSpan span = read_source_span(c);
        WhileExpr we;
        we.label = std::move(label);
        we.condition = std::move(cond);
        we.body = std::move(body);
        we.span = span;
        result->kind = std::move(we);
        result->span = span;
        break;
    }
    case 17: { // For
        std::optional<std::string> label = c.read_opt_str();
        PatternPtr pat = read_pattern(c);
        ExprPtr iter = read_expr(c);
        ExprPtr body = read_expr(c);
        SourceSpan span = read_source_span(c);
        ForExpr fe;
        fe.label = std::move(label);
        fe.pattern = std::move(pat);
        fe.iter = std::move(iter);
        fe.body = std::move(body);
        fe.span = span;
        result->kind = std::move(fe);
        result->span = span;
        break;
    }
    case 18: { // Block
        uint64_t n = c.read_varint();
        std::vector<StmtPtr> stmts;
        stmts.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            stmts.push_back(read_stmt(c));
        }
        std::optional<ExprPtr> trailing = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        BlockExpr be;
        be.stmts = std::move(stmts);
        be.expr = std::move(trailing);
        be.span = span;
        result->kind = std::move(be);
        result->span = span;
        break;
    }
    case 19: { // Return
        std::optional<ExprPtr> value = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        ReturnExpr re;
        re.value = std::move(value);
        re.span = span;
        result->kind = std::move(re);
        result->span = span;
        break;
    }
    case 20: { // Break
        std::optional<std::string> label = c.read_opt_str();
        std::optional<ExprPtr> value = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        BreakExpr be;
        be.label = std::move(label);
        be.value = std::move(value);
        be.span = span;
        result->kind = std::move(be);
        result->span = span;
        break;
    }
    case 21: { // Continue
        std::optional<std::string> label = c.read_opt_str();
        SourceSpan span = read_source_span(c);
        ContinueExpr ce;
        ce.label = std::move(label);
        ce.span = span;
        result->kind = std::move(ce);
        result->span = span;
        break;
    }
    case 22: { // Closure
        uint64_t n = c.read_varint();
        std::vector<std::pair<PatternPtr, std::optional<TypePtr>>> params;
        params.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            PatternPtr pat = read_pattern(c);
            std::optional<TypePtr> ty = read_opt_type(c);
            params.emplace_back(std::move(pat), std::move(ty));
        }
        std::optional<TypePtr> ret = read_opt_type(c);
        ExprPtr body = read_expr(c);
        bool is_move = c.read_bool();
        SourceSpan span = read_source_span(c);
        ClosureExpr cle;
        cle.params = std::move(params);
        cle.return_type = std::move(ret);
        cle.body = std::move(body);
        cle.is_move = is_move;
        cle.span = span;
        result->kind = std::move(cle);
        result->span = span;
        break;
    }
    case 23: { // Range
        std::optional<ExprPtr> start = read_opt_expr(c);
        std::optional<ExprPtr> end = read_opt_expr(c);
        bool inclusive = c.read_bool();
        SourceSpan span = read_source_span(c);
        RangeExpr re;
        re.start = std::move(start);
        re.end = std::move(end);
        re.inclusive = inclusive;
        re.span = span;
        result->kind = std::move(re);
        result->span = span;
        break;
    }
    case 24: { // Cast
        ExprPtr expr = read_expr(c);
        TypePtr target = read_type(c);
        SourceSpan span = read_source_span(c);
        CastExpr ce;
        ce.expr = std::move(expr);
        ce.target = std::move(target);
        ce.span = span;
        result->kind = std::move(ce);
        result->span = span;
        break;
    }
    case 25: { // Is
        ExprPtr expr = read_expr(c);
        TypePtr target = read_type(c);
        SourceSpan span = read_source_span(c);
        IsExpr ie;
        ie.expr = std::move(expr);
        ie.target = std::move(target);
        ie.span = span;
        result->kind = std::move(ie);
        result->span = span;
        break;
    }
    case 26: { // Try
        ExprPtr expr = read_expr(c);
        SourceSpan span = read_source_span(c);
        TryExpr te;
        te.expr = std::move(expr);
        te.span = span;
        result->kind = std::move(te);
        result->span = span;
        break;
    }
    case 27: { // Await
        ExprPtr expr = read_expr(c);
        SourceSpan span = read_source_span(c);
        AwaitExpr ae;
        ae.expr = std::move(expr);
        ae.span = span;
        result->kind = std::move(ae);
        result->span = span;
        break;
    }
    case 28: { // Throw
        ExprPtr expr = read_expr(c);
        SourceSpan span = read_source_span(c);
        ThrowExpr te;
        te.expr = std::move(expr);
        te.span = span;
        result->kind = std::move(te);
        result->span = span;
        break;
    }
    case 29: { // Path
        TypePath path = read_type_path(c);
        SourceSpan path_span = path.span;
        auto generics = read_opt_type_args(c, path_span);
        SourceSpan span = read_source_span(c);
        PathExpr pe;
        pe.path = std::move(path);
        pe.generics = std::move(generics);
        pe.span = span;
        result->kind = std::move(pe);
        result->span = span;
        break;
    }
    case 30: { // Lowlevel
        uint64_t n = c.read_varint();
        std::vector<StmtPtr> stmts;
        stmts.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            stmts.push_back(read_stmt(c));
        }
        std::optional<ExprPtr> trailing = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        LowlevelExpr lle;
        lle.stmts = std::move(stmts);
        lle.expr = std::move(trailing);
        lle.span = span;
        result->kind = std::move(lle);
        result->span = span;
        break;
    }
    case 31: { // InterpolatedString
        uint64_t n = c.read_varint();
        std::vector<InterpolatedSegment> segs;
        segs.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            segs.push_back(read_interpolated_segment(c));
        }
        SourceSpan span = read_source_span(c);
        InterpolatedStringExpr ise;
        ise.segments = std::move(segs);
        ise.span = span;
        result->kind = std::move(ise);
        result->span = span;
        break;
    }
    case 32: { // TemplateLiteral
        uint64_t n = c.read_varint();
        std::vector<InterpolatedSegment> segs;
        segs.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            segs.push_back(read_interpolated_segment(c));
        }
        SourceSpan span = read_source_span(c);
        TemplateLiteralExpr tle;
        tle.segments = std::move(segs);
        tle.span = span;
        result->kind = std::move(tle);
        result->span = span;
        break;
    }
    case 33: { // Base
        std::string member = c.read_str();
        std::vector<TypePtr> type_args = read_type_list(c);
        uint64_t n = c.read_varint();
        std::vector<ExprPtr> args;
        args.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            args.push_back(read_expr(c));
        }
        bool is_method_call = c.read_bool();
        SourceSpan span = read_source_span(c);
        BaseExpr be;
        be.member = std::move(member);
        be.type_args = std::move(type_args);
        be.args = std::move(args);
        be.is_method_call = is_method_call;
        be.span = span;
        result->kind = std::move(be);
        result->span = span;
        break;
    }
    case 34: { // New
        TypePath class_type = read_type_path(c);
        SourceSpan path_span = class_type.span;
        auto generics = read_opt_type_args(c, path_span);
        uint64_t n = c.read_varint();
        std::vector<ExprPtr> args;
        args.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            args.push_back(read_expr(c));
        }
        SourceSpan span = read_source_span(c);
        NewExpr ne;
        ne.class_type = std::move(class_type);
        ne.generics = std::move(generics);
        ne.args = std::move(args);
        ne.span = span;
        result->kind = std::move(ne);
        result->span = span;
        break;
    }
    default:
        c.fail("invalid Expr tag");
    }
    return result;
}

// ============================================================================
// Statements
// ============================================================================

StmtPtr read_stmt(Cursor& c) {
    uint8_t tag = c.read_u8();
    auto result = std::make_unique<Stmt>();
    switch (tag) {
    case 0: { // Let
        PatternPtr pat = read_pattern(c);
        std::optional<TypePtr> ty = read_opt_type(c);
        std::optional<ExprPtr> init = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        LetStmt ls;
        ls.pattern = std::move(pat);
        ls.type_annotation = std::move(ty);
        ls.init = std::move(init);
        ls.span = span;
        result->kind = std::move(ls);
        result->span = span;
        break;
    }
    case 1: { // Var
        std::string name = c.read_str();
        std::optional<TypePtr> ty = read_opt_type(c);
        std::optional<ExprPtr> init = read_opt_expr(c);
        SourceSpan span = read_source_span(c);
        VarStmt vs;
        vs.name = std::move(name);
        vs.type_annotation = std::move(ty);
        // C++ VarStmt requires init to be non-null. If the writer encoded
        // Nothing, synthesize a unit literal placeholder so the AST stays
        // structurally valid.
        if (init.has_value()) {
            vs.init = std::move(*init);
        } else {
            auto unit = std::make_unique<Expr>();
            TupleExpr te;
            te.span = span;
            unit->kind = std::move(te);
            unit->span = span;
            vs.init = std::move(unit);
        }
        vs.span = span;
        result->kind = std::move(vs);
        result->span = span;
        break;
    }
    case 2: { // LetElse
        PatternPtr pat = read_pattern(c);
        ExprPtr init = read_expr(c);
        ExprPtr else_body = read_expr(c);
        SourceSpan span = read_source_span(c);
        LetElseStmt les;
        les.pattern = std::move(pat);
        les.init = std::move(init);
        les.else_block = std::move(else_body);
        les.span = span;
        result->kind = std::move(les);
        result->span = span;
        break;
    }
    case 3: { // Expr
        ExprPtr e = read_expr(c);
        SourceSpan span = e->span;
        ExprStmt es;
        es.expr = std::move(e);
        es.span = span;
        result->kind = std::move(es);
        result->span = span;
        break;
    }
    case 4: { // Decl
        DeclPtr d = read_decl(c);
        SourceSpan span = d->span;
        result->kind = std::move(d);
        result->span = span;
        break;
    }
    default:
        c.fail("invalid Stmt tag");
    }
    return result;
}

// ============================================================================
// Declarations — generic params, params, fields, FuncDecl shared body
// ============================================================================

GenericParam read_generic_param(Cursor& c) {
    GenericParam gp;
    gp.name = c.read_str();
    uint64_t n = c.read_varint();
    // The TML side serializes bounds as TypePath; the C++ side stores them as
    // TypePtr (so they can be parameterized like `Iterator[Item=T]`). Wrap
    // each path in a NamedType.
    gp.bounds.reserve(static_cast<size_t>(n));
    for (uint64_t i = 0; i < n; i++) {
        TypePath p = read_type_path(c);
        SourceSpan psp = p.span;
        auto t = std::make_unique<Type>();
        NamedType nt;
        nt.path = std::move(p);
        nt.span = psp;
        t->kind = std::move(nt);
        t->span = psp;
        gp.bounds.push_back(std::move(t));
    }
    gp.span = read_source_span(c);
    return gp;
}

std::vector<GenericParam> read_generic_params(Cursor& c) {
    uint64_t n = c.read_varint();
    std::vector<GenericParam> out;
    out.reserve(static_cast<size_t>(n));
    for (uint64_t i = 0; i < n; i++) {
        out.push_back(read_generic_param(c));
    }
    return out;
}

FuncParam read_func_param(Cursor& c) {
    FuncParam fp;
    std::string name = c.read_str();
    TypePtr ty = read_type(c);
    bool is_this = c.read_bool();
    bool is_mut = c.read_bool();
    SourceSpan span = read_source_span(c);
    // Build an IdentPattern to mirror the original parameter binding name.
    auto pat = std::make_unique<Pattern>();
    IdentPattern ip;
    ip.name = std::move(name);
    ip.is_mut = is_mut;
    ip.span = span;
    pat->kind = std::move(ip);
    pat->span = span;
    fp.pattern = std::move(pat);
    fp.type = std::move(ty);
    fp.span = span;
    (void)is_this; // C++ FuncParam has no explicit `is_this` flag — encoded
                   // by parameter name being "this" in current convention.
    return fp;
}

StructField read_struct_field(Cursor& c) {
    StructField sf;
    sf.name = c.read_str();
    sf.type = read_type(c);
    sf.vis = read_visibility(c);
    sf.span = read_source_span(c);
    return sf;
}

FuncDecl read_func_decl_body(Cursor& c) {
    FuncDecl fd;
    fd.name = c.read_str();
    fd.vis = read_visibility(c);
    fd.generics = read_generic_params(c);
    uint64_t pn = c.read_varint();
    fd.params.reserve(static_cast<size_t>(pn));
    for (uint64_t i = 0; i < pn; i++) {
        fd.params.push_back(read_func_param(c));
    }
    fd.return_type = read_opt_type(c);
    // Body: Maybe[Heap[TmlExpr]]
    uint8_t body_tag = c.read_u8();
    if (body_tag == 1) {
        ExprPtr body_expr = read_expr(c);
        // BlockExpr is required for FuncDecl::body. If the body expression is
        // already a BlockExpr we move it; otherwise wrap it in a synthetic
        // single-trailing-expression block so the AST shape stays valid.
        if (body_expr->is<BlockExpr>()) {
            fd.body = std::move(body_expr->as<BlockExpr>());
        } else {
            BlockExpr blk;
            blk.span = body_expr->span;
            blk.expr = std::move(body_expr);
            fd.body = std::move(blk);
        }
    } else if (body_tag != 0) {
        c.fail("invalid Maybe tag for func body");
    }
    fd.is_async = c.read_bool();
    bool is_override = c.read_bool();
    (void)is_override; // C++ FuncDecl has no `is_override` field; this flag
                       // is only meaningful for ClassMethod, which is not
                       // produced by the standalone func decoder.
    fd.is_unsafe = false;
    fd.span = read_source_span(c);
    return fd;
}

// ============================================================================
// Declarations
// ============================================================================

DeclPtr read_decl(Cursor& c) {
    uint8_t tag = c.read_u8();
    auto result = std::make_unique<Decl>();
    switch (tag) {
    case 0: { // Func
        FuncDecl fd = read_func_decl_body(c);
        SourceSpan span = fd.span;
        result->kind = std::move(fd);
        result->span = span;
        break;
    }
    case 1: { // Struct
        StructDecl sd;
        sd.name = c.read_str();
        sd.vis = read_visibility(c);
        sd.generics = read_generic_params(c);
        uint64_t n = c.read_varint();
        sd.fields.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            sd.fields.push_back(read_struct_field(c));
        }
        sd.span = read_source_span(c);
        SourceSpan span = sd.span;
        result->kind = std::move(sd);
        result->span = span;
        break;
    }
    case 2: { // Union
        UnionDecl ud;
        ud.name = c.read_str();
        ud.vis = read_visibility(c);
        // The TML serializer writes generics for unions, but the C++ UnionDecl
        // does not have a generics field — read and discard them.
        (void)read_generic_params(c);
        uint64_t n = c.read_varint();
        ud.fields.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            ud.fields.push_back(read_struct_field(c));
        }
        ud.span = read_source_span(c);
        SourceSpan span = ud.span;
        result->kind = std::move(ud);
        result->span = span;
        break;
    }
    case 3: { // Enum
        EnumDecl ed;
        ed.name = c.read_str();
        ed.vis = read_visibility(c);
        ed.generics = read_generic_params(c);
        uint64_t n = c.read_varint();
        ed.variants.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            EnumVariant v;
            v.name = c.read_str();
            std::vector<TypePtr> fields = read_type_list(c);
            v.span = read_source_span(c);
            // Always reconstructed as a tuple variant (the TML mirror only
            // models tuple variants).
            v.tuple_fields = std::move(fields);
            ed.variants.push_back(std::move(v));
        }
        ed.span = read_source_span(c);
        SourceSpan span = ed.span;
        result->kind = std::move(ed);
        result->span = span;
        break;
    }
    case 4: { // Trait
        TraitDecl td;
        td.name = c.read_str();
        td.vis = read_visibility(c);
        td.generics = read_generic_params(c);
        uint64_t sn = c.read_varint();
        td.super_traits.reserve(static_cast<size_t>(sn));
        for (uint64_t i = 0; i < sn; i++) {
            TypePath p = read_type_path(c);
            SourceSpan psp = p.span;
            auto t = std::make_unique<Type>();
            NamedType nt;
            nt.path = std::move(p);
            nt.span = psp;
            t->kind = std::move(nt);
            t->span = psp;
            td.super_traits.push_back(std::move(t));
        }
        uint64_t mn = c.read_varint();
        td.methods.reserve(static_cast<size_t>(mn));
        for (uint64_t i = 0; i < mn; i++) {
            td.methods.push_back(read_func_decl_body(c));
        }
        td.span = read_source_span(c);
        SourceSpan span = td.span;
        result->kind = std::move(td);
        result->span = span;
        break;
    }
    case 5: { // Impl
        ImplDecl id;
        id.self_type = read_type(c);
        // behavior: Maybe[TmlTypePath]
        uint8_t b_tag = c.read_u8();
        if (b_tag == 1) {
            TypePath p = read_type_path(c);
            SourceSpan psp = p.span;
            auto t = std::make_unique<Type>();
            NamedType nt;
            nt.path = std::move(p);
            nt.span = psp;
            t->kind = std::move(nt);
            t->span = psp;
            id.trait_type = std::move(t);
        } else if (b_tag != 0) {
            c.fail("invalid Maybe tag for impl behavior");
        }
        id.generics = read_generic_params(c);
        uint64_t mn = c.read_varint();
        id.methods.reserve(static_cast<size_t>(mn));
        for (uint64_t i = 0; i < mn; i++) {
            id.methods.push_back(read_func_decl_body(c));
        }
        id.span = read_source_span(c);
        SourceSpan span = id.span;
        result->kind = std::move(id);
        result->span = span;
        break;
    }
    case 6: { // TypeAlias
        TypeAliasDecl ta;
        ta.name = c.read_str();
        ta.vis = read_visibility(c);
        ta.generics = read_generic_params(c);
        ta.type = read_type(c);
        ta.span = read_source_span(c);
        SourceSpan span = ta.span;
        result->kind = std::move(ta);
        result->span = span;
        break;
    }
    case 7: { // Const
        ConstDecl cd;
        cd.name = c.read_str();
        cd.vis = read_visibility(c);
        cd.type = read_type(c);
        cd.value = read_expr(c);
        cd.span = read_source_span(c);
        SourceSpan span = cd.span;
        result->kind = std::move(cd);
        result->span = span;
        break;
    }
    case 8: { // Use
        UseDecl ud;
        uint64_t segn = c.read_varint();
        TypePath path;
        path.segments.reserve(static_cast<size_t>(segn));
        for (uint64_t i = 0; i < segn; i++) {
            path.segments.push_back(c.read_str());
        }
        ud.alias = c.read_opt_str();
        ud.vis = read_visibility(c);
        ud.span = read_source_span(c);
        path.span = ud.span;
        ud.path = std::move(path);
        SourceSpan span = ud.span;
        result->kind = std::move(ud);
        result->span = span;
        break;
    }
    case 9: { // Mod
        ModDecl md;
        md.name = c.read_str();
        md.vis = read_visibility(c);
        uint64_t n = c.read_varint();
        std::vector<DeclPtr> items;
        items.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            items.push_back(read_decl(c));
        }
        md.items = std::move(items);
        md.span = read_source_span(c);
        SourceSpan span = md.span;
        result->kind = std::move(md);
        result->span = span;
        break;
    }
    case 10: { // Class
        ClassDecl cd;
        cd.name = c.read_str();
        cd.vis = read_visibility(c);
        cd.generics = read_generic_params(c);
        uint8_t parent_tag = c.read_u8();
        if (parent_tag == 1) {
            cd.extends = read_type_path(c);
        } else if (parent_tag != 0) {
            c.fail("invalid Maybe tag for class parent");
        }
        uint64_t in = c.read_varint();
        cd.implements.reserve(static_cast<size_t>(in));
        for (uint64_t i = 0; i < in; i++) {
            TypePath p = read_type_path(c);
            SourceSpan psp = p.span;
            auto t = std::make_unique<Type>();
            NamedType nt;
            nt.path = std::move(p);
            nt.span = psp;
            t->kind = std::move(nt);
            t->span = psp;
            cd.implements.push_back(std::move(t));
        }
        uint64_t fn = c.read_varint();
        cd.fields.reserve(static_cast<size_t>(fn));
        for (uint64_t i = 0; i < fn; i++) {
            ClassField cf;
            cf.name = c.read_str();
            cf.type = read_type(c);
            // Visibility encoded with the same Visibility enum, mapped to
            // MemberVisibility (Private/Public/PubCrate→Public).
            Visibility v = read_visibility(c);
            switch (v) {
            case Visibility::Private:
                cf.vis = MemberVisibility::Private;
                break;
            case Visibility::Public:
                cf.vis = MemberVisibility::Public;
                break;
            case Visibility::PubCrate:
                cf.vis = MemberVisibility::Public;
                break;
            }
            cf.is_static = false;
            cf.span = read_source_span(c);
            cd.fields.push_back(std::move(cf));
        }
        uint64_t mn = c.read_varint();
        cd.methods.reserve(static_cast<size_t>(mn));
        for (uint64_t i = 0; i < mn; i++) {
            FuncDecl fd = read_func_decl_body(c);
            ClassMethod cm;
            cm.name = std::move(fd.name);
            cm.generics = std::move(fd.generics);
            cm.params = std::move(fd.params);
            cm.return_type = std::move(fd.return_type);
            cm.body = std::move(fd.body);
            switch (fd.vis) {
            case Visibility::Private:
                cm.vis = MemberVisibility::Private;
                break;
            case Visibility::Public:
                cm.vis = MemberVisibility::Public;
                break;
            case Visibility::PubCrate:
                cm.vis = MemberVisibility::Public;
                break;
            }
            cm.is_static = false;
            cm.is_virtual = false;
            cm.is_override = false;
            cm.is_abstract = false;
            cm.is_final = false;
            cm.span = fd.span;
            cd.methods.push_back(std::move(cm));
        }
        cd.span = read_source_span(c);
        cd.is_abstract = false;
        cd.is_sealed = false;
        SourceSpan span = cd.span;
        result->kind = std::move(cd);
        result->span = span;
        break;
    }
    case 11: { // Interface
        InterfaceDecl ifd;
        ifd.name = c.read_str();
        ifd.vis = read_visibility(c);
        ifd.generics = read_generic_params(c);
        uint64_t mn = c.read_varint();
        ifd.methods.reserve(static_cast<size_t>(mn));
        for (uint64_t i = 0; i < mn; i++) {
            FuncDecl fd = read_func_decl_body(c);
            InterfaceMethod im;
            im.name = std::move(fd.name);
            im.generics = std::move(fd.generics);
            im.params = std::move(fd.params);
            im.return_type = std::move(fd.return_type);
            im.default_body = std::move(fd.body);
            im.is_static = false;
            im.span = fd.span;
            ifd.methods.push_back(std::move(im));
        }
        ifd.span = read_source_span(c);
        SourceSpan span = ifd.span;
        result->kind = std::move(ifd);
        result->span = span;
        break;
    }
    case 12: { // Namespace
        NamespaceDecl nd;
        std::string name = c.read_str();
        // C++ NamespaceDecl uses a path vector; the TML mirror only stores a
        // single name string. Place the entire name as a single segment.
        nd.path.push_back(std::move(name));
        (void)read_visibility(c); // C++ NamespaceDecl has no visibility slot
        uint64_t n = c.read_varint();
        nd.items.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; i++) {
            nd.items.push_back(read_decl(c));
        }
        nd.span = read_source_span(c);
        SourceSpan span = nd.span;
        result->kind = std::move(nd);
        result->span = span;
        break;
    }
    case 13: { // BehaviorAlias
        BehaviorAliasDecl ba;
        ba.name = c.read_str();
        ba.vis = read_visibility(c);
        ba.generics = read_generic_params(c);
        uint64_t bn = c.read_varint();
        ba.bounds.reserve(static_cast<size_t>(bn));
        for (uint64_t i = 0; i < bn; i++) {
            TypePath p = read_type_path(c);
            SourceSpan psp = p.span;
            auto t = std::make_unique<Type>();
            NamedType nt;
            nt.path = std::move(p);
            nt.span = psp;
            t->kind = std::move(nt);
            t->span = psp;
            ba.bounds.push_back(std::move(t));
        }
        ba.span = read_source_span(c);
        SourceSpan span = ba.span;
        result->kind = std::move(ba);
        result->span = span;
        break;
    }
    default:
        c.fail("invalid Decl tag");
    }
    return result;
}

// ============================================================================
// Module
// ============================================================================

Module read_module(Cursor& c) {
    Module m;
    m.name = c.read_str();
    uint64_t doc_n = c.read_varint();
    m.module_docs.reserve(static_cast<size_t>(doc_n));
    for (uint64_t i = 0; i < doc_n; i++) {
        m.module_docs.push_back(c.read_str());
    }
    m.span = read_source_span(c);
    uint64_t decl_n = c.read_varint();
    m.decls.reserve(static_cast<size_t>(decl_n));
    for (uint64_t i = 0; i < decl_n; i++) {
        m.decls.push_back(read_decl(c));
    }
    return m;
}

} // namespace

// ============================================================================
// Public entry point
// ============================================================================

parser::Module read_ast(const std::vector<uint8_t>& data, std::string_view file_path) {
    Cursor c(data, file_path);
    uint32_t magic = c.read_u32_le();
    if (magic != AST_MAGIC) {
        throw std::runtime_error("ast_reader: bad magic (expected 0x41535420 \"AST \", got 0x" +
                                 [&] {
                                     char buf[16];
                                     std::snprintf(buf, sizeof(buf), "%08X", magic);
                                     return std::string(buf);
                                 }() +
                                 ")");
    }
    uint8_t major = c.read_u8();
    uint8_t minor = c.read_u8();
    if (major != AST_VERSION_MAJOR) {
        throw std::runtime_error("ast_reader: unsupported AST major version " +
                                 std::to_string(static_cast<int>(major)) + " (expected " +
                                 std::to_string(static_cast<int>(AST_VERSION_MAJOR)) + ")");
    }
    (void)minor; // forward-compat: any minor accepted
    return read_module(c);
}

} // namespace tml::serial
