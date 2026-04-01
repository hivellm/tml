TML_MODULE("compiler")

//! # Profile Command — Flame Graph Generation
//!
//! Reads a `.cpuprofile` JSON file and generates flame graphs.
//!
//! ## Supported Output Formats
//!
//! | Format | Description                                        |
//! |--------|----------------------------------------------------|
//! | ASCII  | Terminal flame graph using block characters        |
//! | SVG    | Scalable Vector Graphics for browser/tool viewing  |
//!
//! ## cpuprofile JSON Format (V8 CPU Profile)
//!
//! The parser handles the standard V8 cpuprofile structure:
//! ```json
//! {
//!   "nodes": [
//!     { "id": 1, "callFrame": { "functionName": "...", "url": "...",
//!       "lineNumber": 0, "columnNumber": 0 }, "hitCount": 0, "children": [2, 3] }
//!   ],
//!   "startTime": 12345,
//!   "endTime": 67890,
//!   "samples": [1, 2, 3, ...],
//!   "timeDeltas": [100, 200, ...]
//! }
//! ```
//!
//! ## Flame Graph Algorithm
//!
//! 1. Parse nodes and build parent→children tree from `children` arrays
//! 2. Accumulate inclusive time: each node's time = sum of time deltas where
//!    that node_id appears in `samples`
//! 3. Walk the tree depth-first to assign x-offsets for display
//! 4. Render each frame as a proportionally-sized block

#include "cmd_profile.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::cli {

// ============================================================================
// Minimal JSON extraction helpers (no external dependencies)
// ============================================================================

namespace {

/// Extract a string value for a key in a flat JSON object.
/// Only handles `"key":"value"` patterns — sufficient for cpuprofile callFrame fields.
std::string json_str(const std::string& json, const char* key) {
    std::string needle = std::string("\"") + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return "";
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos)
        return "";
    return json.substr(pos, end - pos);
}

/// Extract an integer value for a key in a flat JSON object.
/// Handles `"key":N` patterns (signed 64-bit).
int64_t json_int(const std::string& json, const char* key) {
    std::string needle = std::string("\"") + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return 0;
    pos += needle.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        pos++;
    if (pos >= json.size())
        return 0;
    // Handle negative
    bool neg = json[pos] == '-';
    if (neg)
        pos++;
    int64_t val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return neg ? -val : val;
}

/// Parse an array of integers from `"key":[1, 2, 3]`.
std::vector<int64_t> json_int_array(const std::string& json, const char* key) {
    std::string needle = std::string("\"") + key + "\":[";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return {};
    pos += needle.size();

    std::vector<int64_t> result;
    while (pos < json.size() && json[pos] != ']') {
        // Skip whitespace and commas
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\n' ||
                                     json[pos] == '\r' || json[pos] == '\t'))
            pos++;
        if (pos >= json.size() || json[pos] == ']')
            break;
        bool neg = json[pos] == '-';
        if (neg)
            pos++;
        int64_t val = 0;
        bool got_digit = false;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
            val = val * 10 + (json[pos] - '0');
            pos++;
            got_digit = true;
        }
        if (got_digit)
            result.push_back(neg ? -val : val);
        else
            pos++; // skip non-digit character
    }
    return result;
}

// ============================================================================
// cpuprofile data model
// ============================================================================

struct ProfileNode {
    uint32_t id{0};
    std::string function_name;
    std::string url;
    uint32_t line_number{0};
    std::vector<uint32_t> children;
    uint64_t hit_count{0};    // From cpuprofile hitCount field
    uint64_t sample_ticks{0}; // Accumulated from samples array
    uint64_t self_ticks{0};   // Ticks where this node is the leaf
    uint64_t total_ticks{0};  // Inclusive ticks (self + children, filled by tree walk)
};

struct CpuProfile {
    std::vector<ProfileNode> nodes;
    int64_t start_time{0};
    int64_t end_time{0};
    std::vector<int64_t> samples;
    std::vector<int64_t> time_deltas;
    uint32_t root_id{1};
};

