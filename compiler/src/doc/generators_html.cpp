//! # HTML Documentation Generator Implementation
//!
//! This file implements the HtmlGenerator class for producing HTML documentation
//! sites. It is split from generators.cpp due to file size.

#include "doc/generators.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

namespace tml::doc {

HtmlGenerator::HtmlGenerator(GeneratorConfig config) : config_(std::move(config)) {}

void HtmlGenerator::generate_site(const DocIndex& index, const std::filesystem::path& output_dir) {
    std::filesystem::create_directories(output_dir);

    // Create pages subdirectory for module pages
    auto pages_dir = output_dir / "pages";
    std::filesystem::create_directories(pages_dir);

    // Organize modules by library, filtering out tests and mod files
    auto libraries = organize_by_library(index, config_.include_tests);

    // Generate index page (in root)
    {
        std::ofstream out(output_dir / "index.html");
        if (out) {
            generate_index_page_with_libraries(libraries, index, out);
        }
    }

    // Generate module pages (in /pages subdirectory) - only for filtered modules
    for (const auto& lib : libraries) {
        for (const auto* module : lib.modules) {
            std::string filename = module->name + ".html";
            for (auto& c : filename) {
                if (c == ':') {
                    c = '_';
                }
            }
            std::ofstream out(pages_dir / filename);
            if (out) {
                // Generate module page with full navigation and correct asset paths
                write_head(module->name, out, "../");

                out << R"(<body>
<div class="layout">
)";

                // Sidebar with full module navigation organized by library
                write_module_sidebar_with_libraries(module->name, *module, libraries, out, "../");

                // Main content
                out << R"(<main class="main-content">
<div class="page-header">
    <h1 class="page-title">)"
                    << escape_html(module->name) << R"(</h1>
)";
                if (!module->doc.empty()) {
                    out << "<p class=\"page-description\">" << escape_html(module->summary)
                        << "</p>\n";
                }
                out << "</div>\n";

                // Group items by kind for better organization
                std::vector<const DocItem*> structs, enums, traits, functions, methods, constants,
                    type_aliases;
                for (const auto& item : module->items) {
                    switch (item.kind) {
                    case DocItemKind::Struct:
                        structs.push_back(&item);
                        break;
                    case DocItemKind::Enum:
                        enums.push_back(&item);
                        break;
                    case DocItemKind::Trait:
                        traits.push_back(&item);
                        break;
                    case DocItemKind::Function:
                        functions.push_back(&item);
                        break;
                    case DocItemKind::Method:
                        methods.push_back(&item);
                        break;
                    case DocItemKind::Constant:
                        constants.push_back(&item);
                        break;
                    case DocItemKind::TypeAlias:
                        type_aliases.push_back(&item);
                        break;
                    default:
                        break;
                    }
                }

                // Structs
                if (!structs.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Structs</h2>\n";
                    for (const auto* item : structs) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Enums
                if (!enums.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Enums</h2>\n";
                    for (const auto* item : enums) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Behaviors
                if (!traits.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Behaviors</h2>\n";
                    for (const auto* item : traits) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Functions
                if (!functions.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Functions</h2>\n";
                    for (const auto* item : functions) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Constants
                if (!constants.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Constants</h2>\n";
                    for (const auto* item : constants) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Type Aliases
                if (!type_aliases.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Type Aliases</h2>\n";
                    for (const auto* item : type_aliases) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                write_footer(out);
                out << R"(</main>
</div>
<button class="mobile-toggle" aria-label="Toggle menu">☰</button>
)";
                write_scripts(out, "../");
                out << R"(</body>
</html>)";
            }
        }
    }

    // Generate search index (in root) - also filter
    {
        std::ofstream out(output_dir / "search-index.js");
        if (out) {
            generate_search_index_filtered(libraries, out);
        }
    }

    // Generate CSS (in root)
    {
        std::ofstream out(output_dir / "style.css");
        if (out) {
            write_css(out);
        }
    }

    // Generate search JavaScript (in root)
    {
        std::ofstream out(output_dir / "search.js");
        if (out) {
            write_search_js(out);
        }
    }
}

void HtmlGenerator::write_css(std::ostream& out) {
    out << R"(/* TML Documentation - Modern Dark Theme */
:root {
    --bg-primary: #0f1419;
    --bg-secondary: #1a1f25;
    --bg-tertiary: #242a32;
    --bg-hover: #2d353f;
    --text-primary: #e6e6e6;
    --text-secondary: #9ca3af;
    --text-muted: #6b7280;
    --accent-primary: #4fc3f7;
    --accent-secondary: #81d4fa;
    --accent-green: #4ade80;
    --accent-yellow: #fbbf24;
    --accent-purple: #a78bfa;
    --accent-pink: #f472b6;
    --accent-orange: #fb923c;
    --border-color: #374151;
    --code-bg: #1e252e;
    --search-bg: #1a1f25;
    --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.3);
    --font-sans: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    --font-mono: 'JetBrains Mono', 'Fira Code', 'Cascadia Code', Consolas, monospace;
}

* { box-sizing: border-box; margin: 0; padding: 0; }

html { scroll-behavior: smooth; }

body {
    font-family: var(--font-sans);
    background: var(--bg-primary);
    color: var(--text-primary);
    line-height: 1.7;
    min-height: 100vh;
}

/* Layout */
.layout {
    display: grid;
    grid-template-columns: 280px 1fr;
    min-height: 100vh;
}

@media (max-width: 900px) {
    .layout {
        grid-template-columns: 1fr;
    }
    .sidebar {
        position: fixed;
        left: -300px;
        transition: left 0.3s ease;
        z-index: 1000;
    }
    .sidebar.open { left: 0; }
    .mobile-toggle { display: flex !important; }
}

/* Sidebar */
.sidebar {
    background: var(--bg-secondary);
    border-right: 1px solid var(--border-color);
    height: 100vh;
    position: sticky;
    top: 0;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
}

.sidebar-header {
    padding: 20px;
    border-bottom: 1px solid var(--border-color);
    background: var(--bg-tertiary);
}

.logo {
    display: flex;
    align-items: center;
    gap: 12px;
    text-decoration: none;
    color: var(--text-primary);
    font-weight: 700;
    font-size: 1.25rem;
}

.logo-icon {
    width: 32px;
    height: 32px;
    background: linear-gradient(135deg, var(--accent-primary), var(--accent-purple));
    border-radius: 8px;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: 800;
    font-size: 14px;
}

.version-badge {
    font-size: 0.7rem;
    background: var(--accent-primary);
    color: var(--bg-primary);
    padding: 2px 8px;
    border-radius: 12px;
    font-weight: 600;
}

/* Search */
.search-container {
    padding: 16px 20px;
    border-bottom: 1px solid var(--border-color);
}

.search-box {
    position: relative;
    width: 100%;
}

.search-input {
    width: 100%;
    padding: 10px 16px 10px 40px;
    background: var(--bg-primary);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    color: var(--text-primary);
    font-size: 0.9rem;
    font-family: var(--font-sans);
    transition: all 0.2s ease;
}

.search-input:focus {
    outline: none;
    border-color: var(--accent-primary);
    box-shadow: 0 0 0 3px rgba(79, 195, 247, 0.15);
}

.search-input::placeholder {
    color: var(--text-muted);
}

.search-icon {
    position: absolute;
    left: 12px;
    top: 50%;
    transform: translateY(-50%);
    color: var(--text-muted);
    width: 18px;
    height: 18px;
}

.search-shortcut {
    position: absolute;
    right: 12px;
    top: 50%;
    transform: translateY(-50%);
    background: var(--bg-tertiary);
    color: var(--text-muted);
    padding: 2px 8px;
    border-radius: 4px;
    font-size: 0.75rem;
    font-family: var(--font-mono);
    border: 1px solid var(--border-color);
}

/* Search Results Dropdown */
.search-results {
    position: absolute;
    top: 100%;
    left: 0;
    right: 0;
    margin-top: 8px;
    background: var(--bg-secondary);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    box-shadow: var(--shadow);
    max-height: 400px;
    overflow-y: auto;
    z-index: 1000;
    display: none;
}

.search-results.active { display: block; }

.search-result-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 16px;
    text-decoration: none;
    color: var(--text-primary);
    border-bottom: 1px solid var(--border-color);
    transition: background 0.15s ease;
}

.search-result-item:last-child { border-bottom: none; }
.search-result-item:hover,
.search-result-item.selected { background: var(--bg-hover); }

.result-kind {
    font-size: 0.7rem;
    font-weight: 600;
    padding: 3px 8px;
    border-radius: 4px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    flex-shrink: 0;
}

.result-kind.function { background: var(--accent-primary); color: var(--bg-primary); }
.result-kind.struct { background: var(--accent-green); color: var(--bg-primary); }
.result-kind.enum { background: var(--accent-yellow); color: var(--bg-primary); }
.result-kind.behavior { background: var(--accent-purple); color: var(--bg-primary); }
.result-kind.method { background: var(--accent-pink); color: var(--bg-primary); }
.result-kind.constant { background: var(--accent-orange); color: var(--bg-primary); }

.result-info { flex: 1; min-width: 0; }
.result-name { font-weight: 600; font-family: var(--font-mono); font-size: 0.9rem; }
.result-path { font-size: 0.8rem; color: var(--text-muted); margin-top: 2px; }

.search-empty {
    padding: 24px;
    text-align: center;
    color: var(--text-muted);
}

/* Navigation */
.nav-section {
    padding: 16px 20px;
    flex: 1;
    overflow-y: auto;
}

.nav-title {
    font-size: 0.7rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: var(--text-muted);
    margin-bottom: 12px;
}

.nav-list { list-style: none; }

.nav-list-collapsed {
    max-height: 200px;
    overflow: hidden;
    position: relative;
}

.nav-list-collapsed::after {
    content: '';
    position: absolute;
    bottom: 0;
    left: 0;
    right: 0;
    height: 40px;
    background: linear-gradient(transparent, var(--bg-secondary));
    pointer-events: none;
}

.nav-list-collapsed.expanded {
    max-height: none;
    overflow: visible;
}

.nav-list-collapsed.expanded::after {
    display: none;
}

.nav-toggle {
    display: block;
    width: 100%;
    padding: 8px 12px;
    margin-top: 8px;
    background: var(--bg-tertiary);
    border: 1px solid var(--border-color);
    border-radius: 6px;
    color: var(--text-secondary);
    font-size: 0.8rem;
    cursor: pointer;
    transition: all 0.15s ease;
}

.nav-toggle:hover {
    background: var(--bg-hover);
    color: var(--text-primary);
}

.nav-item {
    margin-bottom: 4px;
}

.nav-item.active .nav-link {
    background: rgba(79, 195, 247, 0.15);
    color: var(--accent-primary);
    border-left: 3px solid var(--accent-primary);
    margin-left: -3px;
}

.nav-link {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 8px 12px;
    color: var(--text-secondary);
    text-decoration: none;
    border-radius: 6px;
    font-size: 0.9rem;
    transition: all 0.15s ease;
}

.nav-link:hover {
    background: var(--bg-hover);
    color: var(--text-primary);
}

.nav-link.active {
    background: var(--accent-primary);
    color: var(--bg-primary);
    font-weight: 600;
}

.nav-icon {
    width: 8px;
    height: 8px;
    border-radius: 2px;
    flex-shrink: 0;
}

.nav-icon.module { background: var(--accent-primary); }
.nav-icon.struct { background: var(--accent-green); }
.nav-icon.enum { background: var(--accent-yellow); }
.nav-icon.function { background: var(--accent-purple); }
.nav-icon.behavior { background: var(--accent-pink); }

/* Main Content */
.main-content {
    padding: 40px 60px;
    max-width: 1000px;
}

@media (max-width: 900px) {
    .main-content { padding: 20px; }
}

.mobile-toggle {
    display: none;
    position: fixed;
    bottom: 20px;
    right: 20px;
    width: 50px;
    height: 50px;
    background: var(--accent-primary);
    border: none;
    border-radius: 50%;
    color: var(--bg-primary);
    cursor: pointer;
    box-shadow: var(--shadow);
    align-items: center;
    justify-content: center;
    z-index: 999;
}

/* Page Header */
.page-header {
    margin-bottom: 40px;
    padding-bottom: 24px;
    border-bottom: 1px solid var(--border-color);
}

.page-title {
    font-size: 2.5rem;
    font-weight: 800;
    margin-bottom: 8px;
    background: linear-gradient(135deg, var(--accent-primary), var(--accent-purple));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
}

.page-description {
    font-size: 1.1rem;
    color: var(--text-secondary);
    line-height: 1.8;
}

/* Item Cards */
.item-section {
    margin-bottom: 48px;
}

.section-title {
    font-size: 1.4rem;
    font-weight: 700;
    margin-bottom: 20px;
    color: var(--text-primary);
    display: flex;
    align-items: center;
    gap: 12px;
}

.section-title::before {
    content: '';
    width: 4px;
    height: 24px;
    background: var(--accent-primary);
    border-radius: 2px;
}

.item-card {
    background: var(--bg-secondary);
    border: 1px solid var(--border-color);
    border-radius: 12px;
    padding: 24px;
    margin-bottom: 16px;
    transition: all 0.2s ease;
}

.item-card:hover {
    border-color: var(--accent-primary);
    box-shadow: 0 0 0 1px var(--accent-primary);
}

.item-header {
    display: flex;
    align-items: flex-start;
    gap: 16px;
    margin-bottom: 16px;
}

.item-kind-badge {
    font-size: 0.7rem;
    font-weight: 700;
    padding: 4px 10px;
    border-radius: 6px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    flex-shrink: 0;
}

.item-kind-badge.function { background: rgba(79, 195, 247, 0.15); color: var(--accent-primary); }
.item-kind-badge.struct { background: rgba(74, 222, 128, 0.15); color: var(--accent-green); }
.item-kind-badge.enum { background: rgba(251, 191, 36, 0.15); color: var(--accent-yellow); }
.item-kind-badge.behavior { background: rgba(167, 139, 250, 0.15); color: var(--accent-purple); }
.item-kind-badge.method { background: rgba(244, 114, 182, 0.15); color: var(--accent-pink); }
.item-kind-badge.constant { background: rgba(251, 146, 60, 0.15); color: var(--accent-orange); }
.item-kind-badge.field { background: rgba(156, 163, 175, 0.15); color: var(--text-secondary); }

.item-name {
    font-size: 1.2rem;
    font-weight: 700;
    font-family: var(--font-mono);
    color: var(--text-primary);
}

.item-name a {
    color: inherit;
    text-decoration: none;
}

.item-name a:hover { color: var(--accent-primary); }

/* Signature */
.signature {
    background: var(--code-bg);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    padding: 16px 20px;
    margin-bottom: 16px;
    overflow-x: auto;
    font-family: var(--font-mono);
    font-size: 0.9rem;
    line-height: 1.6;
}

.sig-keyword { color: var(--accent-purple); font-weight: 600; }
.sig-name { color: var(--accent-primary); }
.sig-type { color: var(--accent-green); }
.sig-param { color: var(--accent-yellow); }
.sig-punct { color: var(--text-muted); }

/* Description */
.item-description {
    color: var(--text-secondary);
    line-height: 1.8;
}

.item-description p { margin-bottom: 12px; }
.item-description code {
    background: var(--code-bg);
    padding: 2px 6px;
    border-radius: 4px;
    font-family: var(--font-mono);
    font-size: 0.85em;
    color: var(--accent-primary);
}

/* Deprecated Warning */
.deprecated-warning {
    background: rgba(251, 146, 60, 0.1);
    border: 1px solid var(--accent-orange);
    border-radius: 8px;
    padding: 12px 16px;
    margin-bottom: 16px;
    display: flex;
    align-items: center;
    gap: 12px;
}

.deprecated-warning::before {
    content: '⚠';
    font-size: 1.2rem;
}

.deprecated-warning strong {
    color: var(--accent-orange);
}

/* Parameters & Returns */
.params-section, .returns-section {
    margin-top: 20px;
}

.params-title, .returns-title {
    font-size: 0.9rem;
    font-weight: 700;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.5px;
    margin-bottom: 12px;
}

.params-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.9rem;
}

