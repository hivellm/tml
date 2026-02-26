@echo off
setlocal enabledelayedexpansion

::
:: TML Build Cache Cleanup Script
:: Removes accumulated build artifacts that slow down builds
::
:: This script safely removes:
::   - CMake cache (will regenerate on next build)
::   - Object files from previous builds
::   - LLVM artifacts from different build attempts
::   - Cranelift artifacts
::   - Coverage data
::   - Test logs
::
:: WARNING: This will force a full rebuild on the next build (5-10 minutes)
::

set "ROOT_DIR=%~dp0.."
cd /d "%ROOT_DIR%"

echo.
echo ================================================================================
echo                 TML Build Cache Cleanup Script
echo ================================================================================
echo.
echo This script removes accumulated build artifacts.
echo.
echo WARNING: Next build will be a FULL rebuild (5-10 minutes)
echo.
echo Directories to be removed:
echo   - build/cache/                 (6.1GB CMake cache + object files)
echo   - build/llvm/                  (1.1GB LLVM artifacts)
echo   - build/cranelift/             (229MB Cranelift artifacts)
echo   - build/coverage/              (2.4MB coverage data)
echo   - .test-cache.json             (test metadata)
echo.
echo Total disk space to be freed: ~7.5GB
echo.
echo Press ENTER to continue, or Ctrl+C to cancel...
pause

echo.
echo Removing build cache...

if exist "build\cache" (
    echo   Removing build/cache/...
    rmdir /s /q "build\cache"
    if errorlevel 0 echo     OK: build/cache removed
)

if exist "build\llvm" (
    echo   Removing build/llvm/...
    rmdir /s /q "build\llvm"
    if errorlevel 0 echo     OK: build/llvm removed
)

if exist "build\cranelift" (
    echo   Removing build/cranelift/...
    rmdir /s /q "build\cranelift"
    if errorlevel 0 echo     OK: build/cranelift removed
)

if exist "build\coverage" (
    echo   Removing build/coverage/...
    rmdir /s /q "build\coverage"
    if errorlevel 0 echo     OK: build/coverage removed
)

if exist ".test-cache.json" (
    echo   Removing .test-cache.json...
    del /q ".test-cache.json"
    if errorlevel 0 echo     OK: .test-cache.json removed
)

if exist ".run-cache.json" (
    echo   Removing .run-cache.json...
    del /q ".run-cache.json"
    if errorlevel 0 echo     OK: .run-cache.json removed
)

echo.
echo ================================================================================
echo                           Cleanup Complete!
echo ================================================================================
echo.
echo Next steps:
echo.
echo 1. Clean build:
echo    scripts\build.bat --clean
echo.
echo 2. Measure build time:
echo    powershell -Command "Measure-Command { cmd /c 'scripts\build.bat --clean' }"
echo.
echo 3. For faster future builds, try:
echo    scripts\build.bat release          (3-5x faster, Release build)
echo    scripts\build.bat --modular        (Parallel DLL compilation)
echo    scripts\build.bat --cranelift      (Ultra-fast experimental backend)
echo.
echo Expected build times after cleanup:
echo    First build (full):     5-10 minutes
echo    Incremental builds:     1-2 minutes
echo    Release builds:         2-3 minutes
echo.
echo For full optimization tips, see: .sandbox/BUILD_ANALYSIS_REPORT.md
echo.

endlocal