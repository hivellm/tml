#!/usr/bin/env node
/**
 * JSON Benchmark - Node.js Implementation
 *
 * Compares Node.js's built-in JSON module performance.
 * Run with: node json_bench.js
 */

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

function benchmark(name, iterations, dataSize, func) {
    // Warmup
    const warmupIters = Math.min(Math.floor(iterations / 10), 10);
    for (let i = 0; i < warmupIters; i++) {
        func();
    }

    const start = process.hrtime.bigint();
    for (let i = 0; i < iterations; i++) {
        func();
    }
    const end = process.hrtime.bigint();

    const totalUs = Number(end - start) / 1000;
    const avgUs = totalUs / iterations;
    const throughput = dataSize > 0 ? (dataSize * iterations) / (totalUs / 1e6) / (1024 * 1024) : 0;

    return {
        name,
        timeUs: avgUs,
        iterations,
        throughputMbS: throughput
    };
}

function printResult(r) {
    let line = `${r.name.padEnd(40)} ${r.timeUs.toFixed(2).padStart(12)} us ${r.iterations.toString().padStart(12)} iters`;
    if (r.throughputMbS > 0) {
        line += ` ${r.throughputMbS.toFixed(2).padStart(12)} MB/s`;
    }
    console.log(line);
}

function printSeparator() {
    console.log('-'.repeat(80));
}

// ============================================================================
// Test Data Generation
// ============================================================================

function generateSmallJson() {
    return JSON.stringify({
        name: "John Doe",
        age: 30,
        active: true,
        email: "john@example.com",
        scores: [95, 87, 92, 88, 91],
        address: {
            street: "123 Main St",
            city: "New York",
            zip: "10001"
        }
    });
}

function generateMediumJson(numItems = 1000) {
    const items = [];
    for (let i = 0; i < numItems; i++) {
        items.push({
            id: i,
            name: `Item ${i}`,
            price: i * 1.5,
            active: i % 2 === 0,
            tags: ["tag1", "tag2", "tag3"]
        });
    }
    return JSON.stringify({ items });
}

function generateLargeJson(numItems = 10000) {
    const data = [];
    for (let i = 0; i < numItems; i++) {
        data.push({
            id: i,
            uuid: `550e8400-e29b-41d4-a716-446655440${String(i % 1000).padStart(3, '0')}`,
            name: `User ${i}`,
            email: `user${i}@example.com`,
            score: i * 0.1,
            metadata: {
                created: "2024-01-01",
                updated: "2024-01-02",
                version: i % 10
            },
            tags: ["alpha", "beta", "gamma", "delta"]
        });
    }
    return JSON.stringify({ data });
}

function generateDeepJson(depth = 100) {
    let result = { level: depth - 1, child: null };
    for (let i = depth - 2; i >= 0; i--) {
        result = { level: i, child: result };
    }
    return JSON.stringify(result);
}

function generateWideArray(size = 10000) {
    const arr = [];
    for (let i = 0; i < size; i++) {
        arr.push(i);
    }
    return JSON.stringify(arr);
}

function generateStringHeavyJson(numItems = 1000) {
    const strings = [];
    for (let i = 0; i < numItems; i++) {
        strings.push(`Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Item ${i}`);
    }
    return JSON.stringify({ strings });
}

// ============================================================================
// Benchmarks
// ============================================================================

