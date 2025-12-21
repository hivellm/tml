#include "tml/common.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include <iostream>
#include <string>

using namespace tml;

void print_usage() {
    std::cout << "TML Compiler " << VERSION << "\n\n";
    std::cout << "Usage: tml <command> [options] [files]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  build     Compile the project\n";
    std::cout << "  run       Build and run the project\n";
    std::cout << "  check     Type-check without generating code\n";
    std::cout << "  test      Run tests\n";
    std::cout << "  fmt       Format source files\n";
    std::cout << "  new       Create a new project\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --help, -h       Show this help\n";
    std::cout << "  --version, -V    Show version\n";
    std::cout << "  --release        Build with optimizations\n";
    std::cout << "  --verbose        Show detailed output\n";
}

void print_version() {
    std::cout << "tml " << VERSION << "\n";
}

int run_lexer_demo(const std::string& code) {
    auto source = lexer::Source::from_string(code, "<stdin>");
    lexer::Lexer lex(source);

    auto tokens = lex.tokenize();

    std::cout << "Tokens:\n";
    for (const auto& token : tokens) {
        std::cout << "  " << lexer::token_kind_to_string(token.kind);
        if (token.kind == lexer::TokenKind::Identifier ||
            token.kind == lexer::TokenKind::IntLiteral ||
            token.kind == lexer::TokenKind::FloatLiteral ||
            token.kind == lexer::TokenKind::StringLiteral) {
            std::cout << " (" << token.lexeme << ")";
        }
        std::cout << " @ " << token.span.start.line << ":" << token.span.start.column;
        std::cout << "\n";
    }

    if (lex.has_errors()) {
        std::cerr << "\nErrors:\n";
        for (const auto& error : lex.errors()) {
            std::cerr << "  " << error.span.start.line << ":"
                      << error.span.start.column << ": "
                      << error.message << "\n";
        }
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    if (command == "--version" || command == "-V") {
        print_version();
        return 0;
    }

    if (command == "lex") {
        // Demo command for testing lexer
        if (argc < 3) {
            std::cerr << "Usage: tml lex <code>\n";
            return 1;
        }
        return run_lexer_demo(argv[2]);
    }

    if (command == "build" || command == "run" || command == "check" ||
        command == "test" || command == "fmt" || command == "new") {
        std::cerr << "Error: '" << command << "' command not yet implemented\n";
        std::cerr << "The compiler is still in early development.\n";
        return 1;
    }

    std::cerr << "Error: Unknown command '" << command << "'\n";
    std::cerr << "Run 'tml --help' for usage information.\n";
    return 1;
}
