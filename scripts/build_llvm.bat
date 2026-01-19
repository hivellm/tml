@echo off
setlocal EnableDelayedExpansion

echo ========================================
echo        LLVM Build Script
echo ========================================
echo.

set LLVM_SRC=%~dp0..\src\llvm-project
set LLVM_BUILD=%~dp0..\build\llvm
set LLVM_INSTALL=%~dp0..\src\llvm-install

if not exist "%LLVM_SRC%\llvm\CMakeLists.txt" (
    echo ERROR: LLVM source not found at %LLVM_SRC%
    echo Please clone LLVM first:
    echo   git clone https://github.com/llvm/llvm-project.git src/llvm-project
    exit /b 1
)

echo LLVM Source:  %LLVM_SRC%
echo LLVM Build:   %LLVM_BUILD%
echo LLVM Install: %LLVM_INSTALL%
echo.

:: Create build directory
if not exist "%LLVM_BUILD%" mkdir "%LLVM_BUILD%"

cd /d "%LLVM_BUILD%"

echo Configuring LLVM (this may take a while)...
echo.

:: Configure LLVM with minimal components needed for TML
:: We only need: LLVM core, lld, and the C API
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%LLVM_INSTALL%" ^
    -DLLVM_ENABLE_PROJECTS="lld" ^
    -DLLVM_TARGETS_TO_BUILD="X86;AArch64" ^
    -DLLVM_BUILD_TOOLS=OFF ^
    -DLLVM_BUILD_EXAMPLES=OFF ^
    -DLLVM_BUILD_TESTS=OFF ^
    -DLLVM_BUILD_DOCS=OFF ^
    -DLLVM_INCLUDE_EXAMPLES=OFF ^
    -DLLVM_INCLUDE_TESTS=OFF ^
    -DLLVM_INCLUDE_DOCS=OFF ^
    -DLLVM_ENABLE_BINDINGS=OFF ^
    -DLLVM_ENABLE_ASSERTIONS=OFF ^
    -DLLVM_ENABLE_RTTI=OFF ^
    -DLLVM_ENABLE_ZLIB=OFF ^
    -DLLVM_ENABLE_ZSTD=OFF ^
    -DLLVM_ENABLE_LIBXML2=OFF ^
    -DLLVM_ENABLE_TERMINFO=OFF ^
    "%LLVM_SRC%\llvm"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo.
echo Building LLVM (this will take 30-60 minutes)...
echo.

:: Build only the necessary targets
cmake --build . --config Release --target LLVM-C lld -- /m

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo Installing LLVM...
echo.

cmake --install . --config Release

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Install failed
    exit /b 1
)

echo.
echo ========================================
echo LLVM build complete!
echo.
echo Libraries installed to: %LLVM_INSTALL%
echo.
echo Now rebuild TML with:
echo   scripts\build.bat --clean
echo ========================================

endlocal
