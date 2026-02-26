#!/bin/bash
# =============================================================================
# TML Compiler — Dependency Installation & Environment Setup
# =============================================================================
# Installs all dependencies for macOS (Homebrew) and Linux (apt/dnf/pacman),
# builds the compiler, and configures environment variables + MCP server.
#
# Usage:
#   chmod +x scripts/install-deps.sh
#   ./scripts/install-deps.sh
#
# Options:
#   --skip-build     Skip building the compiler after installing deps
#   --skip-env       Skip shell environment setup
#   --skip-mcp       Skip MCP server configuration
#   --shell=<shell>  Force shell (bash, zsh, fish). Auto-detected by default.
#   --help           Show this help
# =============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
DIM='\033[2m'
BOLD='\033[1m'
NC='\033[0m'

# Get project root (parent of scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TML_ROOT="$(dirname "$SCRIPT_DIR")"

# Defaults
SKIP_BUILD=false
SKIP_ENV=false
SKIP_MCP=false
FORCE_SHELL=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build) SKIP_BUILD=true; shift ;;
        --skip-env)   SKIP_ENV=true; shift ;;
        --skip-mcp)   SKIP_MCP=true; shift ;;
        --shell=*)    FORCE_SHELL="${1#*=}"; shift ;;
        --help|-h)
            echo "TML Dependency Installer"
            echo ""
            echo "Usage: ./scripts/install-deps.sh [options]"
            echo ""
            echo "Options:"
            echo "  --skip-build   Skip building the compiler"
            echo "  --skip-env     Skip shell environment configuration"
            echo "  --skip-mcp     Skip MCP server setup"
            echo "  --shell=SHELL  Force shell type (bash, zsh, fish)"
            echo "  --help         Show this help"
            exit 0
            ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; exit 1 ;;
    esac
done

# =============================================================================
# Utility functions
# =============================================================================

log_section() {
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

log_ok()   { echo -e "  ${GREEN}[OK]${NC} $1"; }
log_warn() { echo -e "  ${YELLOW}[WARN]${NC} $1"; }
log_err()  { echo -e "  ${RED}[ERR]${NC} $1"; }
log_info() { echo -e "  ${DIM}[..]${NC} $1"; }

check_cmd() {
    if command -v "$1" &>/dev/null; then
        return 0
    else
        return 1
    fi
}

# =============================================================================
# Detect OS and package manager
# =============================================================================

log_section "Detecting system"

OS="$(uname -s)"
ARCH="$(uname -m)"
PKG_MANAGER=""

case "$OS" in
    Darwin)
        OS_NAME="macOS"
        if check_cmd brew; then
            PKG_MANAGER="brew"
            log_ok "macOS detected with Homebrew"
        else
            log_err "Homebrew is required on macOS. Install it first:"
            echo "    /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
            exit 1
        fi
        ;;
    Linux)
        OS_NAME="Linux"
        if check_cmd apt-get; then
            PKG_MANAGER="apt"
            log_ok "Linux detected with apt (Debian/Ubuntu)"
        elif check_cmd dnf; then
            PKG_MANAGER="dnf"
            log_ok "Linux detected with dnf (Fedora/RHEL)"
        elif check_cmd pacman; then
            PKG_MANAGER="pacman"
            log_ok "Linux detected with pacman (Arch)"
        else
            log_err "No supported package manager found (apt, dnf, pacman)"
            exit 1
        fi
        ;;
    *)
        log_err "Unsupported OS: $OS"
        exit 1
        ;;
esac

log_info "Architecture: $ARCH"

# =============================================================================
# Detect user shell
# =============================================================================

detect_shell() {
    if [ -n "$FORCE_SHELL" ]; then
        echo "$FORCE_SHELL"
        return
    fi

    local user_shell
    user_shell="$(basename "${SHELL:-/bin/bash}")"

    case "$user_shell" in
        bash|zsh|fish) echo "$user_shell" ;;
        *) echo "bash" ;;
    esac
}

USER_SHELL=$(detect_shell)
log_info "Shell: $USER_SHELL"

get_shell_rc() {
    case "$1" in
        bash)
            if [ "$OS" = "Darwin" ]; then
                echo "$HOME/.bash_profile"
            else
                echo "$HOME/.bashrc"
            fi
            ;;
        zsh)  echo "$HOME/.zshrc" ;;
        fish) echo "$HOME/.config/fish/config.fish" ;;
    esac
}