.params-table th,
.params-table td {
    padding: 12px 16px;
    text-align: left;
    border-bottom: 1px solid var(--border-color);
}

.params-table th {
    background: var(--bg-tertiary);
    font-weight: 600;
    color: var(--text-muted);
    text-transform: uppercase;
    font-size: 0.75rem;
    letter-spacing: 0.5px;
}

.params-table tr:last-child td { border-bottom: none; }
.params-table tr:hover td { background: var(--bg-hover); }

.param-name {
    font-family: var(--font-mono);
    color: var(--accent-yellow);
}

.param-type {
    font-family: var(--font-mono);
    color: var(--accent-green);
}

/* Methods Section */
.methods-section {
    margin-top: 24px;
    padding-top: 24px;
    border-top: 1px solid var(--border-color);
}

.methods-title {
    font-size: 1rem;
    font-weight: 700;
    margin-bottom: 16px;
    color: var(--text-primary);
}

.method-item {
    background: var(--bg-tertiary);
    border-radius: 8px;
    padding: 16px;
    margin-bottom: 12px;
}

.method-name {
    font-family: var(--font-mono);
    font-weight: 600;
    color: var(--accent-pink);
    margin-bottom: 8px;
}

.method-sig {
    font-family: var(--font-mono);
    font-size: 0.85rem;
    color: var(--text-secondary);
    background: var(--code-bg);
    padding: 8px 12px;
    border-radius: 4px;
    overflow-x: auto;
}

