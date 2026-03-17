# Installation

TML is distributed as a self-contained binary. It includes an embedded LLVM backend and LLD
linker, so you do not need to install any external compiler toolchain to compile and link TML
programs.

## Using Pre-built Binaries (Recommended)

Pre-built binaries are available for Windows (x64), Linux (x64), and macOS (x64 and ARM64).

### Windows

1. Download `tml-windows-x64.zip` from the [releases page](https://github.com/hivellm/tml/releases/latest).
2. Extract the archive to a permanent location, such as `C:\tml`.
3. Add that folder to your `PATH` environment variable.
4. Open a new terminal and verify the installation:

```bash
tml --version
```

### Linux

```bash
wget https://github.com/hivellm/tml/releases/latest/download/tml-linux-x64.tar.gz
tar -xzf tml-linux-x64.tar.gz
sudo mv tml /usr/local/bin/
tml --version
```

### macOS

```bash
curl -LO https://github.com/hivellm/tml/releases/latest/download/tml-macos-arm64.tar.gz
tar -xzf tml-macos-arm64.tar.gz
sudo mv tml /usr/local/bin/
tml --version
```

Use `tml-macos-x64.tar.gz` on Intel Macs.

## Verifying the Installation

After installation, run:

```bash
tml --version
```

You should see the TML version number. Next, create a quick test to confirm compilation works:

```bash
echo 'func main() { println("Hello from TML!") }' > test.tml
tml run test.tml
```

Expected output:

```
Hello from TML!
```

If you see that output, your installation is complete. You can delete `test.tml`.

## Building from Source

Building from source is only necessary if you want to develop the compiler itself or need a
build that is not available as a pre-built binary.

### Prerequisites

- **Git** — for cloning the repository
- **CMake** 3.16 or later
- **C++ compiler** — GCC 11+, Clang 15+, or MSVC 2022

### Clone and Build

**Windows:**

```bash
git clone https://github.com/hivellm/tml.git
cd tml
scripts\build.bat
```

**Linux and macOS:**

```bash
git clone https://github.com/hivellm/tml.git
cd tml
scripts/build.sh
```

The default build produces a debug binary at `build/debug/bin/tml`. For an optimized release
binary:

```bash
scripts\build.bat release   # Windows
scripts/build.sh release    # Linux/macOS
```

The release binary is located at `build/release/bin/tml`.

### What the Build Includes

The default build embeds LLVM (~55 static libraries) and LLD directly into the compiler binary.
This means the resulting `tml` executable:

- Compiles TML source to LLVM IR in-process
- Converts IR to native object files in-process (no external `clang` needed)
- Links object files in-process (no external system linker needed)

The compiler is fully self-contained. The only external dependency at runtime is the operating
system.

## Updating TML

### Pre-built Binaries

Download the latest release from the releases page and replace your existing `tml` binary.

### Built from Source

```bash
cd tml
git pull
scripts\build.bat release   # Windows
scripts/build.sh release    # Linux/macOS
```

## Troubleshooting

**`tml: command not found`** — The binary is not on your `PATH`. Confirm the directory
containing the `tml` executable is listed in `PATH` and that you have opened a new terminal
since modifying `PATH`.

**Permission denied (Linux/macOS)** — The binary may not be executable. Run:

```bash
chmod +x /usr/local/bin/tml
```

**Build fails with missing dependencies** — The most common cause is an outdated C++ compiler.
Confirm your compiler version meets the prerequisites listed above.
