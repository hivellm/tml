// Rust TCP Async Benchmark (tokio)
// Equivalent to TML async benchmark for fair comparison

use std::time::Instant;

#[tokio::main]
async fn main() {
    println!("\n================================================================");
    println!("  Rust TCP Benchmarks: Async (tokio::TcpListener)");
    println!("================================================================\n");

    let addr = "127.0.0.1:0";
    let n = 50;

    println!("=== ASYNC TCP (tokio::TcpListener) ===");
    println!("  Binding to {} ({} iterations)\n", addr, n);

    let start = Instant::now();
    let mut success = 0;

    for _ in 0..n {
        if let Ok(_listener) = tokio::net::TcpListener::bind(addr).await {
            success += 1;
        }
    }

    let elapsed = start.elapsed();
    let ns_elapsed = elapsed.as_nanos() as i64;

    println!("    Iterations: {}", n);
    println!("    Total time: {} ms", ns_elapsed / 1_000_000);
    println!("    Per op:     {} ns", if ns_elapsed > 0 { ns_elapsed / n } else { 0 });
    println!("    Ops/sec:    {}", if ns_elapsed > 0 { (n as i128 * 1_000_000_000) / ns_elapsed as i128 } else { 0 });
    println!("    Successful: {}/{}\n", success, n);

    println!("================================================================\n");
}
