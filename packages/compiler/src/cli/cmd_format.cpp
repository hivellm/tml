#include "cmd_format.hpp"
#include "utils.hpp"
#include "tml/common.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include "tml/parser/parser.hpp"
#include "tml/format/formatter.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace tml;

namespace tml::cli {

int run_fmt(const std::string& path, bool check_only, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    format::FormatOptions options;
    format::Formatter formatter(options);
    auto formatted = formatter.format(module);

    if (check_only) {
        if (formatted != source_code) {
            std::cerr << path << " would be reformatted\n";
            return 1;
        }
        if (verbose) {
            std::cout << path << " is correctly formatted\n";
        }
        return 0;
    }

    std::ofstream out(path);
    if (!out) {
        std::cerr << "error: Cannot write to " << path << "\n";
        return 1;
    }
    out << formatted;
    out.close();

    if (verbose) {
        std::cout << "Formatted " << path << "\n";
    } else {
        std::cout << "fmt: " << path << "\n";
    }

    return 0;
}

}
