#!/usr/bin/env bash
# Smoke test for phase0v_package-registry-mvp.
#
# Verifies that `tml install` runs the TML-dependency resolver in
# addition to the existing native-lib scan. We only exercise the
# "no deps" and "help" paths here; cloning a real git dep is expensive
# and covered in the unit-level tests of DependencyResolver.

set -euo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
SANDBOX=".sandbox/pkg_install_test"
rm -rf "$SANDBOX"
mkdir -p "$SANDBOX"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found" >&2
    exit 1
fi

pass=0
fail=0
TML_ABS="$(cd "$(dirname "$TML")" && pwd)/$(basename "$TML")"

check() {
    local name="$1"; shift
    if "$@"; then
        printf '  PASS  %s\n' "$name"
        pass=$((pass + 1))
    else
        printf '  FAIL  %s\n' "$name"
        fail=$((fail + 1))
    fi
}

# 1. `tml install --help` lists --deps-only and --native-only.
check "install --help mentions --deps-only" \
    bash -c "'$TML_ABS' install --help > '$SANDBOX/h.txt' 2>&1 && grep -q 'deps-only' '$SANDBOX/h.txt'"

check "install --help mentions --native-only" \
    bash -c "grep -q 'native-only' '$SANDBOX/h.txt'"

# 2. `tml install` with an empty project exits 0.
mkdir -p "$SANDBOX/empty"
cat > "$SANDBOX/empty/tml.toml" <<'EOF'
[package]
name = "empty"
version = "0.1.0"
EOF
check "install on empty project exits 0" \
    bash -c "cd '$SANDBOX/empty' && '$TML_ABS' install > out.log 2>&1"

# 3. `tml install --deps-only` skips the native-lib scan and still exits 0.
check "install --deps-only exits 0 on empty project" \
    bash -c "cd '$SANDBOX/empty' && '$TML_ABS' install --deps-only > out2.log 2>&1 && ! grep -qi 'native' out2.log"

# 4. `tml install` reports the TML-dep resolution header when dependencies exist.
mkdir -p "$SANDBOX/with_path_dep" "$SANDBOX/local_lib"
cat > "$SANDBOX/local_lib/tml.toml" <<'EOF'
[package]
name = "local_lib"
version = "0.1.0"
EOF
cat > "$SANDBOX/with_path_dep/tml.toml" <<'EOF'
[package]
name = "with_path_dep"
version = "0.1.0"

[dependencies]
local_lib = { path = "../local_lib" }
EOF
check "install with path dep prints resolving header" \
    bash -c "cd '$SANDBOX/with_path_dep' && '$TML_ABS' install --verbose > out.log 2>&1 || true; grep -q 'Resolving 1 TML dependencies' out.log"

echo
echo "Summary: $pass pass, $fail fail"
exit "$fail"
