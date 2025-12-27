#!/bin/bash
# TML Compiler - Git Hooks Setup Script
# Usage: ./scripts/setup-hooks.sh
#
# Installs pre-commit and pre-push hooks to .git/hooks/

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

HOOKS_DIR="$ROOT_DIR/.git/hooks"

echo -e "${YELLOW}Setting up git hooks...${NC}"

# Check if .git directory exists
if [ ! -d "$ROOT_DIR/.git" ]; then
    echo -e "${RED}Error: Not a git repository${NC}"
    exit 1
fi

# Create hooks directory if it doesn't exist
mkdir -p "$HOOKS_DIR"

# Create pre-commit hook
cat > "$HOOKS_DIR/pre-commit" << 'HOOK_EOF'
#!/bin/bash
# TML Compiler - Pre-commit Hook
# Runs lint checks on staged TML files

set -e

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "$ROOT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Running pre-commit checks...${NC}"

# Find TML compiler
TML_EXE=""
if [ -f "./build/debug/tml" ]; then
    TML_EXE="./build/debug/tml"
elif [ -f "./build/release/tml" ]; then
    TML_EXE="./build/release/tml"
fi

# Get staged TML files
STAGED_TML=$(git diff --cached --name-only --diff-filter=ACM | grep '\.tml$' || true)

if [ -n "$STAGED_TML" ] && [ -n "$TML_EXE" ]; then
    echo -e "${YELLOW}Linting staged TML files...${NC}"
    for file in $STAGED_TML; do
        if [ -f "$file" ]; then
            "$TML_EXE" lint "$file" || {
                echo -e "${RED}Lint failed for: $file${NC}"
                echo -e "Run: ${YELLOW}$TML_EXE lint --fix $file${NC} to auto-fix"
                exit 1
            }
        fi
    done
elif [ -n "$STAGED_TML" ]; then
    echo -e "${YELLOW}TML compiler not found, using basic lint check...${NC}"
    for file in $STAGED_TML; do
        if [ -f "$file" ]; then
            if grep -q $'\t' "$file" 2>/dev/null; then
                echo -e "${RED}[TAB] $file contains tabs${NC}"
                exit 1
            fi
            if grep -q '[[:space:]]$' "$file" 2>/dev/null; then
                echo -e "${RED}[TRAIL] $file has trailing whitespace${NC}"
                exit 1
            fi
        fi
    done
fi

# Check C/C++ format if clang-format is available
STAGED_CPP=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|c|h|hpp)$' || true)

if [ -n "$STAGED_CPP" ] && command -v clang-format &> /dev/null; then
    echo -e "${YELLOW}Checking C/C++ format...${NC}"
    for file in $STAGED_CPP; do
        if [ -f "$file" ]; then
            if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
                echo -e "${RED}Format issue: $file${NC}"
                echo -e "Run: ${YELLOW}clang-format -i $file${NC} to fix"
                exit 1
            fi
        fi
    done
fi

echo -e "${GREEN}Pre-commit checks passed!${NC}"
HOOK_EOF

chmod +x "$HOOKS_DIR/pre-commit"
echo -e "${GREEN}Installed:${NC} pre-commit hook"

# Create pre-push hook
cat > "$HOOKS_DIR/pre-push" << 'HOOK_EOF'
#!/bin/bash
# TML Compiler - Pre-push Hook
# Runs build and tests before pushing

set -e

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "$ROOT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Running pre-push checks...${NC}"

# Build the project
echo -e "${YELLOW}Building project...${NC}"
if [ -f "./scripts/build.sh" ]; then
    ./scripts/build.sh --no-tests
fi

# Find TML compiler
TML_EXE=""
if [ -f "./build/debug/tml" ]; then
    TML_EXE="./build/debug/tml"
elif [ -f "./build/release/tml" ]; then
    TML_EXE="./build/release/tml"
fi

# Run TML lint on all files
if [ -n "$TML_EXE" ]; then
    echo -e "${YELLOW}Running TML lint...${NC}"
    "$TML_EXE" lint packages examples || {
        echo -e "${RED}TML lint failed!${NC}"
        echo -e "Run: ${YELLOW}$TML_EXE lint --fix${NC} to auto-fix"
        exit 1
    }
fi

# Run C++ tests
echo -e "${YELLOW}Running C++ tests...${NC}"
if [ -f "./build/debug/tml_tests" ]; then
    ./build/debug/tml_tests --gtest_brief=1
elif [ -f "./build/release/tml_tests" ]; then
    ./build/release/tml_tests --gtest_brief=1
fi

echo -e "${GREEN}Pre-push checks passed!${NC}"
HOOK_EOF

chmod +x "$HOOKS_DIR/pre-push"
echo -e "${GREEN}Installed:${NC} pre-push hook"

echo ""
echo -e "${GREEN}Git hooks installed successfully!${NC}"
echo ""
echo "Hooks configured:"
echo "  - pre-commit: Runs tml lint on staged TML files"
echo "  - pre-push: Runs build, lint, and tests before push"
echo ""
echo "To skip hooks temporarily:"
echo "  git commit --no-verify"
echo "  git push --no-verify"
