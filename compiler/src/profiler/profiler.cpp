/**
 * @file profiler.cpp
 * @brief TML Native Profiler Implementation
 *
 * Generates Chrome DevTools compatible .cpuprofile files.
 */

#include "profiler/profiler.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tml::profiler {

// Thread-local call stack
thread_local std::vector<StackFrame> Profiler::call_stack_;

// ============================================================================
// Singleton
// ============================================================================

auto Profiler::instance() -> Profiler& {
    static Profiler instance;
    return instance;
}

Profiler::~Profiler() {
    if (active_.load()) {
        stop();
    }
}

// ============================================================================
// Initialization
// ============================================================================

void Profiler::initialize(const std::string& output_path, uint64_t sampling_interval_us) {
    std::lock_guard<std::mutex> lock(mutex_);

    output_path_ = output_path.empty() ? "profile.cpuprofile" : output_path;
    sampling_interval_us_ = sampling_interval_us;

    // Clear any existing data
    data_.nodes.clear();
    data_.samples.clear();
    data_.time_deltas.clear();
    node_map_.clear();

    // Create root node (id=1, required by cpuprofile format)
    CallFrame root;
    root.id = 1;
    root.function_name = "(root)";
    root.file_name = "";
    root.line_number = 0;
    root.column_number = 0;
    root.parent_id = 0;
    root.self_time_us = 0;
    root.total_time_us = 0;
    root.hit_count = 0;
    data_.nodes.push_back(root);

    // Create program node (id=2)
    CallFrame program;
    program.id = 2;
    program.function_name = "(program)";
    program.file_name = "";
    program.line_number = 0;
    program.column_number = 0;
    program.parent_id = 1;
    program.self_time_us = 0;
    program.total_time_us = 0;
    program.hit_count = 0;
    data_.nodes.push_back(program);
    data_.nodes[0].children.push_back(2);

    initialized_.store(true);
}

// ============================================================================
// Start/Stop
// ============================================================================

void Profiler::start() {
    if (!initialized_.load()) {
        initialize();
    }

    data_.start_time = now_us();

    // Initialize call stack with program node
    call_stack_.clear();
    call_stack_.push_back({2, data_.start_time}); // Start in (program)

    active_.store(true, std::memory_order_release);

    std::cerr << "[TML Profiler] Started profiling. Output: " << output_path_ << "\n";
}

void Profiler::stop() {
    if (!active_.load()) {
        return;
    }

    active_.store(false, std::memory_order_release);
    data_.end_time = now_us();

    // Unwind remaining stack frames
    int64_t now = data_.end_time;
    while (!call_stack_.empty()) {
        auto& frame = call_stack_.back();
        if (frame.node_id > 0 && frame.node_id <= data_.nodes.size()) {
            auto& node = data_.nodes[frame.node_id - 1];
            node.total_time_us += now - frame.enter_time_us;
        }
        call_stack_.pop_back();
    }

    // Update root node time
    if (!data_.nodes.empty()) {
        data_.nodes[0].total_time_us = data_.end_time - data_.start_time;
        if (data_.nodes.size() > 1) {
            data_.nodes[1].total_time_us = data_.end_time - data_.start_time;
        }
    }

    // Export the profile
    export_cpuprofile(output_path_);

    std::cerr << "[TML Profiler] Stopped. Profile written to: " << output_path_ << "\n";
    std::cerr << "[TML Profiler] Total time: " << (data_.end_time - data_.start_time) / 1000.0
              << " ms\n";
    std::cerr << "[TML Profiler] Nodes: " << data_.nodes.size() << "\n";
    std::cerr << "[TML Profiler] Samples: " << data_.samples.size() << "\n";
}

// ============================================================================
// Function Tracking
// ============================================================================

void Profiler::enter_function(const char* func_name, const char* file_name, uint32_t line) {
    if (!active_.load(std::memory_order_relaxed)) {
        return;
    }

    int64_t now = now_us();
    uint32_t parent_id = call_stack_.empty() ? 2 : call_stack_.back().node_id;

    // Get or create node for this call
    uint32_t node_id = get_or_create_node(func_name ? func_name : "(unknown)",
                                          file_name ? file_name : "(unknown)", line, parent_id);

    // Push onto call stack
    call_stack_.push_back({node_id, now});

    // Add sample
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Sample sample;
        sample.node_id = node_id;
        sample.timestamp_us = now - data_.start_time;

        if (!data_.samples.empty()) {
            data_.time_deltas.push_back(sample.timestamp_us - data_.samples.back().timestamp_us);
        }
        data_.samples.push_back(sample);

        // Increment hit count
        if (node_id > 0 && node_id <= data_.nodes.size()) {
            data_.nodes[node_id - 1].hit_count++;
        }
    }
}

void Profiler::exit_function() {
    if (!active_.load(std::memory_order_relaxed) || call_stack_.empty()) {
        return;
    }

    int64_t now = now_us();
    auto frame = call_stack_.back();
    call_stack_.pop_back();

    int64_t duration = now - frame.enter_time_us;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frame.node_id > 0 && frame.node_id <= data_.nodes.size()) {
            auto& node = data_.nodes[frame.node_id - 1];
            node.total_time_us += duration;
            // Self time will be calculated during export
        }
    }
}

