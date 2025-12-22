#include "tml/common.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include "tml/parser/parser.hpp"
#include "tml/types/checker.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

using namespace tml;
namespace fs = std::filesystem;

void print_usage() {
    std::cout << "TML Compiler " << VERSION << "

";
    std::cout << "Usage: tml <command> [options] [files]

";
    std::cout << "Commands:
";
    std::cout << "  build     Compile the project
";
    std::cout << "  run       Build and run the project
";
    std::cout << "  check     Type-check without generating code
";
    std::cout << "  test      Run tests
";
    std::cout << "  fmt       Format source files
";
    std::cout << "  new       Create a new project
";
    std::cout << "  lex       Tokenize a file (debug)
";
    std::cout << "  parse     Parse a file (debug)
";
    std::cout << "
Options:
";
    std::cout << "  --help, -h       Show this help
";
    std::cout << "  --version, -V    Show version
";
    std::cout << "  --release        Build with optimizations
";
    std::cout << "  --verbose        Show detailed output
";
}

void print_version() {
    std::cout << "tml " << VERSION << "
";
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int run_lex(const std::string& path, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "
";
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (verbose) {
        std::cout << "Tokens (" << tokens.size() << "):
";
        for (const auto& token : tokens) {
            std::cout << "  " << token.span.start.line << ":"
                      << token.span.start.column << " "
                      << lexer::token_kind_to_string(token.kind);
            if (token.kind == lexer::TokenKind::Identifier ||
                token.kind == lexer::TokenKind::IntLiteral ||
                token.kind == lexer::TokenKind::FloatLiteral ||
                token.kind == lexer::TokenKind::StringLiteral) {
                std::cout << " ";
            }
            std::cout << "
";
        }
    }

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "
";
        }
        return 1;
    }

    if (!verbose) {
        std::cout << "Lexed " << tokens.size() << " tokens from " << path << "
";
    }
    return 0;
}