// ============================================================================
// cpuprofile JSON parser
// ============================================================================

/// Parse a single node object from the JSON `nodes` array.
/// Input `obj` is the substring `{...}` for one node.
ProfileNode parse_node(const std::string& obj) {
    ProfileNode node;

    // Extract id
    node.id = static_cast<uint32_t>(json_int(obj, "id"));

    // Extract callFrame sub-object
    auto cf_start = obj.find("\"callFrame\":");
    if (cf_start != std::string::npos) {
        cf_start = obj.find('{', cf_start);
        if (cf_start != std::string::npos) {
            // Find matching closing brace
            int depth = 1;
            auto cf_end = cf_start + 1;
            while (cf_end < obj.size() && depth > 0) {
                if (obj[cf_end] == '{')
                    depth++;
                else if (obj[cf_end] == '}')
                    depth--;
                cf_end++;
            }
            std::string cf = obj.substr(cf_start, cf_end - cf_start);
            node.function_name = json_str(cf, "functionName");
            node.url = json_str(cf, "url");
            node.line_number = static_cast<uint32_t>(json_int(cf, "lineNumber"));
        }
    }

    // Extract hitCount
    node.hit_count = static_cast<uint64_t>(json_int(obj, "hitCount"));

    // Extract children array
    auto ch_start = obj.find("\"children\":[");
    if (ch_start != std::string::npos) {
        ch_start += 12; // skip `"children":[`
        while (ch_start < obj.size() && obj[ch_start] != ']') {
            while (ch_start < obj.size() &&
                   (obj[ch_start] == ' ' || obj[ch_start] == ',' || obj[ch_start] == '\n' ||
                    obj[ch_start] == '\r' || obj[ch_start] == '\t'))
                ch_start++;
            if (ch_start >= obj.size() || obj[ch_start] == ']')
                break;
            uint32_t child_id = 0;
            bool got = false;
            while (ch_start < obj.size() && obj[ch_start] >= '0' && obj[ch_start] <= '9') {
                child_id = child_id * 10 + (obj[ch_start] - '0');
                ch_start++;
                got = true;
            }
            if (got)
                node.children.push_back(child_id);
            else
                ch_start++;
        }
    }

    if (node.function_name.empty())
        node.function_name = "(unknown)";

    return node;
}

/// Parse the full `nodes` array from the cpuprofile JSON.
std::vector<ProfileNode> parse_nodes_array(const std::string& json) {
    std::vector<ProfileNode> nodes;

    auto arr_start = json.find("\"nodes\":[");
    if (arr_start == std::string::npos)
        return nodes;
    arr_start += 9; // skip `"nodes":[`

    // Walk through objects in the array
    size_t pos = arr_start;
    while (pos < json.size()) {
        // Skip whitespace and commas
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\n' ||
                                     json[pos] == '\r' || json[pos] == '\t'))
            pos++;
        if (pos >= json.size() || json[pos] == ']')
            break;
        if (json[pos] != '{')
            break;
        // Find matching closing brace
        int depth = 1;
        size_t obj_start = pos;
        pos++;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '{')
                depth++;
            else if (json[pos] == '}')
                depth--;
            // Skip strings to avoid false brace matches
            else if (json[pos] == '"') {
                pos++;
                while (pos < json.size() && json[pos] != '"') {
                    if (json[pos] == '\\')
                        pos++;
                    pos++;
                }
            }
            pos++;
        }
        std::string obj = json.substr(obj_start, pos - obj_start);
        nodes.push_back(parse_node(obj));
    }

    return nodes;
}

/// Load a .cpuprofile file and parse it into a CpuProfile struct.
/// Returns false and prints an error on failure.
bool load_cpuprofile(const std::string& path, CpuProfile& profile) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "error: cannot open '" << path << "'\n";
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    profile.nodes = parse_nodes_array(json);
    if (profile.nodes.empty()) {
        std::cerr << "error: no nodes found in '" << path << "'. Is it a valid .cpuprofile?\n";
        return false;
    }

    profile.start_time = json_int(json, "startTime");
    profile.end_time = json_int(json, "endTime");
    profile.samples = json_int_array(json, "samples");
    profile.time_deltas = json_int_array(json, "timeDeltas");

    // Determine root id (first node's id, typically 1)
    profile.root_id = profile.nodes.empty() ? 1 : profile.nodes[0].id;

    return true;
}