.method-desc {
    margin-top: 8px;
    color: var(--text-muted);
    font-size: 0.9rem;
}

/* Module List */
.module-list {
    display: grid;
    gap: 16px;
}

.module-card {
    background: var(--bg-secondary);
    border: 1px solid var(--border-color);
    border-radius: 12px;
    padding: 20px 24px;
    text-decoration: none;
    transition: all 0.2s ease;
}

.module-card:hover {
    border-color: var(--accent-primary);
    transform: translateY(-2px);
    box-shadow: var(--shadow);
}

.module-name {
    font-size: 1.1rem;
    font-weight: 700;
    font-family: var(--font-mono);
    color: var(--accent-primary);
    margin-bottom: 6px;
}

.module-summary {
    color: var(--text-secondary);
    font-size: 0.9rem;
}

/* Footer */
.footer {
    margin-top: 60px;
    padding: 24px 0;
    border-top: 1px solid var(--border-color);
    text-align: center;
    color: var(--text-muted);
    font-size: 0.85rem;
}

.footer a {
    color: var(--accent-primary);
    text-decoration: none;
}

.footer a:hover { text-decoration: underline; }

/* Scrollbar */
::-webkit-scrollbar { width: 8px; height: 8px; }
::-webkit-scrollbar-track { background: var(--bg-primary); }
::-webkit-scrollbar-thumb {
    background: var(--border-color);
    border-radius: 4px;
}
::-webkit-scrollbar-thumb:hover { background: var(--text-muted); }

