@echo off
:: Verify that build/llvm/ is correctly picked up by the TML compiler
:: build — no LLVM_DIR, no F:/LLVM references.
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

echo ================================================================================
echo   Bootstrap verification
echo ================================================================================
echo.

if not exist "build\llvm\lib\cmake\llvm\LLVMConfig.cmake" (
    echo ERROR: build/llvm/ is not built.
    echo Run scripts\build-llvm.bat first.
    exit /b 1
)

echo [1/3] LLVMConfig.cmake found at build/llvm/lib/cmake/llvm/
echo.

echo [2/3] Deleting stale build/cache/ (was configured with LLVM_DIR=F:/LLVM) ...
if exist "build\cache" rmdir /s /q "build\cache"

echo [3/3] Rebuilding TML compiler without LLVM_DIR ...
call scripts\build.bat
if errorlevel 1 (
    echo.
    echo TML compiler build failed.
    exit /b 1
)

echo.
echo ================================================================================
echo   Bootstrap verified
echo ================================================================================
echo.
echo The TML compiler now uses the locally-built build/llvm/.
echo No LLVM_DIR / no F:/LLVM / no external LLVM install required.
echo.

endlocal
