// Algorithm Implementations - Go
//
// Shared implementations for main.go and benchmarks

package main

// ============================================================================
// Factorial
// ============================================================================

func factorialRecursive(n int32) int32 {
	if n <= 1 {
		return 1
	}
	return n * factorialRecursive(n-1)
}

func factorialIterative(n int32) int32 {
	result := int32(1)
	for i := int32(2); i <= n; i++ {
		result *= i
	}
	return result
}

// ============================================================================
// Fibonacci
// ============================================================================

func fibonacciRecursive(n int32) int32 {
	if n <= 1 {
		return n
	}
	return fibonacciRecursive(n-1) + fibonacciRecursive(n-2)
}

func fibonacciIterative(n int32) int32 {
	if n <= 1 {
		return n
	}
	a, b := int32(0), int32(1)
	for i := int32(2); i <= n; i++ {
		a, b = b, a+b
	}
	return b
}

// ============================================================================
// GCD (Greatest Common Divisor)
// ============================================================================

func gcdRecursive(a, b int32) int32 {
	if b == 0 {
		return a
	}
	return gcdRecursive(b, a%b)
}

func gcdIterative(a, b int32) int32 {
	for b != 0 {
		a, b = b, a%b
	}
	return a
}

// ============================================================================
// Power (Fast Exponentiation)
// ============================================================================

func powerNaive(base, exp int32) int32 {
	result := int32(1)
	for i := int32(0); i < exp; i++ {
		result *= base
	}
	return result
}

func powerFast(base, exp int32) int32 {
	if exp == 0 {
		return 1
	}
	if exp == 1 {
		return base
	}
	half := powerFast(base, exp/2)
	if exp%2 == 0 {
		return half * half
	}
	return base * half * half
}

// ============================================================================
// Prime Check
// ============================================================================

func isPrime(n int32) bool {
	if n <= 1 {
		return false
	}
	if n <= 3 {
		return true
	}
	if n%2 == 0 || n%3 == 0 {
		return false
	}
	for i := int32(5); i*i <= n; i += 6 {
		if n%i == 0 || n%(i+2) == 0 {
			return false
		}
	}
	return true
}

func countPrimes(limit int32) int32 {
	count := int32(0)
	for n := int32(2); n <= limit; n++ {
		if isPrime(n) {
			count++
		}
	}
	return count
}

// ============================================================================
// Collatz Conjecture
// ============================================================================

func collatzSteps(n int32) int32 {
	steps := int32(0)
	for n != 1 {
		if n%2 == 0 {
			n /= 2
		} else {
			n = 3*n + 1
		}
		steps++
	}
	return steps
}

// ============================================================================
// Sum Range
// ============================================================================

func sumRange(start, end int32) int32 {
	sum := int32(0)
	for i := start; i <= end; i++ {
		sum += i
	}
	return sum
}