/* Code Blocks */
pre {
    background: var(--code-bg);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    padding: 16px 20px;
    overflow-x: auto;
    font-family: var(--font-mono);
    font-size: 0.9rem;
    line-height: 1.6;
    margin: 16px 0;
}

code {
    font-family: var(--font-mono);
}

/* Examples */
.examples-section {
    margin-top: 20px;
}

.example-block {
    margin-bottom: 16px;
}

.example-title {
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--text-muted);
    margin-bottom: 8px;
}

/* Animations */
@keyframes fadeIn {
    from { opacity: 0; transform: translateY(10px); }
    to { opacity: 1; transform: translateY(0); }
}

.item-card { animation: fadeIn 0.3s ease; }

/* Focus States */
:focus-visible {
    outline: 2px solid var(--accent-primary);
    outline-offset: 2px;
}
)";
}

void HtmlGenerator::write_search_js(std::ostream& out) {
    out << R"(// TML Documentation Search
(function() {
    const searchInput = document.getElementById('search-input');
    const searchResults = document.getElementById('search-results');
    let selectedIndex = -1;
    let currentResults = [];

    if (!searchInput || !searchResults || !window.searchIndex) return;

    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    function getKindClass(kind) {
        const kindMap = {
            'function': 'function',
            'method': 'method',
            'struct': 'struct',
            'enum': 'enum',
            'behavior': 'behavior',
            'trait': 'behavior',
            'constant': 'constant',
            'field': 'field'
        };
        return kindMap[kind] || 'function';
    }

    function search(query) {
        if (!query.trim()) {
            searchResults.classList.remove('active');
            return [];
        }

        const q = query.toLowerCase();
        const results = window.searchIndex.filter(item => {
            const name = (item.name || '').toLowerCase();
            const path = (item.path || '').toLowerCase();
            return name.includes(q) || path.includes(q);
        }).slice(0, 15);

        return results;
    }

    function renderResults(results) {
        if (results.length === 0) {
            searchResults.innerHTML = '<div class="search-empty">No results found</div>';
            searchResults.classList.add('active');
            return;
        }

        searchResults.innerHTML = results.map((item, index) => `
            <a href="${item.module || 'index'}.html#${item.id || item.name}"
               class="search-result-item ${index === selectedIndex ? 'selected' : ''}"
               data-index="${index}">
                <span class="result-kind ${getKindClass(item.kind)}">${escapeHtml(item.kind)}</span>
                <div class="result-info">
                    <div class="result-name">${escapeHtml(item.name)}</div>
                    <div class="result-path">${escapeHtml(item.path || '')}</div>
                </div>
            </a>
        `).join('');
        searchResults.classList.add('active');
    }

    function updateSelection() {
        const items = searchResults.querySelectorAll('.search-result-item');
        items.forEach((item, index) => {
            item.classList.toggle('selected', index === selectedIndex);
        });
        if (selectedIndex >= 0 && items[selectedIndex]) {
            items[selectedIndex].scrollIntoView({ block: 'nearest' });
        }
    }

    searchInput.addEventListener('input', (e) => {
        selectedIndex = -1;
        currentResults = search(e.target.value);
        renderResults(currentResults);
    });

    searchInput.addEventListener('keydown', (e) => {
        const items = searchResults.querySelectorAll('.search-result-item');

        if (e.key === 'ArrowDown') {
            e.preventDefault();
            selectedIndex = Math.min(selectedIndex + 1, items.length - 1);
            updateSelection();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            selectedIndex = Math.max(selectedIndex - 1, -1);
            updateSelection();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (selectedIndex >= 0 && items[selectedIndex]) {
                items[selectedIndex].click();
            }
        } else if (e.key === 'Escape') {
            searchResults.classList.remove('active');
            searchInput.blur();
        }
    });

    // Global shortcut: / to focus search
    document.addEventListener('keydown', (e) => {
        if (e.key === '/' && document.activeElement !== searchInput) {
            e.preventDefault();
            searchInput.focus();
        }
    });

    // Close on outside click
    document.addEventListener('click', (e) => {
        if (!searchInput.contains(e.target) && !searchResults.contains(e.target)) {
            searchResults.classList.remove('active');
        }
    });

    // Mobile toggle
    const mobileToggle = document.querySelector('.mobile-toggle');
    const sidebar = document.querySelector('.sidebar');
    if (mobileToggle && sidebar) {
        mobileToggle.addEventListener('click', () => {
            sidebar.classList.toggle('open');
        });
    }
})();

// Toggle modules list expand/collapse
function toggleModulesList() {
    const list = document.getElementById('modules-list');
    const btn = list.parentElement.querySelector('.nav-toggle');
    if (list && btn) {
        list.classList.toggle('expanded');
        btn.textContent = list.classList.contains('expanded') ? 'Show less' : 'Show all modules';
    }
}
)";
}