SHELL_RC=$(get_shell_rc "$USER_SHELL")
log_info "Shell config: $SHELL_RC"

# =============================================================================
# Install system dependencies
# =============================================================================

log_section "Installing dependencies"

install_macos() {
    log_info "Updating Homebrew..."
    brew update --quiet

    # LLVM (includes clang, lld, llvm-config)
    if brew list llvm &>/dev/null; then
        log_ok "LLVM already installed"
    else
        log_info "Installing LLVM (this may take a while)..."
        brew install llvm
        log_ok "LLVM installed"
    fi

    # CMake
    if check_cmd cmake; then
        log_ok "CMake already installed ($(cmake --version | head -1))"
    else
        log_info "Installing CMake..."
        brew install cmake
        log_ok "CMake installed"
    fi

    # OpenSSL
    if brew list openssl@3 &>/dev/null || brew list openssl &>/dev/null; then
        log_ok "OpenSSL already installed"
    else
        log_info "Installing OpenSSL..."
        brew install openssl@3
        log_ok "OpenSSL installed"
    fi

    # zlib (usually comes with macOS, but install explicitly)
    if brew list zlib &>/dev/null; then
        log_ok "zlib already installed"
    else
        log_info "Installing zlib..."
        brew install zlib
        log_ok "zlib installed"
    fi

    # Brotli (optional, for compression support)
    if brew list brotli &>/dev/null; then
        log_ok "Brotli already installed"
    else
        log_info "Installing Brotli..."
        brew install brotli
        log_ok "Brotli installed"
    fi

    # Zstd (optional, for compression support)
    if brew list zstd &>/dev/null; then
        log_ok "Zstd already installed"
    else
        log_info "Installing Zstd..."
        brew install zstd
        log_ok "Zstd installed"
    fi

    # Node.js (for Rulebook MCP server via npx)
    if check_cmd node; then
        log_ok "Node.js already installed ($(node --version))"
    else
        log_info "Installing Node.js..."
        brew install node
        log_ok "Node.js installed"
    fi

    # Detect LLVM paths
    if [ -d "/opt/homebrew/opt/llvm" ]; then
        BREW_LLVM="/opt/homebrew/opt/llvm"
    elif [ -d "/usr/local/opt/llvm" ]; then
        BREW_LLVM="/usr/local/opt/llvm"
    else
        BREW_LLVM=""
        log_warn "Could not find Homebrew LLVM installation path"
    fi
}

install_linux_apt() {
    log_info "Updating package lists..."
    sudo apt-get update -qq

    # Build essentials
    sudo apt-get install -y -qq build-essential

    # CMake
    if check_cmd cmake; then
        log_ok "CMake already installed ($(cmake --version | head -1))"
    else
        log_info "Installing CMake..."
        sudo apt-get install -y -qq cmake
        log_ok "CMake installed"
    fi

    # LLVM + LLD (try llvm-18 first, then llvm-17, then default)
    local llvm_installed=false
    for ver in 18 17; do
        if dpkg -l "llvm-${ver}-dev" &>/dev/null 2>&1; then
            log_ok "LLVM ${ver} already installed"
            llvm_installed=true
            break
        fi
    done

    if [ "$llvm_installed" = false ]; then
        log_info "Installing LLVM + LLD..."
        # Try versioned packages first
        if apt-cache show llvm-18-dev &>/dev/null 2>&1; then
            sudo apt-get install -y -qq llvm-18-dev lld-18 clang-18 libclang-18-dev liblld-18-dev
            log_ok "LLVM 18 installed"
        elif apt-cache show llvm-17-dev &>/dev/null 2>&1; then
            sudo apt-get install -y -qq llvm-17-dev lld-17 clang-17 libclang-17-dev liblld-17-dev
            log_ok "LLVM 17 installed"
        else
            sudo apt-get install -y -qq llvm-dev lld clang libclang-dev liblld-dev
            log_ok "LLVM installed (default version)"
        fi
    fi

    # OpenSSL
    if dpkg -l libssl-dev &>/dev/null 2>&1; then
        log_ok "OpenSSL dev already installed"
    else
        log_info "Installing OpenSSL dev..."
        sudo apt-get install -y -qq libssl-dev
        log_ok "OpenSSL dev installed"
    fi

    # zlib
    if dpkg -l zlib1g-dev &>/dev/null 2>&1; then
        log_ok "zlib dev already installed"
    else
        log_info "Installing zlib dev..."
        sudo apt-get install -y -qq zlib1g-dev
        log_ok "zlib dev installed"
    fi

    # Brotli
    if dpkg -l libbrotli-dev &>/dev/null 2>&1; then
        log_ok "Brotli dev already installed"
    else
        log_info "Installing Brotli dev..."
        sudo apt-get install -y -qq libbrotli-dev
        log_ok "Brotli dev installed"
    fi

    # Zstd
    if dpkg -l libzstd-dev &>/dev/null 2>&1; then
        log_ok "Zstd dev already installed"
    else
        log_info "Installing Zstd dev..."
        sudo apt-get install -y -qq libzstd-dev
        log_ok "Zstd dev installed"
    fi

    # Node.js
    if check_cmd node; then
        log_ok "Node.js already installed ($(node --version))"
    else
        log_info "Installing Node.js..."
        sudo apt-get install -y -qq nodejs npm
        log_ok "Node.js installed"
    fi
}

