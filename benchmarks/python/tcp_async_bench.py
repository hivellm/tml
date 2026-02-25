#!/usr/bin/env python3
"""Python TCP Async Benchmark (asyncio)
Proper async/await pattern matching TML async"""

import socket
import asyncio
import time

async def bench_async_tcp():
    """Async TCP bind benchmark using asyncio"""
    print("\n================================================================")
    print("  Python TCP Benchmarks: Async (asyncio)")
    print("================================================================\n")

    print("=== ASYNC TCP (asyncio - 50 concurrent) ===")
    print("  Binding to 127.0.0.1:0 (50 concurrent binds)\n")

    n = 50
    success = 0

    async def bind_async():
        """Async bind operation"""
        try:
            # Create socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

            # Non-blocking bind
            sock.bind(("127.0.0.1", 0))
            sock.close()
            return True
        except Exception:
            return False

    start = time.perf_counter()

    # Run all binds concurrently via asyncio gather
    tasks = [bind_async() for _ in range(n)]
    results = await asyncio.gather(*tasks)
    success = sum(results)

    elapsed = time.perf_counter() - start
    ns_elapsed = int(elapsed * 1_000_000_000)

    print(f"    Iterations: {n}")
    print(f"    Total time: {ns_elapsed // 1_000_000} ms")
    print(f"    Per op:     {ns_elapsed // n} ns")
    print(f"    Ops/sec:    {n * 1_000_000_000 // ns_elapsed if ns_elapsed > 0 else 0}")
    print(f"    Successful: {success}/{n}\n")

    print("================================================================\n")

if __name__ == "__main__":
    asyncio.run(bench_async_tcp())