void HtmlGenerator::generate_module_page(const DocModule& module, std::ostream& out) {
    // Standalone module page (without full site navigation)
    // For full site generation with navigation, use generate_site()
    write_head(module.name, out, "");

    out << R"(<body>
<div class="layout">
<main class="main-content" style="margin-left: 0;">
<div class="page-header">
    <h1 class="page-title">)"
        << escape_html(module.name) << R"(</h1>
)";
    if (!module.doc.empty()) {
        out << "<p class=\"page-description\">" << escape_html(module.summary) << "</p>\n";
    }
    out << "</div>\n";

    // Group items by kind for better organization
    std::vector<const DocItem*> structs, enums, traits, functions, methods, constants, type_aliases;
    for (const auto& item : module.items) {
        switch (item.kind) {
        case DocItemKind::Struct:
            structs.push_back(&item);
            break;
        case DocItemKind::Enum:
            enums.push_back(&item);
            break;
        case DocItemKind::Trait:
            traits.push_back(&item);
            break;
        case DocItemKind::Function:
            functions.push_back(&item);
            break;
        case DocItemKind::Method:
            methods.push_back(&item);
            break;
        case DocItemKind::Constant:
            constants.push_back(&item);
            break;
        case DocItemKind::TypeAlias:
            type_aliases.push_back(&item);
            break;
        default:
            break;
        }
    }

    // Structs
    if (!structs.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Structs</h2>\n";
        for (const auto* item : structs) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Enums
    if (!enums.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Enums</h2>\n";
        for (const auto* item : enums) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Behaviors
    if (!traits.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Behaviors</h2>\n";
        for (const auto* item : traits) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Functions
    if (!functions.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Functions</h2>\n";
        for (const auto* item : functions) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Constants
    if (!constants.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Constants</h2>\n";
        for (const auto* item : constants) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Type Aliases
    if (!type_aliases.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Type Aliases</h2>\n";
        for (const auto* item : type_aliases) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    write_footer(out);
    out << R"(</main>
</div>
</body>
</html>)";
}

void HtmlGenerator::generate_item_html(const DocItem& item, std::ostream& out) {
    out << "<div class=\"item\">\n";
    write_signature_html(item, out);

    if (!item.summary.empty()) {
        out << "<p>" << escape_html(item.summary) << "</p>\n";
    }

    out << "</div>\n";
}

void HtmlGenerator::generate_index_page(const DocIndex& index, std::ostream& out) {
    write_head(index.crate_name, out, "");

    out << R"(<body>
<div class="layout">
)";

    // Sidebar
    write_sidebar_index(index, out);

    // Main content
    out << R"(<main class="main-content">
<div class="page-header">
    <h1 class="page-title">)"
        << escape_html(index.crate_name) << R"(</h1>
)";
    if (!index.description.empty()) {
        out << "<p class=\"page-description\">" << escape_html(index.description) << "</p>\n";
    }
    out << "</div>\n";

    // Modules section
    out << R"(<section class="item-section">
<h2 class="section-title">Modules</h2>
<div class="module-list">
)";

    for (const auto& module : index.modules) {
        std::string filename = "pages/" + module.name + ".html";
        for (auto& c : filename) {
            if (c == ':') {
                c = '_';
            }
        }
        out << "<a href=\"" << filename << "\" class=\"module-card\">\n";
        out << "  <div class=\"module-name\">" << escape_html(module.name) << "</div>\n";
        if (!module.summary.empty()) {
            out << "  <div class=\"module-summary\">" << escape_html(module.summary) << "</div>\n";
        }
        out << "</a>\n";
    }

    out << R"(</div>
</section>
)";

    write_footer(out);
    out << R"(</main>
</div>
<button class="mobile-toggle" aria-label="Toggle menu">☰</button>
)";
    write_scripts(out, "");
    out << R"(</body>
</html>)";
}

void HtmlGenerator::generate_index_page_with_libraries(const std::vector<DocLibrary>& libraries,
                                                       const DocIndex& index, std::ostream& out) {
    write_head(index.crate_name, out, "");

    out << R"(<body>
<div class="layout">
)";

    // Sidebar with libraries
    write_sidebar_index_with_libraries(libraries, out);

    // Main content
    out << R"(<main class="main-content">
<div class="page-header">
    <h1 class="page-title">)"
        << escape_html(index.crate_name) << R"(</h1>
)";
    if (!index.description.empty()) {
        out << "<p class=\"page-description\">" << escape_html(index.description) << "</p>\n";
    }
    out << "</div>\n";

    // Libraries section - each library is a separate section
    for (const auto& lib : libraries) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">" << escape_html(lib.name) << "</h2>\n";
        if (!lib.description.empty()) {
            out << "<p class=\"library-description\">" << escape_html(lib.description) << "</p>\n";
        }
        out << "<div class=\"module-list\">\n";

        for (const auto* module : lib.modules) {
            std::string filename = "pages/" + module->name + ".html";
            for (auto& c : filename) {
                if (c == ':') {
                    c = '_';
                }
            }
            out << "<a href=\"" << filename << "\" class=\"module-card\">\n";
            out << "  <div class=\"module-name\">" << escape_html(module->name) << "</div>\n";
            if (!module->summary.empty()) {
                out << "  <div class=\"module-summary\">" << escape_html(module->summary)
                    << "</div>\n";
            }
            out << "</a>\n";
        }

        out << "</div>\n</section>\n";
    }

    write_footer(out);
    out << R"(</main>
</div>
<button class="mobile-toggle" aria-label="Toggle menu">☰</button>
)";
    write_scripts(out, "");
    out << R"(</body>
</html>)";
}

void HtmlGenerator::write_sidebar_index_with_libraries(const std::vector<DocLibrary>& libraries,
                                                       std::ostream& out) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href="index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search docs..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
)";

    // Each library is a nav section
    for (const auto& lib : libraries) {
        out << "    <nav class=\"nav-section\">\n";
        out << "        <div class=\"nav-title\">" << escape_html(lib.name) << "</div>\n";
        out << "        <ul class=\"nav-list\">\n";

        for (const auto* module : lib.modules) {
            std::string filename = "pages/" + module->name + ".html";
            for (auto& c : filename) {
                if (c == ':')
                    c = '_';
            }
            out << "            <li class=\"nav-item\"><a href=\"" << filename
                << "\" class=\"nav-link\"><span class=\"nav-icon module\"></span>"
                << escape_html(module->name) << "</a></li>\n";
        }

        out << "        </ul>\n    </nav>\n";
    }

    out << "</aside>\n";
}