void Profiler::add_sample() {
    if (!active_.load(std::memory_order_relaxed)) {
        return;
    }

    uint32_t current_node = call_stack_.empty() ? 2 : call_stack_.back().node_id;
    int64_t now = now_us();

    std::lock_guard<std::mutex> lock(mutex_);
    Sample sample;
    sample.node_id = current_node;
    sample.timestamp_us = now - data_.start_time;

    if (!data_.samples.empty()) {
        data_.time_deltas.push_back(sample.timestamp_us - data_.samples.back().timestamp_us);
    }
    data_.samples.push_back(sample);
}

// ============================================================================
// Node Management
// ============================================================================

auto Profiler::register_function(const std::string& func_name, const std::string& file_name,
                                 uint32_t line) -> uint32_t {
    // Pre-register with parent 2 (program)
    return get_or_create_node(func_name, file_name, line, 2);
}

auto Profiler::get_or_create_node(const std::string& func_name, const std::string& file_name,
                                  uint32_t line, uint32_t parent_id) -> uint32_t {
    std::string key = make_node_key(parent_id, func_name, file_name, line);

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = node_map_.find(key);
    if (it != node_map_.end()) {
        return it->second;
    }

    // Create new node
    CallFrame node;
    node.id = static_cast<uint32_t>(data_.nodes.size() + 1);
    node.function_name = func_name;
    node.file_name = file_name;
    node.line_number = line;
    node.column_number = 0;
    node.parent_id = parent_id;
    node.self_time_us = 0;
    node.total_time_us = 0;
    node.hit_count = 0;

    data_.nodes.push_back(node);
    node_map_[key] = node.id;

    // Add as child to parent
    if (parent_id > 0 && parent_id <= data_.nodes.size()) {
        data_.nodes[parent_id - 1].children.push_back(node.id);
    }

    return node.id;
}

auto Profiler::make_node_key(uint32_t parent_id, const std::string& func_name,
                             const std::string& file_name, uint32_t line) -> std::string {
    std::ostringstream ss;
    ss << parent_id << ":" << func_name << ":" << file_name << ":" << line;
    return ss.str();
}

// ============================================================================
// Time
// ============================================================================

auto Profiler::now_us() -> int64_t {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// ============================================================================
// Export
// ============================================================================

void Profiler::export_cpuprofile(const std::string& path) {
    std::string json = to_cpuprofile_json();

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[TML Profiler] ERROR: Could not write to " << path << "\n";
        return;
    }

    file << json;
    file.close();
}

auto Profiler::to_cpuprofile_json() const -> std::string {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0);

    ss << "{\n";

    // Nodes array
    ss << "  \"nodes\": [\n";
    for (size_t i = 0; i < data_.nodes.size(); ++i) {
        const auto& node = data_.nodes[i];
        ss << "    {\n";
        ss << "      \"id\": " << node.id << ",\n";
        ss << "      \"callFrame\": {\n";
        ss << "        \"functionName\": \"" << node.function_name << "\",\n";
        ss << "        \"scriptId\": \"0\",\n";
        ss << "        \"url\": \"" << node.file_name << "\",\n";
        ss << "        \"lineNumber\": " << node.line_number << ",\n";
        ss << "        \"columnNumber\": " << node.column_number << "\n";
        ss << "      },\n";
        ss << "      \"hitCount\": " << node.hit_count << ",\n";

        // Children array
        ss << "      \"children\": [";
        for (size_t j = 0; j < node.children.size(); ++j) {
            if (j > 0)
                ss << ", ";
            ss << node.children[j];
        }
        ss << "]";

        // Position ticks (optional, we include timing info)
        if (node.total_time_us > 0) {
            ss << ",\n      \"positionTicks\": []";
        }

        ss << "\n    }";
        if (i < data_.nodes.size() - 1)
            ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Start time (in microseconds)
    ss << "  \"startTime\": " << data_.start_time << ",\n";

    // End time
    ss << "  \"endTime\": " << data_.end_time << ",\n";

    // Samples array
    ss << "  \"samples\": [";
    for (size_t i = 0; i < data_.samples.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << data_.samples[i].node_id;
    }
    ss << "],\n";

    // Time deltas
    ss << "  \"timeDeltas\": [";
    // First delta is from start
    if (!data_.samples.empty()) {
        ss << data_.samples[0].timestamp_us;
        for (size_t i = 0; i < data_.time_deltas.size(); ++i) {
            ss << ", " << data_.time_deltas[i];
        }
    }
    ss << "]\n";

    ss << "}\n";

    return ss.str();
}

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

void tml_profiler_init(const char* output_path) {
    Profiler::instance().initialize(output_path ? output_path : "profile.cpuprofile");
}

void tml_profiler_start(void) {
    Profiler::instance().start();
}

void tml_profiler_stop(void) {
    Profiler::instance().stop();
}

void tml_profiler_enter(const char* func_name, const char* file_name, uint32_t line) {
    Profiler::instance().enter_function(func_name, file_name, line);
}

void tml_profiler_exit(void) {
    Profiler::instance().exit_function();
}

int32_t tml_profiler_is_active(void) {
    return Profiler::instance().is_active() ? 1 : 0;
}

void tml_profiler_sample(void) {
    Profiler::instance().add_sample();
}

} // extern "C"

} // namespace tml::profiler