install_linux_dnf() {
    log_info "Installing dependencies via dnf..."

    sudo dnf install -y gcc gcc-c++ cmake make

    # LLVM
    if check_cmd llvm-config; then
        log_ok "LLVM already installed"
    else
        log_info "Installing LLVM + LLD..."
        sudo dnf install -y llvm-devel lld-devel clang-devel
        log_ok "LLVM installed"
    fi

    # OpenSSL
    sudo dnf install -y openssl-devel
    log_ok "OpenSSL dev installed"

    # zlib
    sudo dnf install -y zlib-devel
    log_ok "zlib dev installed"

    # Brotli
    sudo dnf install -y brotli-devel
    log_ok "Brotli dev installed"

    # Zstd
    sudo dnf install -y libzstd-devel
    log_ok "Zstd dev installed"

    # Node.js
    if check_cmd node; then
        log_ok "Node.js already installed ($(node --version))"
    else
        sudo dnf install -y nodejs npm
        log_ok "Node.js installed"
    fi
}

install_linux_pacman() {
    log_info "Installing dependencies via pacman..."

    sudo pacman -Sy --noconfirm --needed base-devel cmake

    # LLVM
    if check_cmd llvm-config; then
        log_ok "LLVM already installed"
    else
        log_info "Installing LLVM + LLD..."
        sudo pacman -S --noconfirm --needed llvm lld clang
        log_ok "LLVM installed"
    fi

    # OpenSSL
    sudo pacman -S --noconfirm --needed openssl
    log_ok "OpenSSL installed"

    # zlib
    sudo pacman -S --noconfirm --needed zlib
    log_ok "zlib installed"

    # Brotli
    sudo pacman -S --noconfirm --needed brotli
    log_ok "Brotli installed"

    # Zstd
    sudo pacman -S --noconfirm --needed zstd
    log_ok "Zstd installed"

    # Node.js
    if check_cmd node; then
        log_ok "Node.js already installed ($(node --version))"
    else
        sudo pacman -S --noconfirm --needed nodejs npm
        log_ok "Node.js installed"
    fi
}

case "$PKG_MANAGER" in
    brew)   install_macos ;;
    apt)    install_linux_apt ;;
    dnf)    install_linux_dnf ;;
    pacman) install_linux_pacman ;;
esac

# =============================================================================
# Verify critical dependencies
# =============================================================================

log_section "Verifying installation"

ERRORS=0

# CMake >= 3.20
if check_cmd cmake; then
    CMAKE_VER=$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+')
    CMAKE_MAJOR=$(echo "$CMAKE_VER" | cut -d. -f1)
    CMAKE_MINOR=$(echo "$CMAKE_VER" | cut -d. -f2)
    if [ "$CMAKE_MAJOR" -ge 3 ] && [ "$CMAKE_MINOR" -ge 20 ]; then
        log_ok "CMake $CMAKE_VER (>= 3.20)"
    else
        log_warn "CMake $CMAKE_VER found, but >= 3.20 is recommended"
    fi
else
    log_err "CMake not found"
    ERRORS=$((ERRORS + 1))
