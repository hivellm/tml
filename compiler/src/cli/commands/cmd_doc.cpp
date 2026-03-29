TML_MODULE("compiler")

//! # Documentation Command Implementation
//!
//! This file implements the `tml doc` command for generating documentation.

// Windows socket headers must come before any other windows headers.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
#define INVALID_SOCK INVALID_SOCKET
#define close_sock closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCK (-1)
#define close_sock close
#endif

#include "cli/diagnostic.hpp"
#include "cli/utils.hpp"
#include "cmd_doc.hpp"
#include "doc/extractor.hpp"
#include "doc/generators.hpp"
#include "lexer/lexer.hpp"
#include "log/log.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// Symbol Lookup (tml doc <symbol>)
// ============================================================================

/// ANSI color codes for terminal output.
namespace ansi {
static const char* RESET = "\033[0m";
static const char* BOLD = "\033[1m";
static const char* CYAN = "\033[36m";
static const char* YELLOW = "\033[33m";
static const char* GREEN = "\033[32m";
static const char* BLUE = "\033[34m";
static const char* DIM = "\033[2m";
static const char* RED = "\033[31m";
} // namespace ansi

/// Discovers the TML project root from the executable location or cwd.
static auto find_project_root() -> fs::path {
    auto cwd = fs::current_path();
    for (auto dir = cwd; dir.has_parent_path() && dir != dir.root_path(); dir = dir.parent_path()) {
        if (fs::exists(dir / "lib" / "core" / "src") && fs::exists(dir / "lib" / "std" / "src")) {
            return dir;
        }
    }
    // Check relative to cwd for common build output locations (build/debug/)
    std::vector<fs::path> candidates = {cwd / ".." / "..", cwd / "..", cwd / ".." / ".." / ".."};
    for (const auto& c : candidates) {
        std::error_code ec;
        auto norm = fs::weakly_canonical(c, ec);
        if (!ec && fs::exists(norm / "lib" / "core" / "src") &&
            fs::exists(norm / "lib" / "std" / "src")) {
            return norm;
        }
    }
    return {};
}

/// Collects all non-test .tml source files from a directory.
static auto collect_tml_source_files(const fs::path& dir) -> std::vector<fs::path> {
    std::vector<fs::path> files;
    if (!fs::exists(dir))
        return files;
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (!e.is_regular_file())
            continue;
        if (e.path().extension() != ".tml")
            continue;
        auto s = e.path().string();
        if (s.find("tests") != std::string::npos || s.find(".test.") != std::string::npos)
            continue;
        files.push_back(e.path());
    }
    return files;
}

/// Parses a single .tml file and adds its module to the doc index.
static void parse_file_into_index(const fs::path& file, doc::Extractor& extractor,
                                  std::vector<std::pair<parser::Module, std::string>>& out) {
    // Read file
    std::ifstream f(file);
    if (!f)
        return;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string source = ss.str();

    // Run preprocessor
    auto pp_config = preprocessor::Preprocessor::host_config();
    preprocessor::Preprocessor pp(pp_config);
    auto pp_result = pp.process(source, file.string());
    if (!pp_result.success())
        return;

    // Lex
    auto src = lexer::Source::from_string(pp_result.output, file.string());
    lexer::Lexer lex(src);
    auto tokens = lex.tokenize();
    if (lex.has_errors())
        return;

    // Parse
    auto module_name = file.stem().string();
    parser::Parser parser(std::move(tokens));
    auto parse_result = parser.parse_module(module_name);
    if (!std::holds_alternative<parser::Module>(parse_result))
        return;

    auto& module = std::get<parser::Module>(parse_result);

    // Derive module path: lib/core/src/str.tml -> core::str
    std::string mod_path;
    {
        auto s = fs::weakly_canonical(file).string();
        std::replace(s.begin(), s.end(), '\\', '/');
        // Find /lib/ anchor
        auto lib_pos = s.find("/lib/");
        if (lib_pos != std::string::npos) {
            auto rel = s.substr(lib_pos + 5); // skip /lib/
            // Remove /src/ component
            auto src_pos = rel.find("/src/");
            if (src_pos != std::string::npos) {
                rel = rel.substr(0, src_pos) + "/" + rel.substr(src_pos + 5);
            }
            // Remove .tml extension
            if (rel.size() > 4 && rel.substr(rel.size() - 4) == ".tml") {
                rel = rel.substr(0, rel.size() - 4);
            }
            // Remove /mod suffix
            if (rel.size() > 4 && rel.substr(rel.size() - 4) == "/mod") {
                rel = rel.substr(0, rel.size() - 4);
            }
            // Convert / to ::
            mod_path.reserve(rel.size() + 4);
            for (size_t i = 0; i < rel.size(); ++i) {
                if (rel[i] == '/') {
                    mod_path += "::";
                } else {
                    mod_path += rel[i];
                }
            }
        } else {
            mod_path = module_name;
        }
    }

    out.emplace_back(std::move(module), mod_path);
}

