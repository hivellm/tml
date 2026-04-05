/// TML Chart FFI — C wrapper around sciplot for TML @extern("c") bindings
///
/// Provides a simple procedural API:
///   chart_create() → handle
///   chart_add_bar(handle, label, value)
///   chart_add_line_point(handle, x, y)
///   chart_set_title(handle, title)
///   chart_save_png(handle, path)
///   chart_save_svg(handle, path)
///   chart_destroy(handle)

// sciplot headers bundled in native/sciplot/ (MIT license, see SCIPLOT-LICENSE)
// Compile: zig-cxx -std=c++17 -c -I native chart_ffi.cpp -o chart_ffi.obj
#include "sciplot/sciplot.hpp"

#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace sciplot;

// ============================================================================
// Internal chart state
// ============================================================================

enum class ChartType { Bar, Line, Scatter };

struct ChartState {
    std::string title = "Chart";
    std::string xlabel;
    std::string ylabel;
    int width = 800;
    int height = 500;
    ChartType type = ChartType::Bar;

    // Bar chart data
    std::vector<std::string> bar_labels;
    std::vector<double> bar_values;

    // Line/scatter data (multiple series)
    struct Series {
        std::string name;
        std::vector<double> xs;
        std::vector<double> ys;
    };
    std::vector<Series> series;
    int current_series = -1;
};

static std::map<int64_t, ChartState*> g_charts;
static int64_t g_next_id = 1;

// ============================================================================
// C API
// ============================================================================

extern "C" {

/// Create a new chart. Returns a handle (int64).
int64_t chart_create(const char* chart_type) {
    auto* state = new ChartState();
    std::string t = chart_type ? chart_type : "bar";
    if (t == "line")
        state->type = ChartType::Line;
    else if (t == "scatter")
        state->type = ChartType::Scatter;
    else
        state->type = ChartType::Bar;

    int64_t id = g_next_id++;
    g_charts[id] = state;
    return id;
}

/// Set chart title.
void chart_set_title(int64_t handle, const char* title) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    it->second->title = title ? title : "";
}

/// Set axis labels.
void chart_set_xlabel(int64_t handle, const char* label) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    it->second->xlabel = label ? label : "";
}

void chart_set_ylabel(int64_t handle, const char* label) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    it->second->ylabel = label ? label : "";
}

/// Set chart dimensions.
void chart_set_size(int64_t handle, int32_t width, int32_t height) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    it->second->width = width;
    it->second->height = height;
}

/// Add a bar to a bar chart.
void chart_add_bar(int64_t handle, const char* label, double value) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    it->second->bar_labels.push_back(label ? label : "");
    it->second->bar_values.push_back(value);
}

/// Start a new line/scatter series.
void chart_add_series(int64_t handle, const char* name) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    ChartState::Series s;
    s.name = name ? name : "";
    it->second->series.push_back(std::move(s));
    it->second->current_series = static_cast<int>(it->second->series.size()) - 1;
}

/// Add a point to the current series.
void chart_add_point(int64_t handle, double x, double y) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    auto* state = it->second;
    if (state->current_series < 0 || state->current_series >= (int)state->series.size())
        return;
    state->series[state->current_series].xs.push_back(x);
    state->series[state->current_series].ys.push_back(y);
}

/// Save chart to file. Format detected from extension (.png, .svg, .pdf).
int32_t chart_save(int64_t handle, const char* path) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return -1;
    auto* state = it->second;

    try {
        Plot2D plot;
        plot.xlabel(state->xlabel);
        plot.ylabel(state->ylabel);

        if (state->type == ChartType::Bar) {
            // Bar chart: use boxes
            Vec x_indices = linspace(0, static_cast<double>(state->bar_values.size() - 1),
                                     state->bar_values.size());
            Vec y_vals(state->bar_values.size());
            for (size_t i = 0; i < state->bar_values.size(); i++) {
                y_vals[i] = state->bar_values[i];
            }

            plot.drawBoxes(x_indices, y_vals).fillSolid().fillColor("steelblue").borderShow();

            // Set xtic labels
            std::string xtics;
            for (size_t i = 0; i < state->bar_labels.size(); i++) {
                if (i > 0)
                    xtics += ", ";
                xtics += "\"" + state->bar_labels[i] + "\" " + std::to_string(i);
            }
            plot.gnuplot("set xtics (" + xtics + ") rotate by -45");
            plot.gnuplot("set boxwidth 0.7");
            plot.gnuplot("set style fill solid 0.8");
        } else {
            // Line/scatter chart
            for (const auto& s : state->series) {
                Vec xs(s.xs.size()), ys(s.ys.size());
                for (size_t i = 0; i < s.xs.size(); i++) {
                    xs[i] = s.xs[i];
                    ys[i] = s.ys[i];
                }
                if (state->type == ChartType::Line) {
                    plot.drawCurve(xs, ys).label(s.name).lineWidth(2);
                } else {
                    plot.drawPoints(xs, ys).label(s.name).pointSize(1.5);
                }
            }
        }

        Figure fig = {{plot}};
        fig.title(state->title);

        Canvas canvas = {{fig}};
        canvas.size(state->width, state->height);
        canvas.save(path ? path : "chart.png");

        return 0;
    } catch (...) {
        return -1;
    }
}

/// Destroy a chart and free memory.
void chart_destroy(int64_t handle) {
    auto it = g_charts.find(handle);
    if (it == g_charts.end())
        return;
    delete it->second;
    g_charts.erase(it);
}

} // extern "C"
