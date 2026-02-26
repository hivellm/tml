// Go TCP & UDP Request Round-Trip Benchmark
// Measures actual request latency: client sends payload, server echoes back

package main

import (
	"fmt"
	"net"
	"time"
)

const N = 1000

func printResults(iterations int, nsElapsed int64, success int) {
	ms := nsElapsed / 1_000_000
	perOp := int64(0)
	opsSec := int64(0)
	if nsElapsed > 0 {
		perOp = nsElapsed / int64(iterations)
		opsSec = int64(iterations) * 1_000_000_000 / nsElapsed
	}
	fmt.Printf("    Iterations: %d\n", iterations)
	fmt.Printf("    Total time: %d ms\n", ms)
	fmt.Printf("    Per op:     %d ns\n", perOp)
	fmt.Printf("    Ops/sec:    %d\n", opsSec)
	fmt.Printf("    Successful: %d/%d\n\n", success, iterations)
}

// ============================================================================
// Benchmark 1: TCP Bind Only (baseline)
// ============================================================================
func benchTcpBind() {
	fmt.Println("=== TCP Bind (baseline) ===")
	fmt.Printf("  %d iterations, bind + close\n\n", N)

	start := time.Now()
	success := 0
	for i := 0; i < N; i++ {
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err == nil {
			success++
			listener.Close()
		}
	}
	ns := time.Since(start).Nanoseconds()
	printResults(N, ns, success)
}

// ============================================================================
// Benchmark 2: UDP Bind Only (baseline)
// ============================================================================
func benchUdpBind() {
	fmt.Println("=== UDP Bind (baseline) ===")
	fmt.Printf("  %d iterations, bind + close\n\n", N)

	start := time.Now()
	success := 0
	for i := 0; i < N; i++ {
		conn, err := net.ListenPacket("udp", "127.0.0.1:0")
		if err == nil {
			success++
			conn.Close()
		}
	}
	ns := time.Since(start).Nanoseconds()
	printResults(N, ns, success)
}

// ============================================================================
// Benchmark 3: TCP Request on Reused Connection
// ============================================================================
func benchTcpReusedRequest() {
	fmt.Println("=== TCP Request (reused connection) ===")
	fmt.Printf("  %d iterations, 64-byte payload, echo round-trip\n\n", N)

	// Start echo server
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Printf("  ERROR: %v\n\n", err)
		return
	}
	defer listener.Close()
	serverAddr := listener.Addr().String()

	// Echo server goroutine
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			return
		}
		defer conn.Close()
		buf := make([]byte, 256)
		for {
			n, err := conn.Read(buf)
			if err != nil || n == 0 {
				return
			}
			_, err = conn.Write(buf[:n])
			if err != nil {
				return
			}
		}
	}()

	// Give server time to start
	time.Sleep(1 * time.Millisecond)

	// Connect client
	client, err := net.Dial("tcp", serverAddr)
	if err != nil {
		fmt.Printf("  ERROR: %v\n\n", err)
		return
	}
	defer client.Close()

	payload := make([]byte, 64)
	for j := range payload {
		payload[j] = 0x41 // 'A'
	}
	recvBuf := make([]byte, 256)
	success := 0

	start := time.Now()

	for i := 0; i < N; i++ {
		_, err := client.Write(payload)
		if err != nil {
			continue
		}
		n, err := client.Read(recvBuf)
		if err == nil && n > 0 {
			success++
		}
	}

	ns := time.Since(start).Nanoseconds()
	printResults(N, ns, success)
}

// ============================================================================
// Benchmark 4: UDP Request Round-Trip
// ============================================================================
func benchUdpRequest() {
	fmt.Println("=== UDP Request (send + recv echo) ===")
	fmt.Printf("  %d iterations, 64-byte payload, echo round-trip\n\n", N)

	// Bind server
	serverConn, err := net.ListenPacket("udp", "127.0.0.1:0")
	if err != nil {
		fmt.Printf("  ERROR: %v\n\n", err)
		return
	}
	defer serverConn.Close()
	serverAddr := serverConn.LocalAddr()

	// Echo server goroutine
	go func() {
		buf := make([]byte, 256)
		for {
			n, addr, err := serverConn.ReadFrom(buf)
			if err != nil {
				return
			}
			serverConn.WriteTo(buf[:n], addr)
		}
	}()

	// Bind client
	clientConn, err := net.ListenPacket("udp", "127.0.0.1:0")
	if err != nil {
		fmt.Printf("  ERROR: %v\n\n", err)
		return
	}
	defer clientConn.Close()

	payload := make([]byte, 64)
	for j := range payload {
		payload[j] = 0x42 // 'B'
	}
	recvBuf := make([]byte, 256)
	success := 0

	// Give server time to start
	time.Sleep(1 * time.Millisecond)

	start := time.Now()

	for i := 0; i < N; i++ {
		_, err := clientConn.WriteTo(payload, serverAddr)
		if err != nil {
			continue
		}
		n, _, err := clientConn.ReadFrom(recvBuf)
		if err == nil && n > 0 {
			success++
		}
	}

	ns := time.Since(start).Nanoseconds()
	printResults(N, ns, success)
}

// ============================================================================
// Main
// ============================================================================
func main() {
	fmt.Println("\n================================================================")
	fmt.Println("  Go TCP & UDP Request Round-Trip Benchmark")
	fmt.Println("================================================================\n")

	benchTcpBind()
	benchUdpBind()
	benchTcpReusedRequest()
	benchUdpRequest()

	fmt.Println("================================================================")
	fmt.Println("  Notes:")
	fmt.Println("  - TCP reused: single connection, N send+recv round-trips")
	fmt.Println("  - UDP request: send + recv echo round-trip")
	fmt.Println("  - Payload: 64 bytes per request")
	fmt.Println("  - All on 127.0.0.1 (loopback)")
	fmt.Println("================================================================\n")
}