// ============================================================================
// Tick accumulation
// ============================================================================

/// Build a map from node id -> index in the nodes vector.
std::unordered_map<uint32_t, size_t> build_id_index(const std::vector<ProfileNode>& nodes) {
    std::unordered_map<uint32_t, size_t> idx;
    idx.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); i++)
        idx[nodes[i].id] = i;
    return idx;
}

/// Accumulate self_ticks from the samples array.
/// Each sample[i] is a node_id. time_deltas[i] is the duration of that sample.
/// If time_deltas is shorter than samples, use 1 per sample as fallback.
void accumulate_self_ticks(CpuProfile& profile) {
    auto idx = build_id_index(profile.nodes);
    size_t n = profile.samples.size();
    for (size_t i = 0; i < n; i++) {
        uint32_t node_id = static_cast<uint32_t>(profile.samples[i]);
        int64_t delta = (i < profile.time_deltas.size()) ? profile.time_deltas[i] : 1;
        if (delta < 0)
            delta = 0;
        auto it = idx.find(node_id);
        if (it != idx.end()) {
            profile.nodes[it->second].self_ticks += static_cast<uint64_t>(delta);
        }
    }
}

/// Compute total_ticks for each node by summing self + all descendant self ticks.
/// Uses post-order DFS with cycle detection.
void compute_total_ticks(CpuProfile& profile) {
    auto idx = build_id_index(profile.nodes);

    // Post-order DFS via explicit stack to avoid stack overflow for deep call trees
    // Stack holds (node_id, child_index, entered)
    struct Frame {
        uint32_t id;
        size_t child_idx;
    };
    std::vector<bool> visited(profile.nodes.size(), false);

    // Initialize total_ticks = self_ticks for all nodes
    for (auto& node : profile.nodes)
        node.total_ticks = node.self_ticks;

    // DFS from root
    auto root_it = idx.find(profile.root_id);
    if (root_it == idx.end())
        return;

    std::vector<Frame> stack;
    stack.push_back({profile.root_id, 0});

    while (!stack.empty()) {
        auto& top = stack.back();
        auto node_it = idx.find(top.id);
        if (node_it == idx.end()) {
            stack.pop_back();
            continue;
        }
        size_t node_idx = node_it->second;
        auto& node = profile.nodes[node_idx];

        if (top.child_idx < node.children.size()) {
            uint32_t child_id = node.children[top.child_idx];
            top.child_idx++;
            auto child_it = idx.find(child_id);
            if (child_it != idx.end() && !visited[child_it->second]) {
                visited[child_it->second] = true;
                stack.push_back({child_id, 0});
            }
        } else {
            // All children processed — propagate total_ticks to parent
            stack.pop_back();
            if (!stack.empty()) {
                auto parent_it = idx.find(stack.back().id);
                if (parent_it != idx.end()) {
                    profile.nodes[parent_it->second].total_ticks += node.total_ticks;
                }
            }
        }
    }
}

// ============================================================================
// Flame graph data model
// ============================================================================

struct FlameFrame {
    std::string label; // "functionName (file:line)"
    uint64_t self_ticks;
    uint64_t total_ticks;
    int depth{0};
    double x_offset{0.0}; // fractional position in [0,1]
    double width{0.0};    // fractional width in [0,1]
};

