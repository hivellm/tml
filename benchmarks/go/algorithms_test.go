// Algorithm Benchmarks - Go
//
// Run with: go test -bench=. -benchmem

package main

import "testing"

// ============================================================================
// Benchmarks
// ============================================================================

func BenchmarkFactorialRecursive10(b *testing.B) {
	for i := 0; i < b.N; i++ {
		factorialRecursive(10)
	}
}

func BenchmarkFactorialIterative10(b *testing.B) {
	for i := 0; i < b.N; i++ {
		factorialIterative(10)
	}
}

func BenchmarkFibonacciRecursive20(b *testing.B) {
	for i := 0; i < b.N; i++ {
		fibonacciRecursive(20)
	}
}

func BenchmarkFibonacciIterative20(b *testing.B) {
	for i := 0; i < b.N; i++ {
		fibonacciIterative(20)
	}
}

func BenchmarkGCDRecursive(b *testing.B) {
	for i := 0; i < b.N; i++ {
		gcdRecursive(48, 18)
	}
}

func BenchmarkGCDIterative(b *testing.B) {
	for i := 0; i < b.N; i++ {
		gcdIterative(48, 18)
	}
}

func BenchmarkPowerNaive2_10(b *testing.B) {
	for i := 0; i < b.N; i++ {
		powerNaive(2, 10)
	}
}

func BenchmarkPowerFast2_10(b *testing.B) {
	for i := 0; i < b.N; i++ {
		powerFast(2, 10)
	}
}

func BenchmarkCountPrimes100(b *testing.B) {
	for i := 0; i < b.N; i++ {
		countPrimes(100)
	}
}

func BenchmarkCountPrimes1000(b *testing.B) {
	for i := 0; i < b.N; i++ {
		countPrimes(1000)
	}
}

func BenchmarkCollatz27(b *testing.B) {
	for i := 0; i < b.N; i++ {
		collatzSteps(27)
	}
}

func BenchmarkSumRange1_100(b *testing.B) {
	for i := 0; i < b.N; i++ {
		sumRange(1, 100)
	}
}

func BenchmarkSumRange1_10000(b *testing.B) {
	for i := 0; i < b.N; i++ {
		sumRange(1, 10000)
	}
}
