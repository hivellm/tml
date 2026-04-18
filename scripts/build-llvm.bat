@echo off
setlocal enabledelayedexpansion

:: Build the vendored LLVM submodule (src/llvm-project) into build/llvm/.
:: This is a ONE-TIME build (~30-90 minutes). Subsequent TML compiler
:: builds pick up build/llvm/ automatically via CMake's find_package.
::
:: Usage:
::   scripts\build-llvm.bat           Release build (default)
::   scripts\build-llvm.bat debug     Debug build
::   scripts\build-llvm.bat --clean   Delete build/llvm/ and rebuild

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"
set "ROOT_DIR=%CD%"

set "LLVM_SRC=%ROOT_DIR%\src\llvm-project\llvm"
set "LLVM_BUILD=%ROOT_DIR%\build\llvm"

:: ---- arg parsing ----
set "BUILD_TYPE=Release"
set "CLEAN=0"
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="debug" (set "BUILD_TYPE=Debug" & shift & goto :parse_args)
if /i "%~1"=="release" (set "BUILD_TYPE=Release" & shift & goto :parse_args)
if /i "%~1"=="--clean" (set "CLEAN=1" & shift & goto :parse_args)
if /i "%~1"=="--help" goto :show_help
if /i "%~1"=="-h" goto :show_help
echo Unknown argument: %~1
exit /b 1

:show_help
echo Build the vendored LLVM submodule into build/llvm/.
echo.
echo Usage: scripts\build-llvm.bat [release^|debug] [--clean]
exit /b 0

:args_done

if not exist "%LLVM_SRC%\CMakeLists.txt" (
    echo ERROR: src/llvm-project/llvm/CMakeLists.txt not found.
    echo The LLVM submodule is missing. Initialize it with:
    echo   git submodule update --init --recursive src/llvm-project
    exit /b 1
)

if "%CLEAN%"=="1" (
    if exist "%LLVM_BUILD%" (
        echo Removing existing build/llvm/ ...
        rmdir /s /q "%LLVM_BUILD%"
    )
)

if not exist "%LLVM_BUILD%" mkdir "%LLVM_BUILD%"

:: ---- toolchain detection ----
:: LLVM's build has deep POSIX-API references (lseek/read/write/close in
:: libSupport) that zig cc's MSVC target doesn't link cleanly. We use
:: MSVC natively for LLVM — the TML compiler itself still defaults to
:: Zig CC via scripts/build.bat, linking against the /MT LLVM libs.
echo Toolchain: MSVC ^(cl.exe^) + Ninja + static CRT ^(/MT^)
where cl.exe >nul 2>&1
if not !errorlevel!==0 (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    )
)
where cl.exe >nul 2>&1
if not !errorlevel!==0 (
    echo ERROR: cl.exe not found. Install Visual Studio 2022 with
    echo "Desktop development with C++" workload, or run this script
    echo from a "x64 Native Tools Command Prompt for VS 2022".
    exit /b 1
)
set "GENERATOR=-G Ninja"
set "TOOLCHAIN_ARGS=-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"

echo.
echo ================================================================================
echo   Building vendored LLVM   (this takes 30-90 minutes on first run)
echo ================================================================================
echo   Source:   %LLVM_SRC%
echo   Build:    %LLVM_BUILD%
echo   Type:     %BUILD_TYPE%
echo ================================================================================
echo.

:: ---- CMake configure ----
cd /d "%LLVM_BUILD%"

if not exist "CMakeCache.txt" (
    echo Configuring LLVM ^(first run only^) ...
    cmake "%LLVM_SRC%" ^
        %GENERATOR% ^
        %TOOLCHAIN_ARGS% ^
        -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
        -DLLVM_TARGETS_TO_BUILD=X86 ^
        -DLLVM_ENABLE_PROJECTS=lld ^
        -DLLVM_ENABLE_RUNTIMES= ^
        -DLLVM_ENABLE_ASSERTIONS=OFF ^
        -DLLVM_INCLUDE_TESTS=OFF ^
        -DLLVM_INCLUDE_EXAMPLES=OFF ^
        -DLLVM_INCLUDE_BENCHMARKS=OFF ^
        -DLLVM_INCLUDE_DOCS=OFF ^
        -DLLVM_BUILD_TOOLS=OFF ^
        -DLLVM_BUILD_LLVM_DYLIB=OFF ^
        -DLLVM_INCLUDE_UTILS=OFF ^
        -DLLVM_ENABLE_TERMINFO=OFF ^
        -DLLVM_ENABLE_LIBXML2=OFF ^
        -DLLVM_ENABLE_ZLIB=OFF ^
        -DLLVM_ENABLE_ZSTD=OFF ^
        -DLLVM_ENABLE_DIA_SDK=OFF ^
        -DCMAKE_CXX_STANDARD=17 ^
        -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF
    if errorlevel 1 (
        echo LLVM CMake configuration failed!
        exit /b 1
    )
) else (
    echo Re-using existing CMake cache at %LLVM_BUILD%\CMakeCache.txt
)

:: ---- Build ----
echo.
echo Compiling LLVM ^(incremental; Ctrl+C safe to resume^) ...
cmake --build . --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo LLVM build failed.
    exit /b 1
)

echo.
echo ================================================================================
echo   LLVM build complete
echo ================================================================================
echo   Libraries at: %LLVM_BUILD%\%BUILD_TYPE%\lib
echo   Headers at:   %LLVM_BUILD%\include
echo.
echo Next step: run `scripts\build.bat` to build the TML compiler.
echo ================================================================================

endlocal