/// Build the flat list of flame frames by walking the call tree depth-first.
/// Each frame gets x_offset and width proportional to its total_ticks relative
/// to the root's total_ticks.
std::vector<FlameFrame> build_flame_frames(const CpuProfile& profile) {
    auto idx = build_id_index(profile.nodes);
    auto root_it = idx.find(profile.root_id);
    if (root_it == idx.end())
        return {};

    uint64_t root_ticks = profile.nodes[root_it->second].total_ticks;
    if (root_ticks == 0) {
        // Fall back to counting samples
        root_ticks = static_cast<uint64_t>(profile.samples.size());
        if (root_ticks == 0)
            root_ticks = 1;
    }

    std::vector<FlameFrame> frames;

    // DFS with (node_id, depth, x_offset)
    struct Frame {
        uint32_t id;
        int depth;
        double x_offset;
        double parent_width;
    };
    std::vector<Frame> stack;
    stack.push_back({profile.root_id, 0, 0.0, 1.0});

    while (!stack.empty()) {
        auto f = stack.back();
        stack.pop_back();

        auto node_it = idx.find(f.id);
        if (node_it == idx.end())
            continue;
        const auto& node = profile.nodes[node_it->second];

        double width = static_cast<double>(node.total_ticks) / static_cast<double>(root_ticks);

        // Build label
        std::string label = node.function_name;
        if (!node.url.empty()) {
            label += " (";
            label += node.url;
            if (node.line_number > 0) {
                label += ":";
                label += std::to_string(node.line_number);
            }
            label += ")";
        }

        FlameFrame ff;
        ff.label = label;
        ff.self_ticks = node.self_ticks;
        ff.total_ticks = node.total_ticks;
        ff.depth = f.depth;
        ff.x_offset = f.x_offset;
        ff.width = width;
        frames.push_back(ff);

        // Child x-offsets are computed below (forward pass into child_infos, then pushed in
        // reverse)

        // Child x-offsets are computed below using running_x

        // Simple approach: push children in reverse, each gets a fraction of parent width
        // Recompute: push in reverse order, each child's x_offset based on cumulative position
        // We need forward-order x_offsets but reverse-order push. Precompute a small array.
        struct ChildInfo {
            uint32_t id;
            double x;
        };
        std::vector<ChildInfo> child_infos;
        double running_x = f.x_offset;
        for (uint32_t child_id : node.children) {
            auto child_it2 = idx.find(child_id);
            if (child_it2 == idx.end())
                continue;
            const auto& child2 = profile.nodes[child_it2->second];
            double cw = static_cast<double>(child2.total_ticks) / static_cast<double>(root_ticks);
            child_infos.push_back({child_id, running_x});
            running_x += cw;
        }
        // Push in reverse so leftmost is on top of stack
        for (int i = static_cast<int>(child_infos.size()) - 1; i >= 0; i--) {
            stack.push_back({child_infos[i].id, f.depth + 1, child_infos[i].x, width});
        }
    }

    return frames;
}

// ============================================================================
// ASCII flame graph renderer
// ============================================================================