/// Case-insensitive substring check.
static auto icontains(const std::string& haystack, const std::string& needle) -> bool {
    if (needle.empty())
        return true;
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                       [](char a, char b) {
                           return std::tolower(static_cast<unsigned char>(a)) ==
                                  std::tolower(static_cast<unsigned char>(b));
                       }) != haystack.end();
}

/// Checks if an item matches the symbol query.
/// Matches: exact name, exact id, path suffix, or case-insensitive name.
static auto item_matches(const doc::DocItem& item, const std::string& query) -> int {
    // Exact id match (highest priority)
    if (item.id == query)
        return 100;
    // Exact name match
    if (item.name == query)
        return 90;
    // Path-qualified match: "HashMap::get" or "std::collections::HashMap"
    if (icontains(item.id, query))
        return 70;
    // Case-insensitive name match
    if (icontains(item.name, query))
        return 50;
    return 0;
}

/// Prints a single DocItem to stdout with ANSI formatting.
static void print_item(const doc::DocItem& item, const std::string& module_path, bool use_color) {
    auto c = [&](const char* code) -> const char* { return use_color ? code : ""; };

    // Header line: kind + id
    std::cout << c(ansi::BOLD) << c(ansi::CYAN) << doc::doc_item_kind_to_string(item.kind)
              << c(ansi::RESET) << " " << c(ansi::BOLD) << item.id << c(ansi::RESET) << "\n";

    // Signature
    if (!item.signature.empty()) {
        std::cout << c(ansi::GREEN) << "  " << item.signature << c(ansi::RESET) << "\n";
    }

    // Module
    if (!module_path.empty()) {
        std::cout << c(ansi::DIM) << "  module: " << module_path << c(ansi::RESET) << "\n";
    }

    // Source location
    if (!item.source_file.empty()) {
        std::cout << c(ansi::DIM) << "  source: " << item.source_file;
        if (item.source_line > 0) {
            std::cout << ":" << item.source_line;
        }
        std::cout << c(ansi::RESET) << "\n";
    }

    std::cout << "\n";

    // Description
    if (!item.doc.empty()) {
        // Print each paragraph with wrapping at 80 chars
        std::istringstream doc_stream(item.doc);
        std::string line;
        while (std::getline(doc_stream, line)) {
            std::cout << "  " << line << "\n";
        }
        std::cout << "\n";
    } else if (!item.summary.empty()) {
        std::cout << "  " << item.summary << "\n\n";
    }

    // Parameters
    if (!item.params.empty() &&
        (item.kind == doc::DocItemKind::Function || item.kind == doc::DocItemKind::Method)) {
        std::cout << c(ansi::YELLOW) << c(ansi::BOLD) << "Parameters:" << c(ansi::RESET) << "\n";
        for (const auto& p : item.params) {
            if (p.name == "this")
                continue;
            std::cout << "  " << c(ansi::BOLD) << p.name << c(ansi::RESET);
            if (!p.type.empty()) {
                std::cout << ": " << c(ansi::BLUE) << p.type << c(ansi::RESET);
            }
            if (!p.description.empty()) {
                std::cout << "\n    " << p.description;
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    // Return value
    if (item.returns) {
        if (!item.returns->type.empty() || !item.returns->description.empty()) {
            std::cout << c(ansi::YELLOW) << c(ansi::BOLD) << "Returns:" << c(ansi::RESET) << "\n";
            if (!item.returns->type.empty()) {
                std::cout << "  " << c(ansi::BLUE) << item.returns->type << c(ansi::RESET);
            }
            if (!item.returns->description.empty()) {
                std::cout << " — " << item.returns->description;
            }
            std::cout << "\n\n";
        }
    }

    // Fields (for structs)
    if (!item.fields.empty()) {
        std::cout << c(ansi::YELLOW) << c(ansi::BOLD) << "Fields:" << c(ansi::RESET) << "\n";
        for (const auto& f : item.fields) {
            std::cout << "  " << c(ansi::BOLD) << f.name << c(ansi::RESET);
            if (!f.signature.empty()) {
                std::cout << ": " << c(ansi::BLUE) << f.signature << c(ansi::RESET);
            }
            if (!f.summary.empty()) {
                std::cout << "  — " << f.summary;
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    // Variants (for enums)
    if (!item.variants.empty()) {
        std::cout << c(ansi::YELLOW) << c(ansi::BOLD) << "Variants:" << c(ansi::RESET) << "\n";
        for (const auto& v : item.variants) {
            std::cout << "  " << c(ansi::BOLD) << v.name << c(ansi::RESET);
            if (!v.summary.empty()) {
                std::cout << "  — " << v.summary;
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    // Methods (brief listing)
    if (!item.methods.empty()) {
        std::cout << c(ansi::YELLOW) << c(ansi::BOLD) << "Methods:" << c(ansi::RESET) << "\n";
        for (const auto& m : item.methods) {
            std::cout << "  " << c(ansi::BOLD) << m.name << c(ansi::RESET);
            if (!m.signature.empty()) {
                std::cout << "  " << c(ansi::DIM) << m.signature << c(ansi::RESET);
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    // Examples
    if (!item.examples.empty()) {
        std::cout << c(ansi::YELLOW) << c(ansi::BOLD) << "Examples:" << c(ansi::RESET) << "\n";
        for (const auto& ex : item.examples) {
            if (!ex.description.empty()) {
                std::cout << "  " << ex.description << "\n";
            }
            std::cout << c(ansi::DIM) << "  ```\n";
            std::istringstream ex_stream(ex.code);
            std::string line;
            while (std::getline(ex_stream, line)) {
                std::cout << "  " << line << "\n";
            }
            std::cout << "  ```" << c(ansi::RESET) << "\n\n";
        }
    }

    // Deprecation
    if (item.deprecated) {
        std::cout << c(ansi::RED) << "[DEPRECATED] " << item.deprecated->message << c(ansi::RESET)
                  << "\n\n";
    }
}

/// Collects all items from an index into a flat list with their module path.
static void collect_all_items(const doc::DocIndex& index,
                              std::vector<std::pair<const doc::DocItem*, std::string>>& out) {
    std::function<void(const std::vector<doc::DocItem>&, const std::string&)> collect_items;
    collect_items = [&](const std::vector<doc::DocItem>& items, const std::string& mod_path) {
        for (const auto& item : items) {
            out.emplace_back(&item, mod_path);
            collect_items(item.fields, mod_path);
            collect_items(item.variants, mod_path);
            collect_items(item.methods, mod_path);
            collect_items(item.associated_types, mod_path);
            collect_items(item.associated_consts, mod_path);
        }
    };
    std::function<void(const std::vector<doc::DocModule>&)> collect_modules;
    collect_modules = [&](const std::vector<doc::DocModule>& mods) {
        for (const auto& mod : mods) {
            collect_items(mod.items, mod.path);
            collect_modules(mod.submodules);
        }
    };
    collect_modules(index.modules);
}

/// Runs the symbol lookup sub-command: tml doc <symbol>
static int run_symbol_lookup(const std::string& symbol, bool use_color) {
    // Find project root and build doc index from lib/
    auto root = find_project_root();
    if (root.empty()) {
        std::cerr << "Could not find TML project root (looking for lib/core/src and lib/std/src)\n";
        return 1;
    }

    if (use_color) {
        std::cerr << ansi::DIM << "Building documentation index from " << root.string() << "..."
                  << ansi::RESET << "\n";
    }

    // Collect and parse all source files
    doc::ExtractorConfig config;
    config.include_private = false;
    config.include_internals = false;
    config.extract_examples = true;
    config.resolve_links = false;
    doc::Extractor extractor(config);

    std::vector<std::pair<parser::Module, std::string>> modules;
    auto core_files = collect_tml_source_files(root / "lib" / "core" / "src");
    auto std_files = collect_tml_source_files(root / "lib" / "std" / "src");

    for (const auto& f : core_files) {
        parse_file_into_index(f, extractor, modules);
    }
    for (const auto& f : std_files) {
        parse_file_into_index(f, extractor, modules);
    }

    if (modules.empty()) {
        std::cerr << "No modules parsed.\n";
        return 1;
    }

    // Build index
    std::vector<std::pair<const parser::Module*, std::string>> module_ptrs;
    for (const auto& [mod, path] : modules) {
        module_ptrs.emplace_back(&mod, path);
    }
    auto doc_index = extractor.extract_all(module_ptrs);
    doc_index.build_index();

    // Try direct id lookup first
    if (const auto* item = doc_index.find_item(symbol)) {
        print_item(*item, item->path, use_color);
        return 0;
    }

    // Collect all items and score them
    std::vector<std::pair<const doc::DocItem*, std::string>> all_items;
    collect_all_items(doc_index, all_items);

    struct Match {
        const doc::DocItem* item;
        std::string module_path;
        int score;
    };
    std::vector<Match> matches;
    for (const auto& [item_ptr, mod_path] : all_items) {
        int score = item_matches(*item_ptr, symbol);
        if (score > 0) {
            matches.push_back({item_ptr, mod_path, score});
        }
    }

    if (matches.empty()) {
        std::cerr << "No results found for '" << symbol << "'\n";
        std::cerr << "Tip: Try a shorter query, e.g. 'List' instead of 'std::collections::List'\n";
        return 1;
    }

    // Sort by score descending, then by name length ascending (prefer shorter names)
    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.item->name.size() < b.item->name.size();
    });

    // Print top results (limit to 5 to avoid overwhelming output)
    size_t limit = matches.size() < 5 ? matches.size() : 5;
    if (matches.size() > 1 && use_color) {
        std::cerr << ansi::DIM << "Found " << matches.size() << " result(s), showing top " << limit
                  << ":" << ansi::RESET << "\n\n";
    }

    for (size_t i = 0; i < limit; ++i) {
        if (limit > 1) {
            std::cout << (use_color ? ansi::DIM : "") << "--- " << (i + 1) << " / " << limit
                      << " ---" << (use_color ? ansi::RESET : "") << "\n\n";
        }
        print_item(*matches[i].item, matches[i].module_path, use_color);
    }

    return 0;
}

// ============================================================================
// Local HTTP Server (tml doc --serve)
// ============================================================================

/// Returns a MIME type string for a file extension.
static auto mime_type(const std::string& ext) -> std::string {
    if (ext == ".html" || ext == ".htm")
        return "text/html; charset=utf-8";
    if (ext == ".css")
        return "text/css";
    if (ext == ".js")
        return "application/javascript";
    if (ext == ".json")
        return "application/json";
    if (ext == ".png")
        return "image/png";
    if (ext == ".svg")
        return "image/svg+xml";
    if (ext == ".ico")
        return "image/x-icon";
    return "application/octet-stream";
}

/// Reads a file into a string. Returns empty string on error.
static auto read_file_bytes(const fs::path& path) -> std::string {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/// Handles a single HTTP request. Reads the request from sock, writes the response.
static void handle_http_request(socket_t sock, const fs::path& serve_root) {
    // Read request headers (up to 8KB)
    std::string request;
    request.reserve(1024);
    char buf[4096];
    while (true) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            break;
        request.append(buf, static_cast<size_t>(n));
        // Stop once we have full headers (ends with \r\n\r\n or \n\n)
        if (request.find("\r\n\r\n") != std::string::npos ||
            request.find("\n\n") != std::string::npos) {
            break;
        }
        if (request.size() > 8192)
            break;
    }

    if (request.empty()) {
        close_sock(sock);
        return;
    }

    // Parse the request line: "GET /path HTTP/1.1"
    std::string url_path = "/";
    {
        auto first_line_end = request.find('\n');
        if (first_line_end != std::string::npos) {
            auto first_line = request.substr(0, first_line_end);
            // Trim \r
            if (!first_line.empty() && first_line.back() == '\r') {
                first_line.pop_back();
            }
            // Extract URL between first space and second space
            auto sp1 = first_line.find(' ');
            if (sp1 != std::string::npos) {
                auto sp2 = first_line.find(' ', sp1 + 1);
                if (sp2 != std::string::npos) {
                    url_path = first_line.substr(sp1 + 1, sp2 - sp1 - 1);
                } else {
                    url_path = first_line.substr(sp1 + 1);
                }
            }
        }
    }

    // Strip query string
    auto q = url_path.find('?');
    if (q != std::string::npos) {
        url_path = url_path.substr(0, q);
    }

    // URL decode basic percent-encoding
    std::string decoded;
    for (size_t i = 0; i < url_path.size(); ++i) {
        if (url_path[i] == '%' && i + 2 < url_path.size()) {
            char hex[3] = {url_path[i + 1], url_path[i + 2], '\0'};
            char* end;
            long val = std::strtol(hex, &end, 16);
            if (end == hex + 2) {
                decoded += static_cast<char>(val);
                i += 2;
                continue;
            }
        }
        decoded += url_path[i];
    }
    url_path = decoded;

    // Prevent path traversal: reject paths containing ".."
    if (url_path.find("..") != std::string::npos) {
        const char* resp = "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
        send(sock, resp, static_cast<int>(strlen(resp)), 0);
        close_sock(sock);
        return;
    }

    // Map "/" to "/index.html"
    if (url_path == "/" || url_path.empty()) {
        url_path = "/index.html";
    }

    // Remove leading slash and build filesystem path (use fs::path to handle separators)
    auto rel = url_path.substr(1);
    auto file_path = serve_root / fs::path(rel);

    // Serve the file
    auto send_response = [&](int status_code, const std::string& status_text,
                             const std::string& content_type, const std::string& body) {
        std::ostringstream response;
        response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        response << "Content-Type: " << content_type << "\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        auto header = response.str();
        send(sock, header.c_str(), static_cast<int>(header.size()), 0);
        if (!body.empty()) {
            send(sock, body.c_str(), static_cast<int>(body.size()), 0);
        }
    };

    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        // Try with .html extension
        auto html_path = file_path;
        html_path.replace_extension(".html");
        if (fs::exists(html_path) && fs::is_regular_file(html_path)) {
            file_path = html_path;
        } else {
            std::string not_found =
                "<html><body><h1>404 Not Found</h1><p>" + url_path + "</p></body></html>";
            send_response(404, "Not Found", "text/html; charset=utf-8", not_found);
            close_sock(sock);
            return;
        }
    }

    auto body = read_file_bytes(file_path);
    auto ext = file_path.extension().string();
    std::string ctype = mime_type(ext);
    send_response(200, "OK", ctype, body);
    close_sock(sock);
}

/// Starts a local HTTP server serving files from serve_root on port.
/// Returns 1 on socket error, otherwise runs until Ctrl-C.
static int run_http_server(const fs::path& serve_root, int port) {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCK) {
        std::cerr << "Failed to create socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Allow address reuse so we can restart quickly
    int reuse = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
               sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port " << port << " — is another process using it?\n";
        close_sock(server_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(server_sock, 10) < 0) {
        std::cerr << "Failed to listen on socket\n";
        close_sock(server_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "Documentation server running at http://localhost:" << port << "/\n";
    std::cout << "Serving from: " << serve_root.string() << "\n";
    std::cout << "Press Ctrl-C to stop.\n";

    // Open browser
    {
        std::string url = "http://localhost:" + std::to_string(port) + "/";
#ifdef _WIN32
        std::string cmd = "start \"\" \"" + url + "\"";
#elif __APPLE__
        std::string cmd = "open \"" + url + "\"";
#else
        std::string cmd = "xdg-open \"" + url + "\"";
#endif
        (void)system(cmd.c_str());
    }

    // Accept loop — runs until killed
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        socket_t client =
            accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client == INVALID_SOCK) {
            continue;
        }
        handle_http_request(client, serve_root);
    }

    close_sock(server_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

/// Emits parser errors using the diagnostic emitter.
static void emit_parser_errors(DiagnosticEmitter& emitter,
                               const std::vector<parser::ParseError>& errors) {
    for (const auto& error : errors) {
        Diagnostic diag;
        diag.severity = DiagnosticSeverity::Error;
        diag.code = error.code.empty() ? "P001" : error.code;
        diag.message = error.message;
        diag.primary_span = error.span;
        diag.notes = error.notes;
        emitter.emit(diag);
    }
}

int run_doc(const DocOptions& options) {
    // ---- Symbol lookup mode: tml doc <symbol> --------------------------------
    if (!options.symbol_lookup.empty()) {
        // Detect whether stdout supports ANSI colors
        bool use_color = true;
#ifdef _WIN32
        // On Windows, enable VT processing if possible
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (hOut == INVALID_HANDLE_VALUE || !GetConsoleMode(hOut, &mode)) {
            use_color = false;
        } else {
            // Try to enable ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(hOut, mode | 0x0004);
        }
#endif
        return run_symbol_lookup(options.symbol_lookup, use_color);
    }

    if (options.input_files.empty() && !options.all_modules) {
        TML_LOG_ERROR("doc", "No input files specified. Usage: tml doc <file.tml> [options] or tml "
                             "doc --all [options]");
        return 1;
    }

    auto& diag = get_diagnostic_emitter();

    // Create output directory
    fs::create_directories(options.output_dir);

    // Extractor configuration
    doc::ExtractorConfig extract_config;
    extract_config.include_private = options.include_private;
    extract_config.include_internals = options.include_internals;
    extract_config.extract_examples = true;
    extract_config.resolve_links = true;

    doc::Extractor extractor(extract_config);

    // Collect all modules to document
    std::vector<std::pair<parser::Module, std::string>> modules;

    // Helper to check if a file should be documented
    auto is_documentable = [](const fs::path& path) -> bool {
        std::string filename = path.filename().string();
        // Skip test files and error test files
        if (filename.ends_with(".test.tml") || filename.ends_with(".error.tml")) {
            return false;
        }
        // Skip files in tests/ or examples/ directories
        std::string path_str = path.string();
        for (auto& c : path_str) {
            if (c == '\\')
                c = '/';
        }
        if (path_str.find("/tests/") != std::string::npos ||
            path_str.find("/examples/") != std::string::npos) {
            return false;
        }
        return path.extension() == ".tml";
    };

    // If --all, find all .tml source files in current directory and lib/
    std::vector<std::string> files = options.input_files;
    if (options.all_modules) {
        // Look for project files
        if (fs::exists("src")) {
            for (const auto& entry : fs::recursive_directory_iterator("src")) {
                if (is_documentable(entry.path())) {
                    files.push_back(entry.path().string());
                }
            }
        }
        if (fs::exists("lib")) {
            for (const auto& entry : fs::recursive_directory_iterator("lib")) {
                if (is_documentable(entry.path())) {
                    files.push_back(entry.path().string());
                }
            }
        }
        // Also check current directory
        for (const auto& entry : fs::directory_iterator(".")) {
            if (is_documentable(entry.path())) {
                files.push_back(entry.path().string());
            }
        }
    }

    if (files.empty()) {
        TML_LOG_ERROR("doc", "No .tml files found");
        return 1;
    }

    // Parse each file
    for (const auto& file : files) {
        TML_LOG_INFO("doc", "Processing: " << file);

        // Read file
        std::string source_code;
        try {
            source_code = read_file(file);
        } catch (const std::exception& e) {
            TML_LOG_ERROR("doc", "Could not read file '" << file << "': " << e.what());
            continue;
        }

        // Run preprocessor to handle #if/#ifdef/#define etc.
        auto pp_config = preprocessor::Preprocessor::host_config();
        preprocessor::Preprocessor pp(pp_config);
        auto pp_result = pp.process(source_code, file);

        if (!pp_result.success()) {
            for (const auto& pp_diag : pp_result.errors()) {
                std::string code = pp_diag.severity == preprocessor::DiagnosticSeverity::Warning
                                       ? "PP002"
                                       : "PP001";
                SourceLocation loc;
                loc.file = file;
                loc.line = static_cast<uint32_t>(pp_diag.line);
                loc.column = static_cast<uint32_t>(pp_diag.column);
                loc.offset = 0;
                loc.length = 0;
                SourceSpan pp_span{loc, loc};
                if (pp_diag.severity == preprocessor::DiagnosticSeverity::Warning) {
                    diag.warning(code, pp_diag.message, pp_span);
                } else {
                    diag.error(code, pp_diag.message, pp_span);
                }
            }
            continue;
        }

        // Use preprocessed source for lexing
        std::string preprocessed = pp_result.output;
        diag.set_source_content(file, preprocessed);

        // Lex
        auto source = lexer::Source::from_string(preprocessed, file);
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();

        if (lex.has_errors()) {
            for (const auto& err : lex.errors()) {
                diag.error(err.code.empty() ? "L001" : err.code, err.message, err.span);
            }
            continue;
        }

        // Parse
        parser::Parser parser(std::move(tokens));
        auto module_name = fs::path(file).stem().string();
        auto parse_result = parser.parse_module(module_name);

        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
            const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
            emit_parser_errors(diag, errors);
            continue;
        }

        auto& module = std::get<parser::Module>(parse_result);

        // Derive module path from file path
        std::string module_path = module_name;
        auto rel_path = fs::path(file).parent_path();
        if (!rel_path.empty() && rel_path != ".") {
            // Convert path separators to ::
            std::string path_str = rel_path.string();
            for (auto& c : path_str) {
                if (c == '/' || c == '\\') {
                    c = ':';
                }
            }
            // Remove consecutive colons and make proper path
            std::string clean_path;
            bool last_was_colon = false;
            for (char c : path_str) {
                if (c == ':') {
                    if (!last_was_colon && !clean_path.empty()) {
                        clean_path += "::";
                    }
                    last_was_colon = true;
                } else {
                    clean_path += c;
                    last_was_colon = false;
                }
            }
            if (!clean_path.empty()) {
                module_path = clean_path + "::" + module_name;
            }
        }

        modules.emplace_back(std::move(module), module_path);
    }

    if (modules.empty()) {
        TML_LOG_ERROR("doc", "No modules parsed successfully");
        return 1;
    }

    // Build module pointers for extraction
    std::vector<std::pair<const parser::Module*, std::string>> module_ptrs;
    for (const auto& [module, path] : modules) {
        module_ptrs.emplace_back(&module, path);
    }

    // Extract documentation
    auto doc_index = extractor.extract_all(module_ptrs);
    doc_index.crate_name = "TML Project";
    doc_index.version = "0.1.0";

    // Generate output based on format
    doc::GeneratorConfig gen_config;
    gen_config.title = doc_index.crate_name;
    gen_config.version = doc_index.version;
    gen_config.include_private = options.include_private;

    switch (options.format) {
    case DocFormat::Json: {
        doc::JsonGenerator generator(gen_config);
        fs::path output_file = fs::path(options.output_dir) / "docs.json";
        generator.generate_file(doc_index, output_file);
        TML_LOG_INFO("doc", "Generated: " << output_file);
        TML_LOG_INFO("doc", "Documentation written to " << output_file);
        break;
    }

    case DocFormat::Html: {
        doc::HtmlGenerator generator(gen_config);
        generator.generate_site(doc_index, options.output_dir);
        TML_LOG_INFO("doc", "Generated HTML documentation in " << options.output_dir);
        fs::path index_file = fs::path(options.output_dir) / "index.html";
        TML_LOG_INFO("doc", "Documentation written to " << index_file);

        // Open in browser if requested (and not serving — serve opens it itself)
        if (options.open_browser && !options.serve) {
#ifdef _WIN32
            std::string cmd = "start \"\" \"" + index_file.string() + "\"";
#elif __APPLE__
            std::string cmd = "open \"" + index_file.string() + "\"";
#else
            std::string cmd = "xdg-open \"" + index_file.string() + "\"";
#endif
            (void)system(cmd.c_str());
        }

        // Start local HTTP server if requested
        if (options.serve) {
            return run_http_server(fs::absolute(fs::path(options.output_dir)), options.serve_port);
        }
        break;
    }

    case DocFormat::Markdown: {
        doc::MarkdownGenerator generator(gen_config);
        generator.generate_directory(doc_index, options.output_dir);
        TML_LOG_INFO("doc", "Generated Markdown documentation in " << options.output_dir);
        fs::path index_file = fs::path(options.output_dir) / "README.md";
        TML_LOG_INFO("doc", "Documentation written to " << index_file);
        break;
    }
    }

    // Summary
    size_t total_items = 0;
    for (const auto& module : doc_index.modules) {
        total_items += module.items.size();
    }
    TML_LOG_INFO("doc", "Documented " << doc_index.modules.size() << " modules, " << total_items
                                      << " items");

    return 0;
}

DocOptions parse_doc_args(int argc, char* argv[]) {
    DocOptions options;

    // Track whether we have seen any .tml file argument — if the first non-flag
    // arg doesn't end in .tml (and has no path separator), treat it as a symbol.
    bool symbol_candidate_seen = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_doc_help();
            exit(0);
        } else if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (arg == "--all") {
            options.all_modules = true;
        } else if (arg == "--include-private") {
            options.include_private = true;
        } else if (arg == "--include-internals") {
            options.include_internals = true;
        } else if (arg == "--open") {
            options.open_browser = true;
        } else if (arg == "--serve") {
            options.serve = true;
        } else if (arg.starts_with("--port=")) {
            try {
                options.serve_port = std::stoi(arg.substr(7));
            } catch (...) {
                TML_LOG_WARN("doc", "Invalid port '" << arg.substr(7) << "', using 8080");
                options.serve_port = 8080;
            }
        } else if (arg.starts_with("--format=")) {
            std::string format = arg.substr(9);
            if (format == "json") {
                options.format = DocFormat::Json;
            } else if (format == "html") {
                options.format = DocFormat::Html;
            } else if (format == "md" || format == "markdown") {
                options.format = DocFormat::Markdown;
            } else {
                TML_LOG_WARN("doc", "Unknown format '" << format << "', using html");
                options.format = DocFormat::Html;
            }
        } else if (arg.starts_with("--output=") || arg.starts_with("-o=")) {
            options.output_dir = arg.substr(arg.find('=') + 1);
        } else if (arg[0] != '-') {
            // Distinguish file arguments from symbol names.
            // A .tml file ends in ".tml"; a symbol name does not.
            bool looks_like_file = (arg.size() > 4 && arg.substr(arg.size() - 4) == ".tml") ||
                                   arg.find('/') != std::string::npos ||
                                   arg.find('\\') != std::string::npos;
            if (looks_like_file) {
                options.input_files.push_back(arg);
            } else if (!symbol_candidate_seen && options.input_files.empty()) {
                // First non-flag, non-file argument is the symbol to look up
                options.symbol_lookup = arg;
                symbol_candidate_seen = true;
            } else {
                // Additional positional args after a symbol are treated as files
                options.input_files.push_back(arg);
            }
        } else {
            TML_LOG_WARN("doc", "Unknown option '" << arg << "'");
        }
    }

    // If --serve is set without explicit format, ensure we produce HTML
    if (options.serve && options.format != DocFormat::Html) {
        TML_LOG_WARN("doc", "--serve requires HTML output, switching to --format=html");
        options.format = DocFormat::Html;
    }

    return options;
}

void print_doc_help() {
    std::cerr << R"(
TML Documentation Generator

Usage: tml doc [file.tml...] [options]
       tml doc --all [options]
       tml doc <symbol>               # Terminal symbol lookup
       tml doc --serve [options]      # Generate HTML and serve locally

Symbol Lookup:
  tml doc List                        # Look up 'List' in the standard library
  tml doc HashMap::get                # Look up a specific method
  tml doc core::str::split            # Look up by full qualified path

Doc Generation:
  --all               Document all .tml files in project
  --format=<fmt>      Output format: json, html, md (default: html)
  --output=<dir>      Output directory (default: docs)
  -o=<dir>            Alias for --output
  --include-private   Include private (non-pub) items
  --include-internals Include items marked @internal
  --open              Open documentation in browser after generation
  --serve             Generate HTML docs and start a local HTTP server
  --port=<n>          Port for --serve (default: 8080)
  --verbose, -v       Show detailed output
  --help, -h          Show this help

Examples:
  tml doc main.tml                    # Document single file
  tml doc src/*.tml --format=json     # Output as JSON
  tml doc --all --open                # Document project and open in browser
  tml doc lib/core.tml -o=api-docs    # Custom output directory
  tml doc --all --serve               # Serve docs at http://localhost:8080/
  tml doc --all --serve --port=9000   # Serve on a custom port

Output Formats:
  html     - Interactive HTML website with search
  json     - Machine-readable JSON for tooling
  md       - Markdown files for wikis/READMEs

)";
}

} // namespace tml::cli
