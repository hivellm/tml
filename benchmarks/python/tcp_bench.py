#!/usr/bin/env python3
"""Python TCP Socket Bind Benchmark
Equivalent to TML benchmark for fair comparison
"""

import socket
import time
import asyncio
import threading

def bench_sync_tcp():
    """Synchronous TCP bind benchmark"""
    print("\n================================================================")
    print("  Python TCP Benchmarks: Sync vs Async (Socket Bind)")
    print("================================================================\n")

    print("=== SYNC TCP (socket.socket) ===")
    print("  Binding to 127.0.0.1:0 (50 iterations)\n")

    n = 50
    start = time.perf_counter()
    success = 0

    for _ in range(n):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.bind(("127.0.0.1", 0))
            success += 1
            sock.close()
        except Exception:
            pass

    elapsed = time.perf_counter() - start
    ns_elapsed = int(elapsed * 1_000_000_000)

    print(f"    Iterations: {n}")
    print(f"    Total time: {ns_elapsed // 1_000_000} ms")
    print(f"    Per op:     {ns_elapsed // n} ns")
    print(f"    Ops/sec:    {n * 1_000_000_000 // ns_elapsed if ns_elapsed > 0 else 0}")
    print(f"    Successful: {success}/{n}\n")


def bench_threaded_tcp():
    """Threaded TCP bind benchmark (Python's concurrency model)"""
    print("=== THREADED TCP (with threading) ===")
    print("  50 concurrent binds (separate threads)\n")

    n = 50
    success = [0]
    lock = threading.Lock()

    def bind_thread():
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.bind(("127.0.0.1", 0))
            with lock:
                success[0] += 1
            sock.close()
        except Exception:
            pass

    start = time.perf_counter()
    threads = []

    for _ in range(n):
        t = threading.Thread(target=bind_thread)
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    elapsed = time.perf_counter() - start
    ns_elapsed = int(elapsed * 1_000_000_000)

    print(f"    Iterations: {n}")
    print(f"    Total time: {ns_elapsed // 1_000_000} ms")
    print(f"    Per op:     {ns_elapsed // n} ns")
    print(f"    Ops/sec:    {n * 1_000_000_000 // ns_elapsed if ns_elapsed > 0 else 0}")
    print(f"    Successful: {success[0]}/{n}\n")


async def bench_async_tcp():
    """Async TCP bind benchmark using asyncio"""
    print("=== ASYNC TCP (asyncio) ===")
    print("  50 concurrent binds (event loop)\n")

    n = 50
    success = 0

    async def bind_async():
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setblocking(False)
            sock.bind(("127.0.0.1", 0))
            sock.close()
            return True
        except Exception:
            return False

    start = time.perf_counter()

    # Run all binds concurrently
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
    bench_sync_tcp()
    bench_threaded_tcp()
    asyncio.run(bench_async_tcp())
