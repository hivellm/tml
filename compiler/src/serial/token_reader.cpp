TML_MODULE("compiler")

#include "serial/token_reader.hpp"

#include "lexer/source.hpp"
#include "lexer/token.hpp"

#include <cstring>
#include <stdexcept>

namespace tml::serial {

namespace {

constexpr uint32_t TOKEN_MAGIC = 0x544D4C4CU; // "TMLL"

class Cursor {
public:
    Cursor(const uint8_t* data, size_t len) : data_(data), len_(len) {}

    uint8_t u8() {
        require(1);
        return data_[pos_++];
    }
    uint32_t u32() {
        require(4);
        uint32_t v;
        std::memcpy(&v, data_ + pos_, 4);
        pos_ += 4;
        return v;
    }
    uint64_t u64() {
        require(8);
        uint64_t v;
        std::memcpy(&v, data_ + pos_, 8);
        pos_ += 8;
        return v;
    }
    bool b() {
        return u8() != 0;
    }
    uint64_t varint() {
        uint64_t v = 0;
        unsigned shift = 0;
        while (true) {
            uint8_t byte = u8();
            v |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) {
                return v;
            }
            shift += 7;
            if (shift > 63) {
                throw std::runtime_error("varint overflow");
            }
        }
    }
    std::string str() {
        auto n = static_cast<size_t>(varint());
        require(n);
        std::string s(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
        return s;
    }
    void skip(size_t n) {
        require(n);
        pos_ += n;
    }
    [[nodiscard]] bool eof() const {
        return pos_ >= len_;
    }

private:
    void require(size_t n) const {
        if (pos_ + n > len_) {
            throw std::runtime_error("token blob truncated");
        }
    }
    const uint8_t* data_;
    size_t len_;
    size_t pos_ = 0;
};

void read_value(Cursor& c, uint8_t tag, lexer::Token& t) {
    switch (tag) {
    case 0:
        return; // monostate
    case 1: {   // IntValue: u8 width + u8 signed + u64 magnitude
        c.u8();
        c.u8();
        uint64_t v = c.u64();
        t.value = lexer::IntValue{v, 10};
        return;
    }
    case 2: { // FloatValue: u8 width + f64
        c.u8();
        uint64_t bits = c.u64();
        double d;
        std::memcpy(&d, &bits, 8);
        t.value = lexer::FloatValue{d};
        return;
    }
    case 3: { // StringValue: string
        t.value = lexer::StringValue{c.str()};
        return;
    }
    case 4: { // CharValue: u32 codepoint
        t.value = lexer::CharValue{static_cast<char32_t>(c.u32())};
        return;
    }
    case 5: { // bool
        t.value = (c.u8() != 0);
        return;
    }
    case 6: { // DocValue: string + bool
        std::string text = c.str();
        bool inner = c.u8() != 0;
        (void)inner;
        t.value = lexer::DocValue{std::move(text)};
        return;
    }
    default:
        throw std::runtime_error("unknown token value tag");
    }
}

} // namespace

query::TokenizeResult read_tokens(const std::vector<uint8_t>& bytes) {
    query::TokenizeResult result;
    try {
        Cursor c(bytes.data(), bytes.size());
        uint32_t magic = c.u32();
        if (magic != TOKEN_MAGIC) {
            result.errors.push_back("token blob: bad magic");
            return result;
        }
        uint8_t major = c.u8();
        c.u8(); // minor
        if (major != 1) {
            result.errors.push_back("token blob: unsupported major version");
            return result;
        }

        std::string source_path = c.str();
        std::string source_text = c.str();
        bool stage_success = c.b();

        auto err_count = static_cast<size_t>(c.varint());
        for (size_t i = 0; i < err_count; ++i) {
            result.errors.push_back(c.str());
        }
        if (!stage_success) {
            return result;
        }

        auto source = std::make_shared<lexer::Source>(
            lexer::Source::from_string(std::move(source_text), source_path));
        auto src_text = source->content();

        auto tok_count = static_cast<size_t>(c.varint());
        auto tokens = std::make_shared<std::vector<lexer::Token>>();
        tokens->reserve(tok_count);

        for (size_t i = 0; i < tok_count; ++i) {
            lexer::Token t;
            t.kind = static_cast<lexer::TokenKind>(c.u8());
            uint32_t off = c.u32();
            uint32_t len = c.u32();
            uint32_t line = c.u32();
            uint32_t col = c.u32();

            t.span.start.line = line;
            t.span.start.column = col;
            t.span.start.offset = off;
            t.span.start.length = len;
            // End: same line/col approximation; consumer rarely uses end col.
            t.span.end.line = line;
            t.span.end.column = col + len;
            t.span.end.offset = off + len;
            t.span.end.length = 0;

            if (off + len <= src_text.size()) {
                t.lexeme = std::string_view(src_text.data() + off, len);
            }

            uint8_t value_tag = c.u8();
            read_value(c, value_tag, t);

            tokens->push_back(std::move(t));
        }

        result.tokens = std::move(tokens);
        result.source = std::move(source);
        result.success = true;
        return result;
    } catch (const std::exception& e) {
        result.errors.emplace_back(std::string("token blob: ") + e.what());
        return result;
    }
}

} // namespace tml::serial
