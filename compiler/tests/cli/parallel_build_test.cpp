// Parallel Build System Tests
// Tests for DependencyGraph, BuildQueue, ParallelBuilder

#include "../../src/cli/build_cache.hpp"
#include "../../src/cli/parallel_build.hpp"

#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

using namespace tml::cli;
namespace fs = std::filesystem;

// ============================================================================
// DependencyGraph Tests
// ============================================================================

class DependencyGraphTest : public ::testing::Test {
protected:
    DependencyGraph graph;
};

TEST_F(DependencyGraphTest, EmptyGraphNoCycles) {
    EXPECT_FALSE(graph.has_cycles());
    EXPECT_TRUE(graph.all_complete());
}

TEST_F(DependencyGraphTest, SingleFileNoDeps) {
    graph.add_file("main.tml", {});

    EXPECT_FALSE(graph.has_cycles());
    EXPECT_FALSE(graph.all_complete());

    auto ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready[0], "main.tml");
}

TEST_F(DependencyGraphTest, LinearDependencyChain) {
    // C depends on B, B depends on A
    graph.add_file("a.tml", {});
    graph.add_file("b.tml", {"a.tml"});
    graph.add_file("c.tml", {"b.tml"});

    EXPECT_FALSE(graph.has_cycles());

    // Only A should be ready initially
    auto ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready[0], "a.tml");

    // Complete A, now B should be ready
    graph.mark_complete("a.tml");
    ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready[0], "b.tml");

    // Complete B, now C should be ready
    graph.mark_complete("b.tml");
    ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready[0], "c.tml");

    // Complete C, all done
    graph.mark_complete("c.tml");
    EXPECT_TRUE(graph.all_complete());
}

TEST_F(DependencyGraphTest, DiamondDependency) {
    //       A
    //      / \
    //     B   C
    //      \ /
    //       D
    graph.add_file("a.tml", {});
    graph.add_file("b.tml", {"a.tml"});
    graph.add_file("c.tml", {"a.tml"});
    graph.add_file("d.tml", {"b.tml", "c.tml"});

    EXPECT_FALSE(graph.has_cycles());

    // Only A should be ready
    auto ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 1u);

    // Complete A, B and C should be ready
    graph.mark_complete("a.tml");
    ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 2u);

    // Complete B, D still waiting for C
    graph.mark_complete("b.tml");
    ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready[0], "c.tml");

    // Complete C, now D is ready
    graph.mark_complete("c.tml");
    ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready[0], "d.tml");
}

TEST_F(DependencyGraphTest, TopologicalSort) {
    graph.add_file("a.tml", {});
    graph.add_file("b.tml", {"a.tml"});
    graph.add_file("c.tml", {"a.tml"});
    graph.add_file("d.tml", {"b.tml", "c.tml"});

    auto sorted = graph.topological_sort();
    EXPECT_EQ(sorted.size(), 4u);

    // A must come before B, C
    // B, C must come before D
    auto pos_a = std::find(sorted.begin(), sorted.end(), "a.tml");
    auto pos_b = std::find(sorted.begin(), sorted.end(), "b.tml");
    auto pos_c = std::find(sorted.begin(), sorted.end(), "c.tml");
    auto pos_d = std::find(sorted.begin(), sorted.end(), "d.tml");

    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_a, pos_c);
    EXPECT_LT(pos_b, pos_d);
    EXPECT_LT(pos_c, pos_d);
}

TEST_F(DependencyGraphTest, ParallelFilesNoInterference) {
    // Independent files with no dependencies
    graph.add_file("a.tml", {});
    graph.add_file("b.tml", {});
    graph.add_file("c.tml", {});

    auto ready = graph.get_ready_files();
    EXPECT_EQ(ready.size(), 3u);
}

// ============================================================================
// BuildQueue Tests
// ============================================================================

class BuildQueueTest : public ::testing::Test {
protected:
    BuildQueue queue;
};

TEST_F(BuildQueueTest, EmptyQueue) {
    EXPECT_TRUE(queue.is_empty());
    EXPECT_EQ(queue.size(), 0u);
}

TEST_F(BuildQueueTest, PushAndPop) {
    auto job = std::make_shared<BuildJob>();
    job->source_file = "test.tml";

    queue.push(job);
    EXPECT_FALSE(queue.is_empty());
    EXPECT_EQ(queue.size(), 1u);

    auto popped = queue.pop();
    EXPECT_NE(popped, nullptr);
    EXPECT_EQ(popped->source_file, "test.tml");
    EXPECT_TRUE(queue.is_empty());
}

TEST_F(BuildQueueTest, FIFOOrder) {
    auto job1 = std::make_shared<BuildJob>();
    job1->source_file = "first.tml";
    auto job2 = std::make_shared<BuildJob>();
    job2->source_file = "second.tml";
    auto job3 = std::make_shared<BuildJob>();
    job3->source_file = "third.tml";

    queue.push(job1);
    queue.push(job2);
    queue.push(job3);

    EXPECT_EQ(queue.pop()->source_file, "first.tml");
    EXPECT_EQ(queue.pop()->source_file, "second.tml");
    EXPECT_EQ(queue.pop()->source_file, "third.tml");
}