/// Render an ASCII flame graph to stdout.
/// Each depth level is one row. Width is scaled to `term_width` columns.
void render_ascii_flamegraph(const std::vector<FlameFrame>& frames, int term_width = 100) {
    if (frames.empty()) {
        std::cout << "(no profile data)\n";
        return;
    }

    // Find max depth
    int max_depth = 0;
    for (const auto& f : frames)
        max_depth = std::max(max_depth, f.depth);

    // Group frames by depth
    std::vector<std::vector<const FlameFrame*>> by_depth(max_depth + 1);
    for (const auto& f : frames)
        by_depth[f.depth].push_back(&f);

    // Render from bottom (max depth) to top (depth 0) — flame graph style: root at bottom
    // Actually cpuprofile root is conceptual, show root at top (icicle style is more common
    // for profilers, but traditional flame graph has root at bottom).
    // We render root at top (icicle) since it's more readable for call profiles.
    std::cout << "\n";
    for (int d = 0; d <= max_depth; d++) {
        // Build a row of `term_width` characters
        std::string row(term_width, ' ');

        for (const auto* fp : by_depth[d]) {
            int col_start = static_cast<int>(fp->x_offset * term_width);
            int col_end = static_cast<int>((fp->x_offset + fp->width) * term_width);
            if (col_end > term_width)
                col_end = term_width;
            int col_width = col_end - col_start;
            if (col_width <= 0)
                continue;

            // Fill with label, truncated to fit
            std::string label = fp->label;
            if (col_width >= 3) {
                // Truncate label with ellipsis if needed
                if (static_cast<int>(label.size()) > col_width - 2) {
                    label = label.substr(0, col_width - 3) + "..";
                }
                // Left bracket, label (padded), right bracket
                row[col_start] = '[';
                int label_pos = col_start + 1;
                for (int ci = 0; ci < static_cast<int>(label.size()) && label_pos < col_end - 1;
                     ci++, label_pos++) {
                    row[label_pos] = label[ci];
                }
                // Fill remaining with dashes
                for (int ci = label_pos; ci < col_end - 1; ci++)
                    row[ci] = '=';
                if (col_end - 1 >= col_start)
                    row[col_end - 1] = ']';
            } else if (col_width == 2) {
                row[col_start] = '[';
                row[col_start + 1] = ']';
            } else {
                row[col_start] = '|';
            }
        }

        std::cout << row << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// SVG flame graph renderer
// ============================================================================

namespace {

/// Map a function name to a deterministic color for SVG rendering.
/// Uses a simple hash to pick from a warm palette (orange/red/yellow) consistent
/// with the standard flame graph color scheme.
std::string frame_color(const std::string& name) {
    // Warm palette base
    uint32_t hash = 2166136261u;
    for (char c : name) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    // r in 205-255, g in 0-230, b in 0-55
    int r = 205 + (hash & 0x1F);   // 205-236
    int g = (hash >> 5) & 0xFF;    // 0-255
    g = static_cast<int>(g * 0.9); // scale down slightly
    int b = (hash >> 13) & 0x37;   // 0-55

    char buf[32];
    snprintf(buf, sizeof(buf), "rgb(%d,%d,%d)", r, g, b);
    return std::string(buf);
}

/// Escape special XML characters in a string for use in SVG attributes/text.
std::string xml_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

} // anonymous namespace

/// Write an SVG flame graph to `output_path`.
/// Returns false on error.
bool render_svg_flamegraph(const std::vector<FlameFrame>& frames, const std::string& output_path) {
    if (frames.empty()) {
        std::cerr << "error: no frames to render\n";
        return false;
    }

    // SVG dimensions
    const int svg_width = 1200;
    const int row_height = 18;
    const int font_size = 12;
    const int top_padding = 30;

    int max_depth = 0;
    for (const auto& f : frames)
        max_depth = std::max(max_depth, f.depth);

    int svg_height = top_padding + (max_depth + 1) * row_height + 10;

    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "error: cannot write to '" << output_path << "'\n";
        return false;
    }

    // SVG header
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\""
        << " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
    out << "<svg version=\"1.1\" width=\"" << svg_width << "\" height=\"" << svg_height << "\""
        << " xmlns=\"http://www.w3.org/2000/svg\""
        << " xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n";

    // Background
    out << "<rect width=\"100%\" height=\"100%\" fill=\"#f8f8f8\" />\n";

    // Title
    out << "<text text-anchor=\"middle\" x=\"" << svg_width / 2 << "\" y=\"15\""
        << " font-size=\"14\" font-family=\"monospace\" fill=\"#333\">TML Flame Graph</text>\n";

    // Frames
    for (const auto& f : frames) {
        if (f.width < 0.001)
            continue; // Too narrow to render

        int x = static_cast<int>(f.x_offset * svg_width);
        int y = top_padding + f.depth * row_height;
        int w = static_cast<int>(f.width * svg_width);
        if (w < 1)
            w = 1;
        int h = row_height - 1;

        std::string color = frame_color(f.label);

        // Rectangle
        out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
            << "\" fill=\"" << color << "\" stroke=\"white\" stroke-width=\"0.5\">\n";
        out << "  <title>" << xml_escape(f.label) << " (" << f.total_ticks << " ticks)</title>\n";
        out << "</rect>\n";

        // Label text (only if wide enough)
        if (w >= 20) {
            std::string display_label = f.label;
            // Truncate: roughly 1 char per 7px
            size_t max_chars = static_cast<size_t>(w / 7);
            if (display_label.size() > max_chars && max_chars > 3) {
                display_label = display_label.substr(0, max_chars - 2) + "..";
            }
            out << "<text x=\"" << (x + 2) << "\" y=\"" << (y + h - 4) << "\""
                << " font-size=\"" << font_size << "\" font-family=\"monospace\""
                << " fill=\"black\" clip-path=\"url(#clip" << x << "_" << y << ")\">"
                << xml_escape(display_label) << "</text>\n";
        }
    }

    out << "</svg>\n";
    out.close();
    return true;
}

// ============================================================================
// Flame graph subcommand
// ============================================================================

int cmd_flamegraph(int argc, char* argv[], bool verbose) {
    // Usage: tml profile flamegraph <input.cpuprofile> [--ascii] [-o output.svg]
    if (argc < 4) {
        std::cerr << "Usage: tml profile flamegraph <input.cpuprofile> [--ascii] [-o output.svg]\n";
        std::cerr << "\nOptions:\n";
        std::cerr << "  --ascii            Print ASCII flame graph to stdout (default)\n";
        std::cerr << "  -o <output.svg>    Generate SVG flame graph to file\n";
        return 1;
    }

    std::string input_path = argv[3];
    bool ascii_mode = true;
    std::string svg_output;

    for (int i = 4; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--ascii") {
            ascii_mode = true;
        } else if (arg == "-o" && i + 1 < argc) {
            svg_output = argv[++i];
            ascii_mode = false;
        } else if (arg.starts_with("-o=")) {
            svg_output = arg.substr(3);
            ascii_mode = false;
        } else if (arg == "--verbose" || arg == "-v") {
            // verbose already handled by caller
        } else {
            std::cerr << "warning: unknown option '" << arg << "'\n";
        }
    }

    if (verbose) {
        std::cerr << "Loading profile: " << input_path << "\n";
    }

    CpuProfile profile;
    if (!load_cpuprofile(input_path, profile))
        return 1;

    if (verbose) {
        std::cerr << "Nodes: " << profile.nodes.size() << "\n";
        std::cerr << "Samples: " << profile.samples.size() << "\n";
        std::cerr << "Time deltas: " << profile.time_deltas.size() << "\n";
        if (profile.end_time > profile.start_time) {
            double dur_ms = (profile.end_time - profile.start_time) / 1000.0;
            std::cerr << "Duration: " << std::fixed << std::setprecision(2) << dur_ms << " ms\n";
        }
    }

    // Compute tick distributions
    accumulate_self_ticks(profile);
    compute_total_ticks(profile);

    // Build flame frames
    auto frames = build_flame_frames(profile);

    if (ascii_mode) {
        render_ascii_flamegraph(frames);
    } else {
        if (!render_svg_flamegraph(frames, svg_output))
            return 1;
        std::cout << "SVG flame graph written to: " << svg_output << "\n";
    }

    return 0;
}

} // anonymous namespace

// ============================================================================
// Public entry point
// ============================================================================

/// Main `tml profile` command dispatcher.
///
/// Subcommands:
///   flamegraph <input.cpuprofile> [--ascii] [-o output.svg]
int run_profile(int argc, char* argv[], bool verbose) {
    if (argc < 3) {
        std::cerr << "Usage: tml profile <subcommand> [options]\n";
        std::cerr << "\nSubcommands:\n";
        std::cerr
            << "  flamegraph <input.cpuprofile> [--ascii] [-o output.svg]   Generate flame graph\n";
        std::cerr << "\nExamples:\n";
        std::cerr << "  tml profile flamegraph profile.cpuprofile\n";
        std::cerr << "  tml profile flamegraph profile.cpuprofile -o flamegraph.svg\n";
        return 1;
    }

    std::string subcmd = argv[2];

    if (subcmd == "flamegraph") {
        return cmd_flamegraph(argc, argv, verbose);
    }

    std::cerr << "error: unknown profile subcommand '" << subcmd << "'\n";
    std::cerr << "Run 'tml profile' for usage information.\n";
    return 1;
}

} // namespace tml::cli
