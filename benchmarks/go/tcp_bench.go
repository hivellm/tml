// Go TCP Socket Bind Benchmark
// Equivalent to TML benchmark for fair comparison

package main

import (
	"fmt"
	"net"
	"time"
)

func main() {
	fmt.Println("\n================================================================")
	fmt.Println("  Go TCP Benchmarks: Sync Socket Bind")
	fmt.Println("================================================================\n")

	// ========================================================================
	// Sync TCP: Listener bind overhead
	// ========================================================================
	fmt.Println("=== SYNC TCP (net.Listen) ===")
	fmt.Println("  Binding to 127.0.0.1:0 (50 iterations)\n")

	n := 50
	start := time.Now()
	success := 0
	for i := 0; i < n; i++ {
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err == nil {
			success++
			listener.Close()
		}
	}
	elapsed := time.Since(start)
	nsElapsed := int64(elapsed.Nanoseconds())

	fmt.Printf("    Iterations: %d\n", n)
	fmt.Printf("    Total time: %d ms\n", nsElapsed/1_000_000)
	fmt.Printf("    Per op:     %d ns\n", nsElapsed/int64(n))
	fmt.Printf("    Ops/sec:    %d\n", int64(n)*1_000_000_000/nsElapsed)
	fmt.Printf("    Successful: %d/%d\n\n", success, n)

	// ========================================================================
	// Goroutine spawning with channel communication (Go async model)
	// ========================================================================
	fmt.Println("=== CONCURRENT TCP (with goroutines) ===")
	fmt.Println("  1000 concurrent binds\n")

	n_concurrent := 1000
	start = time.Now()
	success = 0
	channel := make(chan bool, n_concurrent)

	for i := 0; i < n_concurrent; i++ {
		go func() {
			listener, err := net.Listen("tcp", "127.0.0.1:0")
			if err == nil {
				success++
				listener.Close()
			}
			channel <- true
		}()
	}

	// Wait for all goroutines
	for i := 0; i < n_concurrent; i++ {
		<-channel
	}

	elapsed = time.Since(start)
	nsElapsed = int64(elapsed.Nanoseconds())

	fmt.Printf("    Iterations: %d\n", n_concurrent)
	fmt.Printf("    Total time: %d ms\n", nsElapsed/1_000_000)
	if nsElapsed > 0 {
		fmt.Printf("    Per op:     %d ns\n", nsElapsed/int64(n_concurrent))
		fmt.Printf("    Ops/sec:    %d\n", int64(n_concurrent)*1_000_000_000/nsElapsed)
	}
	fmt.Printf("    Successful: %d/%d\n\n", success, n_concurrent)

	fmt.Println("================================================================\n")
}