void HtmlGenerator::write_module_sidebar_with_libraries(const std::string& current_module,
                                                        const DocModule& module,
                                                        const std::vector<DocLibrary>& libraries,
                                                        std::ostream& out,
                                                        const std::string& asset_prefix) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href=")"
        << asset_prefix << R"(index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
)";

    // Libraries navigation - collapsed by default, current library expanded
    for (const auto& lib : libraries) {
        bool lib_contains_current = false;
        for (const auto* mod : lib.modules) {
            if (mod->name == current_module) {
                lib_contains_current = true;
                break;
            }
        }

        out << "    <nav class=\"nav-section\">\n";
        out << "        <div class=\"nav-title\">" << escape_html(lib.name) << "</div>\n";
        out << "        <ul class=\"nav-list" << (lib_contains_current ? "" : " nav-list-collapsed")
            << "\" id=\"lib-" << escape_html(lib.name) << "\">\n";

        for (const auto* mod : lib.modules) {
            std::string filename = mod->name + ".html";
            for (auto& c : filename) {
                if (c == ':')
                    c = '_';
            }
            bool is_current = (mod->name == current_module);
            out << "            <li class=\"nav-item" << (is_current ? " active" : "") << "\">"
                << "<a href=\"" << filename << "\" class=\"nav-link\">"
                << "<span class=\"nav-icon module\"></span>" << escape_html(mod->name)
                << "</a></li>\n";
        }

        out << "        </ul>\n";
        if (!lib_contains_current && lib.modules.size() > 5) {
            out << "        <button class=\"nav-toggle\" onclick=\"toggleLibrary('" << lib.name
                << "')\">Show " << lib.name << "</button>\n";
        }
        out << "    </nav>\n";
    }

    // Current module items section
    out << R"(    <nav class="nav-section">
        <div class="nav-title">)"
        << escape_html(current_module) << R"(</div>
        <ul class="nav-list">
)";

    // Group items by kind
    std::vector<const DocItem*> structs, enums, traits, functions;
    for (const auto& item : module.items) {
        switch (item.kind) {
        case DocItemKind::Struct:
            structs.push_back(&item);
            break;
        case DocItemKind::Enum:
            enums.push_back(&item);
            break;
        case DocItemKind::Trait:
            traits.push_back(&item);
            break;
        case DocItemKind::Function:
            functions.push_back(&item);
            break;
        default:
            break;
        }
    }

    for (const auto* item : structs) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon struct\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }
    for (const auto* item : enums) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon enum\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }
    for (const auto* item : traits) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon behavior\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }
    for (const auto* item : functions) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon function\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    out << R"(        </ul>
    </nav>
</aside>
)";
}

void HtmlGenerator::generate_search_index_filtered(const std::vector<DocLibrary>& libraries,
                                                   std::ostream& out) {
    out << "window.searchIndex = [\n";

    bool first = true;

    auto add_item = [&](const DocItem& item, const std::string& module_name) {
        if (!first) {
            out << ",\n";
        }
        first = false;

        std::string escaped_summary;
        for (char c : item.summary) {
            if (c == '"')
                escaped_summary += "\\\"";
            else if (c == '\\')
                escaped_summary += "\\\\";
            else if (c == '\n')
                escaped_summary += " ";
            else if (c == '\r')
                continue;
            else
                escaped_summary += c;
        }

        std::string module_file = module_name;
        for (auto& c : module_file) {
            if (c == ':')
                c = '_';
        }

        out << "  {\"name\": \"" << item.name << "\", \"id\": \"" << item.id << "\", \"path\": \""
            << item.path << "\", \"kind\": \"" << doc_item_kind_to_string(item.kind)
            << "\", \"module\": \"pages/" << module_file << "\", \"summary\": \"" << escaped_summary
            << "\"}";
    };

    std::function<void(const std::vector<DocItem>&, const std::string&)> add_items;
    add_items = [&](const std::vector<DocItem>& items, const std::string& module_name) {
        for (const auto& item : items) {
            add_item(item, module_name);
            add_items(item.methods, module_name);
            add_items(item.fields, module_name);
            add_items(item.variants, module_name);
        }
    };

    for (const auto& lib : libraries) {
        for (const auto* module : lib.modules) {
            add_items(module->items, module->name);
        }
    }

    out << "\n];\n";
}

void HtmlGenerator::generate_search_index(const DocIndex& index, std::ostream& out) {
    out << "window.searchIndex = [\n";

    bool first = true;

    // Helper to add an item to the search index
    auto add_item = [&](const DocItem& item, const std::string& module_name) {
        if (!first) {
            out << ",\n";
        }
        first = false;

        // Escape strings for JSON
        std::string escaped_summary;
        for (char c : item.summary) {
            if (c == '"')
                escaped_summary += "\\\"";
            else if (c == '\\')
                escaped_summary += "\\\\";
            else if (c == '\n')
                escaped_summary += " ";
            else if (c == '\r')
                continue;
            else
                escaped_summary += c;
        }

        std::string module_file = module_name;
        for (auto& c : module_file) {
            if (c == ':')
                c = '_';
        }

        // URLs point to pages/ subdirectory
        out << "  {\"name\": \"" << item.name << "\", \"id\": \"" << item.id << "\", \"path\": \""
            << item.path << "\", \"kind\": \"" << doc_item_kind_to_string(item.kind)
            << "\", \"module\": \"pages/" << module_file << "\", \"summary\": \"" << escaped_summary
            << "\"}";
    };

    // Helper to recursively add items
    std::function<void(const std::vector<DocItem>&, const std::string&)> add_items;
    add_items = [&](const std::vector<DocItem>& items, const std::string& module_name) {
        for (const auto& item : items) {
            add_item(item, module_name);
            add_items(item.methods, module_name);
            add_items(item.fields, module_name);
            add_items(item.variants, module_name);
        }
    };

    for (const auto& module : index.modules) {
        add_items(module.items, module.name);
    }

    out << "\n];\n";
}

void HtmlGenerator::write_head(const std::string& title, std::ostream& out,
                               const std::string& asset_prefix) {
    out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    out << "  <meta charset=\"UTF-8\">\n";
    out << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    out << "  <title>" << escape_html(title) << " - TML Documentation</title>\n";
    out << "  <link rel=\"stylesheet\" href=\"" << asset_prefix << "style.css\">\n";
    out << "</head>\n";
}

void HtmlGenerator::write_scripts(std::ostream& out, const std::string& asset_prefix) {
    out << "<script src=\"" << asset_prefix << "search-index.js\"></script>\n";
    out << "<script src=\"" << asset_prefix << "search.js\"></script>\n";
}

