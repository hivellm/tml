#!/bin/bash
# Integration test for PackageRegistry — workspace member resolution by
# package name. Verifies that tml.exe can resolve imports of the form
# `use <package_name>::...` against a workspace defined in a temp tml.toml
# tree, including for members living outside `lib/`.

set -e

TML_BIN="${TML_BIN:-./build/debug/bin/tml.exe}"
WS="$(mktemp -d -t tml_pkg_reg_XXXXXX)"
trap 'rm -rf "$WS"' EXIT

echo "=== PackageRegistry integration test ==="
echo "workspace: $WS"

# 1. Workspace with a member outside lib/ (mirrors compiler-tml).
mkdir -p "$WS/tools/quirky/src"
cat > "$WS/tml.toml" <<'EOF'
[package]
name = "wsroot"
version = "0.1.0"
edition = "2025"

[workspace]
members = ["tools/quirky"]
EOF

cat > "$WS/tools/quirky/tml.toml" <<'EOF'
[package]
name = "quirky"
version = "0.1.0"
edition = "2025"
EOF

cat > "$WS/tools/quirky/src/answer.tml" <<'EOF'
pub func answer() -> I64 {
    return 42
}
EOF

# 2. Consumer imports the package by its [package].name.
cat > "$WS/consumer.tml" <<'EOF'
use quirky::answer::answer

pub func main() {
    let _ = answer()
}
EOF

# 3. tml check from workspace root must resolve `quirky::answer::answer`.
ABS_TML="$(pwd)/$TML_BIN"
pushd "$WS" >/dev/null
if "$ABS_TML" check consumer.tml; then
    echo "PASS: workspace member resolved by package name"
else
    echo "FAIL: tml check could not resolve quirky::answer"
    popd >/dev/null
    exit 1
fi
popd >/dev/null

# 4. Negative case: consumer that imports an unknown package must fail.
cat > "$WS/bad.tml" <<'EOF'
use notreal::nope::thing

pub func main() {}
EOF
pushd "$WS" >/dev/null
if "$ABS_TML" check bad.tml 2>/dev/null; then
    echo "FAIL: unknown package should have errored"
    popd >/dev/null
    exit 1
else
    echo "PASS: unknown package correctly rejected"
fi
popd >/dev/null

echo
echo "=== All PackageRegistry tests passed ==="