function runBenchmarks() {
    console.log('\n=== Node.js JSON Module ===\n');
    printSeparator();

    const results = [];

    // Small JSON parsing
    let jsonStr = generateSmallJson();
    let r = benchmark("Node.js: Parse small JSON", 100000, jsonStr.length, () => JSON.parse(jsonStr));
    results.push(r);
    printResult(r);

    // Medium JSON parsing
    jsonStr = generateMediumJson(1000);
    r = benchmark("Node.js: Parse medium JSON (100KB)", 1000, jsonStr.length, () => JSON.parse(jsonStr));
    results.push(r);
    printResult(r);

    // Large JSON parsing
    jsonStr = generateLargeJson(10000);
    r = benchmark("Node.js: Parse large JSON (1MB)", 100, jsonStr.length, () => JSON.parse(jsonStr));
    results.push(r);
    printResult(r);

    // Deep nesting
    jsonStr = generateDeepJson(100);
    r = benchmark("Node.js: Parse deep nesting (100 levels)", 10000, jsonStr.length, () => JSON.parse(jsonStr));
    results.push(r);
    printResult(r);

    // Wide array
    jsonStr = generateWideArray(10000);
    r = benchmark("Node.js: Parse wide array (10K ints)", 1000, jsonStr.length, () => JSON.parse(jsonStr));
    results.push(r);
    printResult(r);

    // String-heavy JSON
    jsonStr = generateStringHeavyJson(1000);
    r = benchmark("Node.js: Parse string-heavy JSON", 500, jsonStr.length, () => JSON.parse(jsonStr));
    results.push(r);
    printResult(r);

    printSeparator();

    // Serialization benchmarks
    jsonStr = generateMediumJson(1000);
    let obj = JSON.parse(jsonStr);
    r = benchmark("Node.js: Serialize medium JSON", 1000, jsonStr.length, () => JSON.stringify(obj));
    results.push(r);
    printResult(r);

    jsonStr = generateLargeJson(10000);
    obj = JSON.parse(jsonStr);
    r = benchmark("Node.js: Serialize large JSON", 100, jsonStr.length, () => JSON.stringify(obj));
    results.push(r);
    printResult(r);

    jsonStr = generateMediumJson(1000);
    obj = JSON.parse(jsonStr);
    r = benchmark("Node.js: Pretty print medium JSON", 500, jsonStr.length, () => JSON.stringify(obj, null, 2));
    results.push(r);
    printResult(r);

    printSeparator();

    // Build benchmark
    r = benchmark("Node.js: Build object (1000 fields)", 10000, 0, () => {
        const obj = {};
        for (let i = 0; i < 1000; i++) {
            obj[`field${i}`] = i;
        }
        return obj;
    });
    results.push(r);
    printResult(r);

    r = benchmark("Node.js: Build array (10000 items)", 1000, 0, () => {
        const arr = [];
        for (let i = 0; i < 10000; i++) {
            arr.push(i);
        }
        return arr;
    });
    results.push(r);
    printResult(r);

    printSeparator();

    // Access patterns
    jsonStr = generateMediumJson(1000);
    obj = JSON.parse(jsonStr);
    const items = obj.items;

    r = benchmark("Node.js: Random access (1000 items)", 10000, 0, () => {
        let total = 0;
        for (const item of items) {
            total += item.id;
        }
        return total;
    });
    results.push(r);
    printResult(r);

    printSeparator();

    // Summary
    console.log('\n=== Summary ===\n');
    const totalTime = results.reduce((sum, r) => sum + r.timeUs, 0);
    console.log(`Total benchmark time: ${(totalTime / 1000).toFixed(2)} ms`);
}

// ============================================================================
// Main
// ============================================================================

function main() {
    console.log('JSON Benchmark Suite - Node.js Implementation');
    console.log('='.repeat(48));
    console.log(`Node.js version: ${process.version}`);

    // Show test data sizes
    console.log('\nTest data sizes:');
    console.log(`  Small JSON:  ${generateSmallJson().length} bytes`);
    console.log(`  Medium JSON: ${generateMediumJson(1000).length} bytes`);
    console.log(`  Large JSON:  ${generateLargeJson(10000).length} bytes`);
    console.log(`  Deep JSON:   ${generateDeepJson(100).length} bytes`);
    console.log(`  Wide Array:  ${generateWideArray(10000).length} bytes`);
    console.log(`  String-heavy:${generateStringHeavyJson(1000).length} bytes`);

    runBenchmarks();

    console.log('\nBenchmark complete.');
}

main();
