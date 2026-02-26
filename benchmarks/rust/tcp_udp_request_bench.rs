// Rust TCP & UDP Request Round-Trip Benchmark
// Measures actual request latency: client sends payload, server echoes back

use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream, UdpSocket};
use std::thread;
use std::time::Instant;

const N: usize = 1000;

fn print_results(iterations: usize, ns_elapsed: i64, success: usize) {
    let ms = ns_elapsed / 1_000_000;
    let per_op = if ns_elapsed > 0 { ns_elapsed / iterations as i64 } else { 0 };
    let ops_sec = if ns_elapsed > 0 {
        (iterations as i128 * 1_000_000_000) / ns_elapsed as i128
    } else {
        0
    };
    println!("    Iterations: {}", iterations);
    println!("    Total time: {} ms", ms);
    println!("    Per op:     {} ns", per_op);
    println!("    Ops/sec:    {}", ops_sec);
    println!("    Successful: {}/{}\n", success, iterations);
}

// ============================================================================
// Benchmark 1: TCP Bind Only (baseline)
// ============================================================================
fn bench_tcp_bind() {
    println!("=== TCP Bind (baseline) ===");
    println!("  {} iterations, bind + close\n", N);

    let start = Instant::now();
    let mut success = 0;
    for _ in 0..N {
        if let Ok(_listener) = TcpListener::bind("127.0.0.1:0") {
            success += 1;
        }
    }
    let ns = start.elapsed().as_nanos() as i64;
    print_results(N, ns, success);
}

// ============================================================================
// Benchmark 2: UDP Bind Only (baseline)
// ============================================================================
fn bench_udp_bind() {
    println!("=== UDP Bind (baseline) ===");
    println!("  {} iterations, bind + close\n", N);

    let start = Instant::now();
    let mut success = 0;
    for _ in 0..N {
        if let Ok(_socket) = UdpSocket::bind("127.0.0.1:0") {
            success += 1;
        }
    }
    let ns = start.elapsed().as_nanos() as i64;
    print_results(N, ns, success);
}

// ============================================================================
// Benchmark 3: TCP Request on Reused Connection
// ============================================================================
fn bench_tcp_reused_request() {
    println!("=== TCP Request (reused connection) ===");
    println!("  {} iterations, 64-byte payload, echo round-trip\n", N);

    let listener = match TcpListener::bind("127.0.0.1:0") {
        Ok(l) => l,
        Err(e) => {
            println!("  ERROR: {}\n", e);
            return;
        }
    };
    let server_addr = listener.local_addr().unwrap();

    // Echo server thread
    thread::spawn(move || {
        if let Ok((mut stream, _)) = listener.accept() {
            let mut buf = [0u8; 256];
            loop {
                match stream.read(&mut buf) {
                    Ok(0) => break,
                    Ok(n) => {
                        if stream.write_all(&buf[..n]).is_err() {
                            break;
                        }
                    }
                    Err(_) => break,
                }
            }
        }
    });

    // Small delay for server to start
    thread::sleep(std::time::Duration::from_millis(1));

    let mut client = match TcpStream::connect(server_addr) {
        Ok(s) => s,
        Err(e) => {
            println!("  ERROR: {}\n", e);
            return;
        }
    };

    let payload = [0x41u8; 64]; // 64 bytes of 'A'
    let mut recv_buf = [0u8; 256];
    let mut success = 0;

    let start = Instant::now();

    for _ in 0..N {
        if client.write_all(&payload).is_ok() {
            if let Ok(n) = client.read(&mut recv_buf) {
                if n > 0 {
                    success += 1;
                }
            }
        }
    }

    let ns = start.elapsed().as_nanos() as i64;
    print_results(N, ns, success);
}

// ============================================================================
// Benchmark 4: UDP Request Round-Trip
// ============================================================================
fn bench_udp_request() {
    println!("=== UDP Request (send + recv echo) ===");
    println!("  {} iterations, 64-byte payload, echo round-trip\n", N);

    let server = match UdpSocket::bind("127.0.0.1:0") {
        Ok(s) => s,
        Err(e) => {
            println!("  ERROR: {}\n", e);
            return;
        }
    };
    let server_addr = server.local_addr().unwrap();

    // Echo server thread
    let server_clone = server.try_clone().unwrap();
    thread::spawn(move || {
        let mut buf = [0u8; 256];
        loop {
            match server_clone.recv_from(&mut buf) {
                Ok((n, addr)) => {
                    let _ = server_clone.send_to(&buf[..n], addr);
                }
                Err(_) => break,
            }
        }
    });

    let client = match UdpSocket::bind("127.0.0.1:0") {
        Ok(s) => s,
        Err(e) => {
            println!("  ERROR: {}\n", e);
            return;
        }
    };

    let payload = [0x42u8; 64]; // 64 bytes of 'B'
    let mut recv_buf = [0u8; 256];
    let mut success = 0;

    // Small delay for server to start
    thread::sleep(std::time::Duration::from_millis(1));

    let start = Instant::now();

    for _ in 0..N {
        if client.send_to(&payload, server_addr).is_ok() {
            if let Ok((n, _)) = client.recv_from(&mut recv_buf) {
                if n > 0 {
                    success += 1;
                }
            }
        }
    }

    let ns = start.elapsed().as_nanos() as i64;
    print_results(N, ns, success);
}

// ============================================================================
// Main
// ============================================================================
fn main() {
    println!("\n================================================================");
    println!("  Rust TCP & UDP Request Round-Trip Benchmark");
    println!("================================================================\n");

    bench_tcp_bind();
    bench_udp_bind();
    bench_tcp_reused_request();
    bench_udp_request();

    println!("================================================================");
    println!("  Notes:");
    println!("  - TCP reused: single connection, N send+recv round-trips");
    println!("  - UDP request: send + recv echo round-trip");
    println!("  - Payload: 64 bytes per request");
    println!("  - All on 127.0.0.1 (loopback)");
    println!("================================================================\n");
}
