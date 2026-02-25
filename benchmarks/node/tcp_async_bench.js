#!/usr/bin/env node
/**
 * Node.js TCP Async Benchmark (proper Promise.all async pattern)
 */

const net = require('net');

console.log('\n================================================================');
console.log('  Node.js TCP Benchmarks: Async (Promise.all)');
console.log('================================================================\n');

async function benchAsync() {
  console.log('=== ASYNC TCP (Promise.all - 50 concurrent) ===');
  console.log('  Binding to 127.0.0.1:0 (50 concurrent binds)\n');

  const n = 50;
  const start = process.hrtime.bigint();
  let success = 0;

  // Create 50 concurrent bind operations
  const promises = Array(n).fill().map(() => {
    return new Promise(resolve => {
      const server = net.createServer();

      server.listen(0, '127.0.0.1', () => {
        success++;
        server.close(resolve);
      });

      server.on('error', resolve);

      // Timeout after 5 seconds
      setTimeout(() => {
        server.close(resolve);
      }, 5000);
    });
  });

  // Wait for all to complete
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

benchAsync().catch(console.error);
