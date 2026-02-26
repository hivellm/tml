#!/bin/bash
# TCP & UDP Request Round-Trip Benchmark Runner
# Runs all 4 language implementations and collects results

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
RESULTS_DIR="$SCRIPT_DIR/results"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo ""
echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}  TCP & UDP Request Round-Trip Benchmark Suite${NC}"
echo -e "${CYAN}================================================================${NC}"
echo ""

mkdir -p "$RESULTS_DIR"

# ============================================================================
# Go
# ============================================================================
echo -e "${GREEN}[1/4] Running Go benchmark...${NC}"
if command -v go &> /dev/null; then
    cd "$SCRIPT_DIR/go"
    go run tcp_udp_request_bench.go 2>&1 | tee "$RESULTS_DIR/request_bench_go.txt"
    cd "$SCRIPT_DIR"
else
    echo -e "${RED}  Go not found, skipping${NC}"
fi

# ============================================================================
# Node.js
# ============================================================================
echo -e "${GREEN}[2/4] Running Node.js benchmark...${NC}"
if command -v node &> /dev/null; then
    node "$SCRIPT_DIR/node/tcp_udp_request_bench.js" 2>&1 | tee "$RESULTS_DIR/request_bench_node.txt"
else
    echo -e "${RED}  Node.js not found, skipping${NC}"
fi

# ============================================================================
# Rust
# ============================================================================
echo -e "${GREEN}[3/4] Running Rust benchmark...${NC}"
if command -v rustc &> /dev/null; then
    RUST_BIN="/tmp/tcp_udp_request_bench_rust"
    rustc -O "$SCRIPT_DIR/rust/tcp_udp_request_bench.rs" -o "$RUST_BIN" 2>&1
    "$RUST_BIN" 2>&1 | tee "$RESULTS_DIR/request_bench_rust.txt"
else
    echo -e "${RED}  Rust not found, skipping${NC}"
fi

# ============================================================================
# TML
# ============================================================================
echo -e "${GREEN}[4/4] Running TML benchmark...${NC}"
TML_EXE="$ROOT_DIR/build/debug/tml"
if [ -f "$TML_EXE" ]; then
    "$TML_EXE" run "$SCRIPT_DIR/profile_tml/tcp_udp_request_bench.tml" --release 2>&1 | tee "$RESULTS_DIR/request_bench_tml.txt"
else
    TML_EXE="$ROOT_DIR/build/release/tml"
    if [ -f "$TML_EXE" ]; then
        "$TML_EXE" run "$SCRIPT_DIR/profile_tml/tcp_udp_request_bench.tml" --release 2>&1 | tee "$RESULTS_DIR/request_bench_tml.txt"
    else
        echo -e "${RED}  TML compiler not found, skipping${NC}"
    fi
fi

echo ""
echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}  All benchmarks complete!${NC}"
echo -e "${CYAN}  Results saved to: $RESULTS_DIR/request_bench_*.txt${NC}"
echo -e "${CYAN}================================================================${NC}"
echo ""
