#!/usr/bin/env node
/**
 * Node.js TCP Socket Bind Benchmark
 * Equivalent to TML benchmark for fair comparison
 */

const net = require('net');

console.log('\n================================================================');
console.log('  Node.js TCP Benchmarks: Sync vs Async (Socket Bind)');
console.log('================================================================\n');

// ========================================================================
// Sync-like TCP: Sequential binding
// ========================================================================
console.log('=== SEQUENTIAL TCP (net.Server) ===');
console.log('  Binding to 127.0.0.1:0 (50 iterations)\n');

const n = 50;

function benchSync() {
  return new Promise(resolve => {
    const start = process.hrtime.bigint();
    let success = 0;

    function bindNext(i) {
      if (i >= n) {
        const end = process.hrtime.bigint();
        const nsElapsed = end - start;

        console.log(`    Iterations: ${n}`);
        console.log(`    Total time: ${Number(nsElapsed / 1_000_000n)} ms`);
        console.log(`    Per op:     ${Number(nsElapsed / BigInt(n))} ns`);
        console.log(`    Ops/sec:    ${nsElapsed > 0n ? Number((BigInt(n) * 1_000_000_000n) / nsElapsed) : 0}`);
        console.log(`    Successful: ${success}/${n}\n`);
        resolve();
        return;
      }

      const server = net.createServer();
      server.listen(0, '127.0.0.1', () => {
        success++;
        server.close(() => bindNext(i + 1));
      });
      server.on('error', () => bindNext(i + 1));
    }

    bindNext(0);
  });
}

// ========================================================================
// Concurrent TCP: All at once
// ========================================================================
async function benchConcurrent() {
  console.log('=== CONCURRENT TCP (Promise.all) ===');
  console.log('  50 concurrent binds\n');

  const start = process.hrtime.bigint();
  let success = 0;

  const promises = Array(n).fill().map(() => {
    return new Promise(resolve => {
      const server = net.createServer();
      server.listen(0, '127.0.0.1', () => {
        success++;
        server.close(resolve);
      });
      server.on('error', resolve);
    });
  });

  await Promise.all(promises);

  const end = process.hrtime.bigint();
  const nsElapsed = end - start;

  console.log(`    Iterations: ${n}`);
  console.log(`    Total time: ${Number(nsElapsed / 1_000_000n)} ms`);
  console.log(`    Per op:     ${Number(nsElapsed / BigInt(n))} ns`);
  console.log(`    Ops/sec:    ${nsElapsed > 0n ? Number((BigInt(n) * 1_000_000_000n) / nsElapsed) : 0}`);
  console.log(`    Successful: ${success}/${n}\n`);

  console.log('================================================================\n');
}

(async () => {
  await benchSync();
  await benchConcurrent();
})();