fi

# C++ compiler with C++20
if check_cmd clang++; then
    log_ok "clang++ found ($(clang++ --version | head -1))"
elif check_cmd g++; then
    log_ok "g++ found ($(g++ --version | head -1))"
else
    log_err "No C++20 compiler found (clang++ or g++)"
    ERRORS=$((ERRORS + 1))
fi

# LLVM
LLVM_FOUND=false
if [ "$OS" = "Darwin" ] && [ -n "$BREW_LLVM" ]; then
    if [ -f "$BREW_LLVM/lib/cmake/llvm/LLVMConfig.cmake" ]; then
        log_ok "LLVM found at $BREW_LLVM"
        LLVM_FOUND=true
    fi
elif check_cmd llvm-config; then
    log_ok "LLVM found ($(llvm-config --version))"
    LLVM_FOUND=true
else
    # Check common Linux paths
    for ver in 18 17; do
        if [ -f "/usr/lib/llvm-${ver}/lib/cmake/llvm/LLVMConfig.cmake" ]; then
            log_ok "LLVM ${ver} found at /usr/lib/llvm-${ver}"
            LLVM_FOUND=true
            break
        fi
    done
fi

if [ "$LLVM_FOUND" = false ]; then
    log_err "LLVM not found"
    ERRORS=$((ERRORS + 1))
fi

# Node.js
if check_cmd node; then
    log_ok "Node.js $(node --version)"
else
    log_warn "Node.js not found (needed for Rulebook MCP server)"
fi

# npx
if check_cmd npx; then
    log_ok "npx available"
else
    log_warn "npx not found (needed for Rulebook MCP server)"
fi

if [ $ERRORS -gt 0 ]; then
    echo ""
    log_err "$ERRORS critical dependencies missing. Fix them and re-run."
    exit 1
fi

# =============================================================================
# Build the compiler
# =============================================================================

if [ "$SKIP_BUILD" = false ]; then
    log_section "Building TML compiler"

    cd "$TML_ROOT"

    log_info "Running: scripts/build.sh"
    echo ""

    bash scripts/build.sh

    # Verify build output
    echo ""
    if [ -f "$TML_ROOT/build/debug/tml" ]; then
        log_ok "Compiler built: build/debug/tml"
    else
        log_err "Compiler binary not found at build/debug/tml"
        log_info "Check build output above for errors"
    fi

    if [ -f "$TML_ROOT/build/debug/tml_mcp" ]; then
        log_ok "MCP server built: build/debug/tml_mcp"
    else
        log_warn "MCP server binary not found at build/debug/tml_mcp"
    fi
else
    log_info "Skipping build (--skip-build)"
fi

# =============================================================================
# Configure shell environment
# =============================================================================

if [ "$SKIP_ENV" = false ]; then
    log_section "Configuring environment"

    TML_BIN_DIR="$TML_ROOT/build/debug"
    MARKER="# >>> TML compiler environment >>>"
    MARKER_END="# <<< TML compiler environment <<<"

    # Remove old TML config block if it exists
    remove_old_block() {
        local file="$1"
        if [ -f "$file" ] && grep -q "$MARKER" "$file"; then
            log_info "Removing old TML config from $file"
            sed -i.bak "/$MARKER/,/$MARKER_END/d" "$file"
        fi
    }

    if [ "$USER_SHELL" = "fish" ]; then
        # Fish shell config
        FISH_CONFIG="$HOME/.config/fish/config.fish"
        mkdir -p "$(dirname "$FISH_CONFIG")"

        remove_old_block "$FISH_CONFIG"

        cat >> "$FISH_CONFIG" << FISHEOF
$MARKER
set -gx TML_ROOT "$TML_ROOT"
fish_add_path "$TML_BIN_DIR"
$MARKER_END
FISHEOF

        log_ok "Added TML to fish config ($FISH_CONFIG)"

    else
        # Bash / Zsh
        remove_old_block "$SHELL_RC"

        cat >> "$SHELL_RC" << SHEOF
