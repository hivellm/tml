package main

import (
	"encoding/json"
	"fmt"
	"strings"
	"time"
)

// BenchmarkResult holds the results of a benchmark
type BenchmarkResult struct {
	Name        string
	TimeUs      float64
	Iterations  int64
	ThroughputMBs float64
}

// RunBenchmark executes a benchmark function and returns timing
func RunBenchmark(name string, iterations int64, dataSize int64, fn func()) BenchmarkResult {
	// Warmup
	warmup := iterations / 10
	if warmup > 10 {
		warmup = 10
	}
	for i := int64(0); i < warmup; i++ {
		fn()
	}

	// Benchmark
	start := time.Now()
	for i := int64(0); i < iterations; i++ {
		fn()
	}
	elapsed := time.Since(start)

	totalUs := elapsed.Microseconds()
	avgUs := float64(totalUs) / float64(iterations)
	throughput := 0.0
	if dataSize > 0 {
		throughput = float64(dataSize*iterations) / (float64(totalUs) / 1e6) / (1024 * 1024)
	}

	return BenchmarkResult{
		Name:         name,
		TimeUs:       avgUs,
		Iterations:   iterations,
		ThroughputMBs: throughput,
	}
}

// PrintResult prints a benchmark result
func PrintResult(r BenchmarkResult) {
	fmt.Printf("%-40s %12.2f us %12d iters", r.Name, r.TimeUs, r.Iterations)
	if r.ThroughputMBs > 0 {
		fmt.Printf(" %12.2f MB/s", r.ThroughputMBs)
	}
	fmt.Println()
}

// GenerateSmallJSON creates a small JSON object
func GenerateSmallJSON() []byte {
	data := map[string]interface{}{
		"name":    "John Doe",
		"age":     30,
		"active":  true,
		"email":   "john@example.com",
		"scores":  []int{95, 87, 92, 88, 91},
		"address": map[string]string{
			"street": "123 Main St",
			"city":   "New York",
			"zip":    "10001",
		},
	}
	bytes, _ := json.Marshal(data)
	return bytes
}

// GenerateMediumJSON creates a medium JSON structure
func GenerateMediumJSON() []byte {
	data := map[string]interface{}{
		"users": []map[string]interface{}{
			{"id": 1, "name": "Alice", "email": "alice@example.com", "active": true},
			{"id": 2, "name": "Bob", "email": "bob@example.com", "active": false},
			{"id": 3, "name": "Charlie", "email": "charlie@example.com", "active": true},
			{"id": 4, "name": "Diana", "email": "diana@example.com", "active": true},
			{"id": 5, "name": "Eve", "email": "eve@example.com", "active": false},
		},
		"metadata": map[string]interface{}{
			"total":    5,
			"page":     1,
			"per_page": 10,
			"has_more": false,
		},
	}
	bytes, _ := json.Marshal(data)
	return bytes
}

// GenerateLargeJSON creates a large JSON structure with many entries
func GenerateLargeJSON() []byte {
	users := make([]map[string]interface{}, 100)
	for i := 0; i < 100; i++ {
		users[i] = map[string]interface{}{
			"id":     i + 1,
			"name":   fmt.Sprintf("User%d", i),
			"email":  fmt.Sprintf("user%d@example.com", i),
			"active": i%2 == 0,
			"age":    20 + i%50,
		}
	}
	data := map[string]interface{}{
		"users":    users,
		"metadata": map[string]interface{}{"count": 100},
	}
	bytes, _ := json.Marshal(data)
	return bytes
}

func main() {
	fmt.Println("\n================================================================")
	fmt.Println("  Go JSON Benchmark (encoding/json)")
	fmt.Println("================================================================\n")

	// Prepare test data
	smallJSON := GenerateSmallJSON()
	mediumJSON := GenerateMediumJSON()
	largeJSON := GenerateLargeJSON()

	fmt.Printf("%-40s %12s %12s %12s\n", "Test", "Time", "Iterations", "Throughput")
	fmt.Println(strings.Repeat("-", 80))

	// Small JSON benchmarks
	result1 := RunBenchmark("Parse Small JSON (parse_fast)", 10000, int64(len(smallJSON)), func() {
		var obj interface{}
		json.Unmarshal(smallJSON, &obj)
	})
	PrintResult(result1)

	result2 := RunBenchmark("Parse Small JSON (standard)", 10000, int64(len(smallJSON)), func() {
		var obj map[string]interface{}
		json.Unmarshal(smallJSON, &obj)
	})
	PrintResult(result2)

	// Medium JSON benchmarks
	result3 := RunBenchmark("Parse Medium JSON", 5000, int64(len(mediumJSON)), func() {
		var obj interface{}
		json.Unmarshal(mediumJSON, &obj)
	})
	PrintResult(result3)

	// Large JSON benchmarks
	result4 := RunBenchmark("Parse Large JSON (100 objects)", 100, int64(len(largeJSON)), func() {
		var obj interface{}
		json.Unmarshal(largeJSON, &obj)
	})
	PrintResult(result4)

	// Marshaling benchmarks
	data := map[string]interface{}{
		"name": "John", "age": 30, "items": []int{1, 2, 3},
	}

	result5 := RunBenchmark("Marshal Small Object", 10000, 0, func() {
		json.Marshal(data)
	})
	PrintResult(result5)

	fmt.Println(strings.Repeat("-", 80))
	fmt.Println("\n================================================================\n")
}
