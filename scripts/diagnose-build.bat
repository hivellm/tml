@echo off
setlocal enabledelayedexpansion

::
:: TML Build Performance Diagnostics Script
:: Analyzes build slowness and suggests optimizations
::

echo.
echo ================================================================================
echo               TML Build Performance Diagnostic Report
echo ================================================================================
echo.

set "ROOT_DIR=%~dp0.."
cd /d "%ROOT_DIR%"

echo [1/5] Checking directory sizes...
echo.

for /f "tokens=2" %%A in ('dir /s /b "src\llvm-project" 2^>nul ^| find /c /v ""') do (
    echo   src/llvm-project: %%A files
)
for /f "tokens=2" %%A in ('dir /s /b "src\gcc" 2^>nul ^| find /c /v ""') do (
    echo   src/gcc: %%A files
)
for /f "tokens=2" %%A in ('dir /s /b "compiler" 2^>nul ^| find /c /v ""') do (
    echo   compiler: %%A files
)
for /f "tokens=2" %%A in ('dir /s /b "build" 2^>nul ^| find /c /v ""') do (
    echo   build: %%A files (LARGE! Consider cleaning)
)

echo.
echo [2/5] Checking CMakeLists.txt complexity...
echo.

for /f %%A in ('find compiler -name "CMakeLists.txt" -type f ^| find /c /v ""') do (
    echo   Total CMakeLists.txt files: %%A
)

for /f %%A in ('find compiler/CMakeLists.txt -type f ^| wc -l') do (
    echo   Main compiler/CMakeLists.txt: %%A lines (should be <1000)
)

echo.
echo [3/5] Checking LLVM/GCC references in build...
echo.

setlocal enabledelayedexpansion
set "found=0"
for /r "compiler" %%F in (CMakeLists.txt) do (
    for /f "delims=" %%L in ('findstr /i "llvm-project\|src/gcc" "%%F" 2^>nul') do (
        echo   FOUND: %%F
        set "found=1"
    )
)
if !found!=0 (
    echo   No LLVM/GCC references found in CMakeLists.txt (GOOD!)
) else (
    echo   Check above for unwanted references
)

echo.
echo [4/5] Checking for unused caches...
echo.

if exist ".test-cache.json" (
    for /f %%A in ('.test-cache.json') do (
        echo   Found .test-cache.json - can be cleaned with: del .test-cache.json
    )
)

if exist "build\cache" (
    echo   Found build/cache - large build cache detected
    echo   Clean with: rm -rf build/cache/ (on next build, cache will regenerate)
)

if exist "build\debug\.run-cache" (
    echo   Found .run-cache directory - can be cleaned
)

echo.
echo [5/5] Build optimization recommendations...
echo.

echo QUICK WINS (Do these first):
echo   1. Clean everything:
echo      rm -rf build/
echo      del .test-cache.json
echo.
echo   2. Measure baseline build time:
echo      powershell -Command "Measure-Command { cmd /c 'scripts\build.bat --clean' }" 2>&1
echo.
echo   3. Try faster backends:
echo      scripts\build.bat release       (Release builds are 3-5x faster)
echo      scripts\build.bat --modular     (Parallel DLL compilation)
echo      scripts\build.bat --cranelift   (Fast Cranelift backend)
echo.

echo.
echo MEDIUM EFFORT (Better for long-term):
echo   1. Optimize CMakeLists.txt:
echo      - Disable unused LLVM targets (only enable X86)
echo      - Disable LTO for debug builds
echo      - Enable precompiled headers
echo.
echo   2. Check if src/llvm-project and src/gcc are needed:
echo      grep -r "src/llvm-project\|src/gcc" compiler/
echo      If not found, consider removing (saves 6.6GB)
echo.

echo.
echo AGGRESSIVE (When everything else fails):
echo   1. Remove unused source directories:
echo      git rm -r --cached src/llvm-project src/gcc
echo      rm -rf src/llvm-project src/gcc
echo      git commit -m "Remove unused LLVM/GCC sources (6.6GB)"
echo.
echo   2. Nuclear clean:
echo      rm -rf build/
echo      del .test-cache.json
echo      Then full rebuild
echo.

echo.
echo ================================================================================
echo Full optimization guide: .sandbox/BUILD_OPTIMIZATION_GUIDE.md
echo ================================================================================
echo.

endlocal