$MARKER
export TML_ROOT="$TML_ROOT"
export PATH="$TML_BIN_DIR:\$PATH"
$MARKER_END
SHEOF

        log_ok "Added TML to $SHELL_RC"
    fi

    log_info "TML_ROOT=$TML_ROOT"
    log_info "PATH += $TML_BIN_DIR"
    echo ""
    log_info "Run the following to activate in current session:"

    if [ "$USER_SHELL" = "fish" ]; then
        echo -e "    ${BOLD}source ~/.config/fish/config.fish${NC}"
    else
        echo -e "    ${BOLD}source $SHELL_RC${NC}"
    fi
else
    log_info "Skipping environment setup (--skip-env)"
fi

# =============================================================================
# Configure MCP server for Claude Code
# =============================================================================

if [ "$SKIP_MCP" = false ]; then
    log_section "Configuring MCP server"

    # Project-level .mcp.json (already exists, verify it)
    MCP_JSON="$TML_ROOT/.mcp.json"
    if [ -f "$MCP_JSON" ]; then
        log_ok "Project .mcp.json exists"
    else
        log_info "Creating project .mcp.json..."
        cat > "$MCP_JSON" << MCPEOF
{
  "mcpServers": {
    "tml": {
      "command": "./build/debug/tml_mcp",
      "args": []
    },
    "rulebook": {
      "command": "npx",
      "args": [
        "-y",
        "@hivehub/rulebook@latest",
        "mcp-server"
      ]
    }
  }
}
MCPEOF
        log_ok "Created $MCP_JSON"
    fi

    # Global Claude Code MCP config (~/.claude/settings.json)
    CLAUDE_DIR="$HOME/.claude"
    CLAUDE_MCP="$CLAUDE_DIR/mcp.json"

    if [ -d "$CLAUDE_DIR" ]; then
        log_ok "Claude Code config dir exists ($CLAUDE_DIR)"
    else
        mkdir -p "$CLAUDE_DIR"
        log_ok "Created Claude Code config dir ($CLAUDE_DIR)"
    fi

    # Check if global MCP config has the tml server
    if [ -f "$CLAUDE_MCP" ] && grep -q "tml_mcp" "$CLAUDE_MCP"; then
        log_ok "TML MCP server already in global config"
    else
        log_info "To use TML MCP globally, add this to $CLAUDE_MCP:"
        echo ""
        echo -e "  ${DIM}{${NC}"
        echo -e "  ${DIM}  \"mcpServers\": {${NC}"
        echo -e "  ${DIM}    \"tml\": {${NC}"
        echo -e "  ${DIM}      \"command\": \"$TML_ROOT/build/debug/tml_mcp\",${NC}"
        echo -e "  ${DIM}      \"args\": []${NC}"
        echo -e "  ${DIM}    }${NC}"
        echo -e "  ${DIM}  }${NC}"
        echo -e "  ${DIM}}${NC}"
        echo ""
    fi

    # Verify MCP server binary works
    if [ -f "$TML_ROOT/build/debug/tml_mcp" ]; then
        log_ok "MCP server binary found and ready"
        log_info "The MCP server is used automatically by Claude Code when"
        log_info "running inside the TML project directory."
    else
        log_warn "MCP server binary not built yet."
        log_info "Build the compiler first: ./scripts/build.sh"
    fi
else
    log_info "Skipping MCP setup (--skip-mcp)"
fi

# =============================================================================
# Summary
# =============================================================================

log_section "Setup complete"

echo -e "  ${BOLD}Project root:${NC}  $TML_ROOT"
echo -e "  ${BOLD}Compiler:${NC}      $TML_ROOT/build/debug/tml"
echo -e "  ${BOLD}MCP server:${NC}    $TML_ROOT/build/debug/tml_mcp"
echo -e "  ${BOLD}Shell config:${NC}  $SHELL_RC"
echo ""

echo -e "  ${BOLD}Quick start:${NC}"
if [ "$SKIP_ENV" = false ]; then
    if [ "$USER_SHELL" = "fish" ]; then
        echo -e "    source ~/.config/fish/config.fish"
    else
        echo -e "    source $SHELL_RC"
    fi
fi
echo -e "    tml --version              # verify installation"
echo -e "    tml run hello.tml          # run a TML program"
echo -e "    tml test --no-cache        # run test suite"
echo ""

echo -e "  ${BOLD}Claude Code:${NC}"
echo -e "    cd $TML_ROOT"
echo -e "    claude                     # MCP tools auto-loaded from .mcp.json"
echo ""

echo -e "${GREEN}Done!${NC}"