void HtmlGenerator::write_navigation(const DocIndex& index, std::ostream& out) {
    out << "<nav>\n";
    out << "  <a href=\"index.html\">Home</a>\n";
    for (const auto& module : index.modules) {
        std::string filename = module.name + ".html";
        out << "  <a href=\"" << filename << "\">" << escape_html(module.name) << "</a>\n";
    }
    out << "</nav>\n";
}

void HtmlGenerator::write_item_section(const DocItem& item, std::ostream& out) {
    out << "<div class=\"item\" id=\"" << escape_html(item.id) << "\">\n";

    // Title
    out << "<h3>" << escape_html(item.name) << "</h3>\n";

    write_signature_html(item, out);

    // Deprecated warning
    if (item.deprecated) {
        out << "<div class=\"deprecated\"><strong>Deprecated:</strong> "
            << escape_html(item.deprecated->message) << "</div>\n";
    }

    // Description
    if (!item.doc.empty()) {
        out << "<div class=\"description\">" << markdown_to_html(item.doc) << "</div>\n";
    }

    // Parameters
    if (!item.params.empty()) {
        out << "<h4>Parameters</h4>\n<table>\n";
        out << "<tr><th>Name</th><th>Type</th><th>Description</th></tr>\n";
        for (const auto& param : item.params) {
            out << "<tr><td><code>" << escape_html(param.name) << "</code></td>";
            out << "<td><code>" << escape_html(param.type) << "</code></td>";
            out << "<td>" << escape_html(param.description) << "</td></tr>\n";
        }
        out << "</table>\n";
    }

    // Returns
    if (item.returns) {
        out << "<h4>Returns</h4>\n";
        out << "<p><code>" << escape_html(item.returns->type) << "</code>";
        if (!item.returns->description.empty()) {
            out << " - " << escape_html(item.returns->description);
        }
        out << "</p>\n";
    }

    // Methods
    if (!item.methods.empty()) {
        out << "<h4>Methods</h4>\n";
        for (const auto& method : item.methods) {
            out << "<div class=\"method\">\n";
            out << "<h5>" << escape_html(method.name) << "</h5>\n";
            out << "<pre class=\"signature\">" << escape_html(method.signature) << "</pre>\n";
            if (!method.summary.empty()) {
                out << "<p>" << escape_html(method.summary) << "</p>\n";
            }
            out << "</div>\n";
        }
    }

    out << "</div>\n";
}

void HtmlGenerator::write_signature_html(const DocItem& item, std::ostream& out) {
    out << "<pre class=\"signature\">" << escape_html(item.signature) << "</pre>\n";
}

void HtmlGenerator::write_module_sidebar(const std::string& current_module, const DocModule& module,
                                         const DocIndex& index, std::ostream& out,
                                         const std::string& asset_prefix) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href=")"
        << asset_prefix << R"(index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
)";

    // All modules navigation section
    out << R"(    <nav class="nav-section">
        <div class="nav-title">Modules</div>
        <ul class="nav-list nav-list-collapsed" id="modules-list">
)";

    for (const auto& mod : index.modules) {
        std::string filename = mod.name + ".html";
        for (auto& c : filename) {
            if (c == ':')
                c = '_';
        }
        bool is_current = (mod.name == current_module);
        out << "            <li class=\"nav-item" << (is_current ? " active" : "") << "\">"
            << "<a href=\"" << filename << "\" class=\"nav-link\">"
            << "<span class=\"nav-icon module\"></span>" << escape_html(mod.name) << "</a></li>\n";
    }

    out << R"HTML(        </ul>
        <button class="nav-toggle" onclick="toggleModulesList()">Show all modules</button>
    </nav>
)HTML";

    // Current module items section
    out << R"(    <nav class="nav-section">
        <div class="nav-title">)"
        << escape_html(current_module) << R"(</div>
        <ul class="nav-list">
)";

    // Group items by kind
    std::vector<const DocItem*> structs, enums, traits, functions;
    for (const auto& item : module.items) {
        switch (item.kind) {
        case DocItemKind::Struct:
            structs.push_back(&item);
            break;
        case DocItemKind::Enum:
            enums.push_back(&item);
            break;
        case DocItemKind::Trait:
            traits.push_back(&item);
            break;
        case DocItemKind::Function:
            functions.push_back(&item);
            break;
        default:
            break;
        }
    }

    // Structs
    for (const auto* item : structs) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon struct\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    // Enums
    for (const auto* item : enums) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon enum\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    // Behaviors
    for (const auto* item : traits) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon behavior\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    // Functions
    for (const auto* item : functions) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon function\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    out << R"(        </ul>
    </nav>
</aside>
)";
}

void HtmlGenerator::write_sidebar_index(const DocIndex& index, std::ostream& out) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href="index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
)";
    if (!index.version.empty()) {
        out << "        <span class=\"version-badge\">v" << escape_html(index.version)
            << "</span>\n";
    }
    out << R"(    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search docs..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
    <nav class="nav-section">
        <div class="nav-title">Modules</div>
        <ul class="nav-list">
)";

    for (const auto& module : index.modules) {
        std::string filename = "pages/" + module.name + ".html";
        for (auto& c : filename) {
            if (c == ':')
                c = '_';
        }
        out << "            <li class=\"nav-item\"><a href=\"" << filename
            << "\" class=\"nav-link\"><span class=\"nav-icon module\"></span>"
            << escape_html(module.name) << "</a></li>\n";
    }

    out << R"(        </ul>
    </nav>
</aside>
)";
}