TEST_F(BuildQueueTest, PopTimeoutOnEmpty) {
    // Pop with timeout should return nullptr on empty queue
    auto start = std::chrono::steady_clock::now();
    auto result = queue.pop(50); // 50ms timeout
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(result, nullptr);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(elapsed, 45); // Allow some slack
}

TEST_F(BuildQueueTest, StopQueue) {
    queue.stop();
    // After stop, pop should return quickly with nullptr
    auto result = queue.pop(1000);
    EXPECT_EQ(result, nullptr);
}

// ============================================================================
// BuildJob Tests
// ============================================================================

TEST(BuildJobTest, DefaultState) {
    BuildJob job;
    EXPECT_FALSE(job.completed);
    EXPECT_FALSE(job.failed);
    EXPECT_FALSE(job.cached);
    EXPECT_FALSE(job.queued);
    EXPECT_EQ(job.pending_deps, 0);
    EXPECT_TRUE(job.error_message.empty());
}

TEST(BuildJobTest, StateTransitions) {
    BuildJob job;
    job.source_file = "test.tml";
    job.output_file = "test.obj";

    job.pending_deps = 2;
    EXPECT_EQ(job.pending_deps, 2);

    job.pending_deps--;
    EXPECT_EQ(job.pending_deps, 1);

    job.completed = true;
    EXPECT_TRUE(job.completed);
}

// ============================================================================
// BuildStats Tests
// ============================================================================

TEST(BuildStatsTest, DefaultState) {
    BuildStats stats;
    EXPECT_EQ(stats.total_files.load(), 0);
    EXPECT_EQ(stats.completed.load(), 0);
    EXPECT_EQ(stats.failed.load(), 0);
    EXPECT_EQ(stats.cached.load(), 0);
}

TEST(BuildStatsTest, AtomicIncrements) {
    BuildStats stats;
    stats.total_files = 10;
    stats.completed++;
    stats.completed++;
    stats.cached++;

    EXPECT_EQ(stats.completed.load(), 2);
    EXPECT_EQ(stats.cached.load(), 1);
}

TEST(BuildStatsTest, Reset) {
    BuildStats stats;
    stats.total_files = 10;
    stats.completed = 5;
    stats.failed = 2;
    stats.cached = 3;

    stats.reset();

    EXPECT_EQ(stats.total_files.load(), 0);
    EXPECT_EQ(stats.completed.load(), 0);
    EXPECT_EQ(stats.failed.load(), 0);
    EXPECT_EQ(stats.cached.load(), 0);
}

TEST(BuildStatsTest, ElapsedTime) {
    BuildStats stats;
    stats.reset(); // Sets start_time

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int64_t elapsed = stats.elapsed_ms();
    EXPECT_GE(elapsed, 45);
}

// ============================================================================
// ParallelBuildOptions Tests
// ============================================================================

TEST(ParallelBuildOptionsTest, Defaults) {
    ParallelBuildOptions opts;
    EXPECT_FALSE(opts.verbose);
    EXPECT_FALSE(opts.no_cache);
    EXPECT_FALSE(opts.lto);
    EXPECT_EQ(opts.optimization_level, 0);
    EXPECT_FALSE(opts.debug_info);
    EXPECT_TRUE(opts.output_dir.empty());
    EXPECT_TRUE(opts.cache_dir.empty());
}

// ============================================================================
// PhaseTimer Tests
// ============================================================================

TEST(PhaseTimerTest, SinglePhase) {
    PhaseTimer timer;

    timer.start("compile");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.stop();

    int64_t compile_time = timer.get_timing("compile");
    EXPECT_GE(compile_time, 45000); // At least 45ms in microseconds
    EXPECT_EQ(timer.total_us(), compile_time);
}

TEST(PhaseTimerTest, MultiplePhases) {
    PhaseTimer timer;

    timer.start("parse");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.stop();

    timer.start("typecheck");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    timer.stop();

    timer.start("codegen");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.stop();

    EXPECT_GE(timer.get_timing("parse"), 15000);
    EXPECT_GE(timer.get_timing("typecheck"), 25000);
    EXPECT_GE(timer.get_timing("codegen"), 15000);
    EXPECT_GE(timer.total_us(), 60000);
}

TEST(PhaseTimerTest, NonExistentPhase) {
    PhaseTimer timer;
    EXPECT_EQ(timer.get_timing("nonexistent"), 0);
}

// ============================================================================
// ScopedPhaseTimer Tests
// ============================================================================

TEST(ScopedPhaseTimerTest, AutoStopOnDestruct) {
    PhaseTimer timer;

    {
        ScopedPhaseTimer scoped(timer, "scoped_phase");
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    } // Timer stops here

    int64_t phase_time = timer.get_timing("scoped_phase");
    EXPECT_GE(phase_time, 25000);
}

// ============================================================================
// Hash Function Tests
// ============================================================================

