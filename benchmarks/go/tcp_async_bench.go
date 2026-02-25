// Go TCP Async Benchmark (context-based, simulating async patterns)
// Go doesn't have true "async" like Rust, uses goroutines instead
// This shows concurrent binds via goroutines

package main

import (
	"context"
	"fmt"
	"net"
	"sync"
	"time"
)

func main() {
	fmt.Println("\n================================================================")
	fmt.Println("  Go TCP Benchmarks: Concurrent (goroutines)")
	fmt.Println("================================================================\n")

	fmt.Println("=== CONCURRENT TCP (50 goroutines) ===")
	fmt.Println("  Binding to 127.0.0.1:0 (50 concurrent binds)\n")

	n := 50
	start := time.Now()
	success := 0
	var mu sync.Mutex
	var wg sync.WaitGroup

	for i := 0; i < n; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()

			// Simulate async bind with context
			listener, err := net.Listen("tcp", "127.0.0.1:0")
			if err == nil && ctx.Err() == nil {
				mu.Lock()
				success++
				mu.Unlock()
				listener.Close()
			}
		}()
	}

	wg.Wait()
	elapsed := time.Since(start)
	nsElapsed := int64(elapsed.Nanoseconds())

	fmt.Printf("    Iterations: %d\n", n)
	fmt.Printf("    Total time: %d ms\n", nsElapsed/1_000_000)
	if nsElapsed > 0 {
		fmt.Printf("    Per op:     %d ns\n", nsElapsed/int64(n))
		fmt.Printf("    Ops/sec:    %d\n", int64(n)*1_000_000_000/nsElapsed)
	}
	fmt.Printf("    Successful: %d/%d\n\n", success, n)

	fmt.Println("================================================================\n")
}