void HtmlGenerator::write_item_card(const DocItem& item, std::ostream& out) {
    std::string kind_class;
    switch (item.kind) {
    case DocItemKind::Function:
        kind_class = "function";
        break;
    case DocItemKind::Method:
        kind_class = "method";
        break;
    case DocItemKind::Struct:
        kind_class = "struct";
        break;
    case DocItemKind::Enum:
        kind_class = "enum";
        break;
    case DocItemKind::Trait:
        kind_class = "behavior";
        break;
    case DocItemKind::Constant:
        kind_class = "constant";
        break;
    case DocItemKind::Field:
        kind_class = "field";
        break;
    default:
        kind_class = "function";
        break;
    }

    out << "<article class=\"item-card\" id=\"" << escape_html(item.id) << "\">\n";

    // Header with badge and name
    out << "  <div class=\"item-header\">\n";
    out << "    <span class=\"item-kind-badge " << kind_class << "\">"
        << doc_item_kind_to_string(item.kind) << "</span>\n";
    out << "    <h3 class=\"item-name\"><a href=\"#" << escape_html(item.id) << "\">"
        << escape_html(item.name) << "</a></h3>\n";
    out << "  </div>\n";

    // Signature
    out << "  <div class=\"signature\">" << escape_html(item.signature) << "</div>\n";

    // Deprecated warning
    if (item.deprecated) {
        out << "  <div class=\"deprecated-warning\"><strong>Deprecated:</strong> "
            << escape_html(item.deprecated->message) << "</div>\n";
    }

    // Description
    if (!item.doc.empty()) {
        out << "  <div class=\"item-description\">" << markdown_to_html(item.doc) << "</div>\n";
    }

    // Parameters
    if (!item.params.empty()) {
        out << "  <div class=\"params-section\">\n";
        out << "    <div class=\"params-title\">Parameters</div>\n";
        out << "    <table class=\"params-table\">\n";
        out << "      <thead><tr><th>Name</th><th>Type</th><th>Description</th></tr></thead>\n";
        out << "      <tbody>\n";
        for (const auto& param : item.params) {
            out << "        <tr><td class=\"param-name\">" << escape_html(param.name)
                << "</td><td class=\"param-type\">" << escape_html(param.type) << "</td><td>"
                << escape_html(param.description) << "</td></tr>\n";
        }
        out << "      </tbody>\n";
        out << "    </table>\n";
        out << "  </div>\n";
    }

    // Returns
    if (item.returns) {
        out << "  <div class=\"returns-section\">\n";
        out << "    <div class=\"returns-title\">Returns</div>\n";
        out << "    <p><code class=\"param-type\">" << escape_html(item.returns->type) << "</code>";
        if (!item.returns->description.empty()) {
            out << " &mdash; " << escape_html(item.returns->description);
        }
        out << "</p>\n";
        out << "  </div>\n";
    }

    // Methods
    if (!item.methods.empty()) {
        out << "  <div class=\"methods-section\">\n";
        out << "    <div class=\"methods-title\">Methods</div>\n";
        for (const auto& method : item.methods) {
            out << "    <div class=\"method-item\" id=\"" << escape_html(method.id) << "\">\n";
            out << "      <div class=\"method-name\">" << escape_html(method.name) << "</div>\n";
            out << "      <div class=\"method-sig\">" << escape_html(method.signature)
                << "</div>\n";
            if (!method.summary.empty()) {
                out << "      <div class=\"method-desc\">" << escape_html(method.summary)
                    << "</div>\n";
            }
            out << "    </div>\n";
        }
        out << "  </div>\n";
    }

    // Variants (for enums)
    if (!item.variants.empty()) {
        out << "  <div class=\"methods-section\">\n";
        out << "    <div class=\"methods-title\">Variants</div>\n";
        for (const auto& variant : item.variants) {
            out << "    <div class=\"method-item\">\n";
            out << "      <div class=\"method-name\">" << escape_html(variant.name) << "</div>\n";
            out << "      <div class=\"method-sig\">" << escape_html(variant.signature)
                << "</div>\n";
            if (!variant.summary.empty()) {
                out << "      <div class=\"method-desc\">" << escape_html(variant.summary)
                    << "</div>\n";
            }
            out << "    </div>\n";
        }
        out << "  </div>\n";
    }

    // Fields (for structs)
    if (!item.fields.empty()) {
        out << "  <div class=\"methods-section\">\n";
        out << "    <div class=\"methods-title\">Fields</div>\n";
        for (const auto& field : item.fields) {
            out << "    <div class=\"method-item\">\n";
            out << "      <div class=\"method-name\">" << escape_html(field.name) << "</div>\n";
            if (field.returns) {
                out << "      <div class=\"method-sig\">" << escape_html(field.returns->type)
                    << "</div>\n";
            }
            if (!field.summary.empty()) {
                out << "      <div class=\"method-desc\">" << escape_html(field.summary)
                    << "</div>\n";
            }
            out << "    </div>\n";
        }
        out << "  </div>\n";
    }

    out << "</article>\n";
}

void HtmlGenerator::write_footer(std::ostream& out) {
    out << R"(<footer class="footer">
    <p>Generated by <a href="https://github.com/tml-lang/tml">TML Documentation Generator</a></p>
</footer>
)";
}

std::string HtmlGenerator::escape_html(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            result += c;
            break;
        }
    }
    return result;
}

std::string HtmlGenerator::markdown_to_html(const std::string& markdown) {
    // Simple markdown to HTML conversion
    // For production, use a proper markdown library
    std::string result;
    std::istringstream stream(markdown);
    std::string line;
    bool in_code_block = false;
    bool in_paragraph = false;

    while (std::getline(stream, line)) {
        // Code blocks
        if (line.starts_with("```")) {
            if (in_code_block) {
                result += "</pre>\n";
                in_code_block = false;
            } else {
                if (in_paragraph) {
                    result += "</p>\n";
                    in_paragraph = false;
                }
                result += "<pre><code>";
                in_code_block = true;
            }
            continue;
        }

        if (in_code_block) {
            result += escape_html(line) + "\n";
            continue;
        }

        // Headers
        if (line.starts_with("### ")) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            result += "<h4>" + escape_html(line.substr(4)) + "</h4>\n";
            continue;
        }
        if (line.starts_with("## ")) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            result += "<h3>" + escape_html(line.substr(3)) + "</h3>\n";
            continue;
        }
        if (line.starts_with("# ")) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            result += "<h2>" + escape_html(line.substr(2)) + "</h2>\n";
            continue;
        }

        // Empty line ends paragraph
        if (line.empty()) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            continue;
        }

        // Regular text
        if (!in_paragraph) {
            result += "<p>";
            in_paragraph = true;
        } else {
            result += " ";
        }
        result += escape_html(line);
    }

    if (in_paragraph) {
        result += "</p>\n";
    }
    if (in_code_block) {
        result += "</code></pre>\n";
    }

    return result;
}

} // namespace tml::doc
