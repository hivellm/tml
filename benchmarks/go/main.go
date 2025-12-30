// Algorithm Benchmarks - Go
//
// Run with: go run main.go

package main

import "fmt"

func main() {
	fmt.Println("=== Go Algorithm Benchmarks ===")
	fmt.Println()

	// Correctness tests
	fmt.Printf("Factorial(10): %d\n", factorialIterative(10))
	fmt.Printf("Fibonacci(20): %d\n", fibonacciIterative(20))
	fmt.Printf("GCD(48, 18): %d\n", gcdIterative(48, 18))
	fmt.Printf("Power(2, 10): %d\n", powerFast(2, 10))
	fmt.Printf("Primes up to 100: %d\n", countPrimes(100))
	fmt.Printf("Sum(1..100): %d\n", sumRange(1, 100))
	fmt.Printf("Collatz steps(27): %d\n", collatzSteps(27))
}
