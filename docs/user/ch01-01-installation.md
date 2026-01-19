# Installation

The first step is to install TML. This section covers the installation
process for different operating systems.

## Using Pre-built Binaries (Recommended)

TML is a **self-contained compiler** - you don't need to install LLVM, clang,
or any other external tools to use it. Just download the binary and you're
ready to go.

### Windows

1. Download `tml-windows-x64.zip` from the releases page
2. Extract to a folder (e.g., `C:\tml`)
3. Add the folder to your PATH
4. Run `tml --version` to verify

### Linux

```bash
# Download and extract
wget https://github.com/hivellm/tml/releases/latest/download/tml-linux-x64.tar.gz
tar -xzf tml-linux-x64.tar.gz
sudo mv tml /usr/local/bin/

# Verify
tml --version
```

### macOS

```bash
# Download and extract
curl -LO https://github.com/hivellm/tml/releases/latest/download/tml-macos-x64.tar.gz
tar -xzf tml-macos-x64.tar.gz
sudo mv tml /usr/local/bin/

# Verify
tml --version
```

## Building from Source

If you want to build TML from source (for development or to get the latest
features), you'll need:

### Prerequisites for Building

- **CMake**: Version 3.16 or later
- **C++ Compiler**: GCC 11+, Clang 15+, or MSVC 2022
- **Git**: For cloning the repository

#### Windows Build

```bash
# Clone the repository
git clone https://github.com/hivellm/tml.git
cd tml

# Build (debug by default)
scripts\build.bat

# Or build release version
scripts\build.bat release
```

#### Linux/macOS Build

```bash
# Clone the repository
git clone https://github.com/hivellm/tml.git
cd tml

# Build
scripts/build.sh

# Or build release version
scripts/build.sh release
```

### Building with LLVM Backend (Optional)

For self-contained compilation without external tools, build with LLVM support:

```bash
# Build with LLVM backend
cmake -B build -DTML_USE_LLVM_BACKEND=ON
cmake --build build --config Release
```

This embeds the LLVM backend directly into the TML compiler, allowing it to
compile code without needing clang or any external tools.

## Verifying Installation

After installation, verify everything works:

```bash
tml --version
```

You should see the TML version number printed.

Create a simple test file:

```bash
echo 'func main() { println("Hello from TML!"); }' > hello.tml
tml run hello.tml
```

## Self-Contained Mode

The TML compiler is designed to be self-contained. When built with the LLVM
backend, it includes:

- **Built-in LLVM IR compiler**: No need for clang to compile IR to object files
- **Built-in LLD linker**: No need for system linkers
- **Pre-compiled runtime**: No need to compile C runtime files

If you encounter issues, you can use the `--use-external-tools` flag to fall
back to external tools for debugging:

```bash
tml build myfile.tml --use-external-tools
```

## Updating TML

To update TML:

### Using Pre-built Binaries

Download the latest release and replace your existing `tml` binary.

### Building from Source

```bash
git pull
scripts/build.bat  # Windows
scripts/build.sh   # Linux/macOS
```
