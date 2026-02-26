#!/usr/bin/env node
/**
 * Node.js TCP & UDP Request Round-Trip Benchmark
 * Measures actual request latency: client sends payload, server echoes back
 */

const net = require('net');
const dgram = require('dgram');

const N = 1000;
const PAYLOAD = Buffer.alloc(64, 0x41); // 64 bytes of 'A'

function printResults(name, iterations, nsElapsed, success) {
  const ms = Number(nsElapsed / 1_000_000n);
  const perOp = nsElapsed > 0n ? Number(nsElapsed / BigInt(iterations)) : 0;
  const opsSec = nsElapsed > 0n ? Number((BigInt(iterations) * 1_000_000_000n) / nsElapsed) : 0;

  console.log(`    Iterations: ${iterations}`);
  console.log(`    Total time: ${ms} ms`);
  console.log(`    Per op:     ${perOp} ns`);
  console.log(`    Ops/sec:    ${opsSec}`);
  console.log(`    Successful: ${success}/${iterations}\n`);
}

// ============================================================================
// Benchmark 1: TCP Bind Only (baseline)
// ============================================================================
function benchTcpBind() {
  return new Promise(resolve => {
    console.log('=== TCP Bind (baseline) ===');
    console.log(`  ${N} iterations, bind + close\n`);

    const start = process.hrtime.bigint();
    let success = 0;
    let i = 0;

    function next() {
      if (i >= N) {
        const ns = process.hrtime.bigint() - start;
        printResults('TCP Bind', N, ns, success);
        resolve();
        return;
      }
      const server = net.createServer();
      server.listen(0, '127.0.0.1', () => {
        success++;
        server.close(() => { i++; next(); });
      });
      server.on('error', () => { i++; next(); });
    }
    next();
  });
}

// ============================================================================
// Benchmark 2: UDP Bind Only (baseline)
// ============================================================================
function benchUdpBind() {
  return new Promise(resolve => {
    console.log('=== UDP Bind (baseline) ===');
    console.log(`  ${N} iterations, bind + close\n`);

    const start = process.hrtime.bigint();
    let success = 0;

    for (let i = 0; i < N; i++) {
      try {
        const sock = dgram.createSocket('udp4');
        sock.bind(0, '127.0.0.1', () => {});
        // Sync-like: bind is actually async in Node, but we measure socket creation
        sock.close();
        success++;
      } catch (e) { /* skip */ }
    }

    // UDP bind in Node is async, wait a tick
    setImmediate(() => {
      const ns = process.hrtime.bigint() - start;
      printResults('UDP Bind', N, ns, success);
      resolve();
    });
  });
}

// ============================================================================
// Benchmark 3: TCP Request on Reused Connection
// ============================================================================
function benchTcpReusedRequest() {
  return new Promise((resolve, reject) => {
    console.log('=== TCP Request (reused connection) ===');
    console.log(`  ${N} iterations, 64-byte payload, echo round-trip\n`);

    // Create echo server
    const server = net.createServer(socket => {
      socket.on('data', data => {
        socket.write(data); // echo back
      });
    });

    server.listen(0, '127.0.0.1', () => {
      const port = server.address().port;
      const client = net.createConnection({ port, host: '127.0.0.1' }, () => {
        let success = 0;
        let i = 0;
        const start = process.hrtime.bigint();

        function sendNext() {
          if (i >= N) {
            const ns = process.hrtime.bigint() - start;
            printResults('TCP Reused', N, ns, success);
            client.destroy();
            server.close(resolve);
            return;
          }
          client.write(PAYLOAD);
        }

        client.on('data', (data) => {
          if (data.length > 0) success++;
          i++;
          sendNext();
        });

        sendNext();
      });

      client.on('error', (err) => {
        console.log(`  ERROR: ${err.message}\n`);
        server.close(resolve);
      });
    });
  });
}

// ============================================================================
// Benchmark 4: UDP Request Round-Trip
// ============================================================================
function benchUdpRequest() {
  return new Promise(resolve => {
    console.log('=== UDP Request (send + recv echo) ===');
    console.log(`  ${N} iterations, 64-byte payload, echo round-trip\n`);

    const serverSock = dgram.createSocket('udp4');
    const clientSock = dgram.createSocket('udp4');

    let success = 0;
    let i = 0;
    let start;

    // Server echoes back
    serverSock.on('message', (msg, rinfo) => {
      serverSock.send(msg, rinfo.port, rinfo.address);
    });

    // Client receives echo
    clientSock.on('message', () => {
      success++;
      i++;
      if (i >= N) {
        const ns = process.hrtime.bigint() - start;
        printResults('UDP Request', N, ns, success);
        clientSock.close();
        serverSock.close(resolve);
        return;
      }
      // Send next
      clientSock.send(PAYLOAD, serverSock.address().port, '127.0.0.1');
    });

    serverSock.bind(0, '127.0.0.1', () => {
      clientSock.bind(0, '127.0.0.1', () => {
        start = process.hrtime.bigint();
        // Send first packet
        clientSock.send(PAYLOAD, serverSock.address().port, '127.0.0.1');
      });
    });
  });
}

// ============================================================================
// Main
// ============================================================================
(async () => {
  console.log('\n================================================================');
  console.log('  Node.js TCP & UDP Request Round-Trip Benchmark');
  console.log('================================================================\n');

  await benchTcpBind();
  await benchUdpBind();
  await benchTcpReusedRequest();
  await benchUdpRequest();

  console.log('================================================================');
  console.log('  Notes:');
  console.log('  - TCP reused: single connection, N send+recv round-trips');
  console.log('  - UDP request: send + recv echo round-trip');
  console.log('  - Payload: 64 bytes per request');
  console.log('  - All on 127.0.0.1 (loopback)');
  console.log('================================================================\n');
})();
