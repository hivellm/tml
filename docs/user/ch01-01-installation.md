# Installation

The first step is to install TML. This section covers the installation
process for different operating systems.

## Prerequisites

Before installing TML, you need:

- **LLVM/Clang**: Version 15 or later
- **CMake**: Version 3.16 or later
- **C++ Compiler**: GCC 11+ or MSVC 2022

### Windows

1. Install LLVM from [releases.llvm.org](https://releases.llvm.org/)
2. Install Visual Studio 2022 with C++ workload
3. Install CMake from [cmake.org](https://cmake.org/)

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install llvm clang cmake build-essential
```

### macOS

```bash
brew install llvm cmake
```

## Building TML

Clone the repository and build:

```bash
git clone https://github.com/hivellm/tml.git
cd tml/packages/compiler
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Verifying Installation

After building, verify the installation:

```bash
./tml --version
```

You should see the TML version number printed.

## Updating TML

To update TML, pull the latest changes and rebuild:

```bash
git pull
cd build
cmake --build . --config Release
```