TEST(HashTest, SameContentSameHash) {
    std::string content1 = "func main() { print(42) }";
    std::string content2 = "func main() { print(42) }";

    std::string hash1 = hash_file_content(content1);
    std::string hash2 = hash_file_content(content2);

    EXPECT_EQ(hash1, hash2);
}

TEST(HashTest, DifferentContentDifferentHash) {
    std::string content1 = "func main() { print(42) }";
    std::string content2 = "func main() { print(43) }";

    std::string hash1 = hash_file_content(content1);
    std::string hash2 = hash_file_content(content2);

    EXPECT_NE(hash1, hash2);
}

TEST(HashTest, EmptyContent) {
    std::string hash = hash_file_content("");
    EXPECT_FALSE(hash.empty());
}

// ============================================================================
// CacheEntry Tests
// ============================================================================

TEST(CacheEntryTest, StringFieldsEmpty) {
    CacheEntry entry;
    // String fields are default-initialized to empty
    EXPECT_TRUE(entry.source_hash.empty());
    EXPECT_TRUE(entry.mir_file.empty());
    EXPECT_TRUE(entry.object_file.empty());
}

TEST(CacheEntryTest, FieldAssignment) {
    CacheEntry entry;
    entry.source_hash = "abc123";
    entry.mir_file = "/path/to/mir";
    entry.object_file = "/path/to/obj";
    entry.source_mtime = 12345;
    entry.optimization_level = 2;
    entry.debug_info = true;

    EXPECT_EQ(entry.source_hash, "abc123");
    EXPECT_EQ(entry.mir_file, "/path/to/mir");
    EXPECT_EQ(entry.object_file, "/path/to/obj");
    EXPECT_EQ(entry.source_mtime, 12345);
    EXPECT_EQ(entry.optimization_level, 2);
    EXPECT_TRUE(entry.debug_info);
}

// ============================================================================
// MirCache Tests
// ============================================================================

class MirCacheTest : public ::testing::Test {
protected:
    fs::path test_cache_dir;

    void SetUp() override {
        test_cache_dir = fs::temp_directory_path() / "tml_test_mir_cache";
        fs::create_directories(test_cache_dir);
    }

    void TearDown() override {
        if (fs::exists(test_cache_dir)) {
            fs::remove_all(test_cache_dir);
        }
    }
};

TEST_F(MirCacheTest, EmptyCacheNoValidEntry) {
    MirCache cache(test_cache_dir);

    bool has_cache = cache.has_valid_cache("/path/to/test.tml", "abc123", 2, false);
    EXPECT_FALSE(has_cache);
}

TEST_F(MirCacheTest, CacheStats) {
    MirCache cache(test_cache_dir);

    auto stats = cache.get_stats();
    EXPECT_EQ(stats.total_entries, 0u);
    EXPECT_EQ(stats.valid_entries, 0u);
}

TEST_F(MirCacheTest, Clear) {
    MirCache cache(test_cache_dir);

    // Clear should not throw on empty cache
    EXPECT_NO_THROW(cache.clear());
}

TEST_F(MirCacheTest, InvalidateNonExistent) {
    MirCache cache(test_cache_dir);

    // Invalidate should not throw for non-existent entry
    EXPECT_NO_THROW(cache.invalidate("/nonexistent/path.tml"));
}

TEST_F(MirCacheTest, LoadMirFromEmptyCache) {
    MirCache cache(test_cache_dir);

    auto mir = cache.load_mir("/path/to/test.tml");
    EXPECT_FALSE(mir.has_value());
}

TEST_F(MirCacheTest, GetCachedObjectFromEmptyCache) {
    MirCache cache(test_cache_dir);

    auto obj_path = cache.get_cached_object("/path/to/test.tml");
    EXPECT_TRUE(obj_path.empty());
}

// ============================================================================
// Source File Discovery Tests
// ============================================================================

class SourceDiscoveryTest : public ::testing::Test {
protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "tml_test_discovery";
        fs::create_directories(test_dir);
        fs::create_directories(test_dir / "subdir");
    }

    void TearDown() override {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    void create_file(const fs::path& path, const std::string& content = "") {
        std::ofstream f(path);
        f << content;
        f.close();
    }
};

TEST_F(SourceDiscoveryTest, EmptyDirectory) {
    auto files = discover_source_files(test_dir);
    EXPECT_TRUE(files.empty());
}

TEST_F(SourceDiscoveryTest, FindTmlFiles) {
    create_file(test_dir / "main.tml");
    create_file(test_dir / "lib.tml");
    create_file(test_dir / "other.txt"); // Not a .tml file

    auto files = discover_source_files(test_dir);
    EXPECT_EQ(files.size(), 2u);
}

TEST_F(SourceDiscoveryTest, RecursiveDiscovery) {
    create_file(test_dir / "main.tml");
    create_file(test_dir / "subdir" / "helper.tml");

    auto files = discover_source_files(test_dir);
    EXPECT_EQ(files.size(), 2u);
}

TEST_F(SourceDiscoveryTest, NonExistentDirectory) {
    auto files = discover_source_files(test_dir / "nonexistent");
    EXPECT_TRUE(files.empty());
}
