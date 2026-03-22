@echo off
setlocal enabledelayedexpansion

:: TML Compiler Build Script for Windows
:: Usage: scripts\build.bat [debug|release] [--clean] [--tests] [--pack] [--bump-minor] [--bump-major]

:: Get the root directory (parent of scripts\)
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"
set "ROOT_DIR=%CD%"

:: Detect target triple (Windows MSVC)
set "TARGET=x86_64-pc-windows-msvc"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "TARGET=aarch64-pc-windows-msvc"

:: Default values
set "BUILD_TYPE=debug"
set "CLEAN_BUILD=0"
set "BUILD_TESTS=OFF"
set "ENABLE_ASAN=OFF"
set "ENABLE_UBSAN=OFF"
set "ENABLE_LLVM_BACKEND=ON"
set "ENABLE_CRANELIFT_BACKEND=OFF"
set "BUILD_TARGET="
set "BUMP_MAJOR=0"
set "BUMP_MINOR=0"
set "ENABLE_MODULAR=ON"
set "ENABLE_PROFILE=OFF"
set "ENABLE_PACK=0"
set "USE_ZIG_CC=auto"
set "USE_CLANG=0"

:: Parse arguments
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="debug" set "BUILD_TYPE=debug" & shift & goto :parse_args
if /i "%~1"=="release" set "BUILD_TYPE=release" & shift & goto :parse_args
if /i "%~1"=="--clean" set "CLEAN_BUILD=1" & shift & goto :parse_args
if /i "%~1"=="--no-tests" set "BUILD_TESTS=OFF" & shift & goto :parse_args
if /i "%~1"=="--tests" set "BUILD_TESTS=ON" & shift & goto :parse_args
if /i "%~1"=="--asan" set "ENABLE_ASAN=ON" & shift & goto :parse_args
if /i "%~1"=="--ubsan" set "ENABLE_UBSAN=ON" & shift & goto :parse_args
if /i "%~1"=="--sanitize" set "ENABLE_ASAN=ON" & set "ENABLE_UBSAN=ON" & shift & goto :parse_args
if /i "%~1"=="--no-llvm" set "ENABLE_LLVM_BACKEND=OFF" & shift & goto :parse_args
if /i "%~1"=="--cranelift" set "ENABLE_CRANELIFT_BACKEND=ON" & shift & goto :parse_args
if /i "%~1"=="--monolithic" set "ENABLE_MODULAR=OFF" & shift & goto :parse_args
if /i "%~1"=="--profile" set "ENABLE_PROFILE=ON" & shift & goto :parse_args
if /i "%~1"=="--pack" set "ENABLE_PACK=1" & shift & goto :parse_args
if /i "%~1"=="--zig" set "USE_ZIG_CC=1" & shift & goto :parse_args
if /i "%~1"=="--msvc" set "USE_ZIG_CC=0" & shift & goto :parse_args
if /i "%~1"=="--clang" set "USE_ZIG_CC=0" & set "USE_CLANG=1" & shift & goto :parse_args
if /i "%~1"=="--target" set "BUILD_TARGET=%~2" & shift & shift & goto :parse_args
if /i "%~1"=="--bump-major" set "BUMP_MAJOR=1" & shift & goto :parse_args
if /i "%~1"=="--bump-minor" set "BUMP_MINOR=1" & shift & goto :parse_args
if /i "%~1"=="--help" goto :show_help
if /i "%~1"=="-h" goto :show_help
echo Unknown argument: %~1
exit /b 1

:show_help
echo TML Compiler Build Script
echo.
echo Usage: scripts\build.bat [debug^|release] [options]
echo.
echo Build types:
echo   debug     Build with debug symbols (default)
echo   release   Build with optimizations
echo.
echo Options:
echo   --clean        Clean build directory before building
echo   --tests        Build C++ unit tests (tml_tests.exe)
echo   --no-tests     Don't build C++ tests (default)
echo   --bump-major   Increment major version (resets minor to 0)
echo   --bump-minor   Increment minor version
echo   --asan         Enable AddressSanitizer (memory error detection)
echo   --ubsan        Enable UndefinedBehaviorSanitizer
echo   --sanitize     Enable both ASan and UBSan
echo   --cranelift    Enable Cranelift backend (fast debug builds)
echo   --monolithic   Build single executable (default is modular with DLLs)
echo   --pack         Pack plugins (compress DLLs + manifest); default modular build
echo   --zig          Use Zig CC (default when available, fastest builds)
echo   --msvc         Force MSVC compiler (cl.exe via Ninja or MSBuild)
echo   --clang        Use system Clang (clang/clang++ via Ninja)
echo   --target X     Build only target X (e.g., tml, tml_mcp, tml_tests)
echo   --help         Show this help message
echo.
echo Version:
echo   Use --bump-major or --bump-minor to increment version.
echo   Or edit the VERSION file directly (MAJOR.MINOR.BUILD).
echo.
echo Host target: %TARGET%
echo.
echo Output structure (modular by default):
echo   build\debug\bin\       - tml.exe ^(small launcher^) + DLLs + dependencies
echo   build\debug\lib\       - .lib static libraries
echo   build\release\bin\     - Release executables + DLLs
echo   build\release\lib\     - Release libraries
exit /b 0

:args_done

:: ========================================
:: Version Management
:: ========================================
:: Read VERSION file (format: MAJOR.MINOR.BUILD)
set "VERSION_FILE=%ROOT_DIR%\VERSION"
if not exist "%VERSION_FILE%" (
    echo WARNING: VERSION file not found, creating with 0.1.0
    echo 0.1.0> "%VERSION_FILE%"
)

:: Read the version string
set /p "VERSION_STR=" < "%VERSION_FILE%"

:: Parse MAJOR.MINOR.BUILD using for /f with delims=.
for /f "tokens=1,2,3 delims=." %%a in ("!VERSION_STR!") do (
    set "VER_MAJOR=%%a"
    set "VER_MINOR=%%b"
    set "VER_BUILD=%%c"
)

:: Handle missing build number (e.g., if VERSION file only has "0.1")
if "!VER_BUILD!"=="" set "VER_BUILD=0"

:: Apply manual bumps
if "%BUMP_MAJOR%"=="1" (
    set /a "VER_MAJOR=!VER_MAJOR! + 1"
    set "VER_MINOR=0"
)
if "%BUMP_MINOR%"=="1" (
    set /a "VER_MINOR=!VER_MINOR! + 1"
)

:: Only increment build number on manual bumps (--bump-major or --bump-minor)
:: Auto-increment was removed because it changed version_generated.hpp every build,
:: which cascaded a full rebuild of ~320 files through common.hpp includes.
if "%BUMP_MAJOR%"=="1" set /a "VER_BUILD=!VER_BUILD! + 1"
if "%BUMP_MINOR%"=="1" set /a "VER_BUILD=!VER_BUILD! + 1"

:: Write updated version back to VERSION file only if bumped
if "%BUMP_MAJOR%"=="1" echo !VER_MAJOR!.!VER_MINOR!.!VER_BUILD!> "%VERSION_FILE%"
if "%BUMP_MINOR%"=="1" echo !VER_MAJOR!.!VER_MINOR!.!VER_BUILD!> "%VERSION_FILE%"

:: Set directories
:: Cache: build/cache/<target>/<config>/ - CMake files, object files
:: Output: build/<config>/ - final executables only
set "CACHE_DIR=%ROOT_DIR%\build\cache\%TARGET%\%BUILD_TYPE%"
set "OUTPUT_DIR=%ROOT_DIR%\build\%BUILD_TYPE%"

echo.
echo ========================================
echo        TML Compiler Build System
echo ========================================
echo.
echo Version:     !VER_MAJOR!.!VER_MINOR!.!VER_BUILD!
echo Target:      %TARGET%
echo Build type:  %BUILD_TYPE%
echo Cache dir:   %CACHE_DIR%
echo Output dir:  %OUTPUT_DIR%
echo Tests:       %BUILD_TESTS%
if "%USE_ZIG_CC%"=="1" echo Compiler:    Zig CC ^(Clang 20^)
if "%USE_CLANG%"=="1" echo Compiler:    Clang
if "%ENABLE_MODULAR%"=="ON" echo Modular:     Enabled
if "%ENABLE_PACK%"=="1" echo Pack:        Enabled
if "%ENABLE_CRANELIFT_BACKEND%"=="ON" echo Cranelift:   Enabled
if "%ENABLE_ASAN%"=="ON" echo ASan:        Enabled
if "%ENABLE_UBSAN%"=="ON" echo UBSan:       Enabled
echo.

:: Clean if requested
if "%CLEAN_BUILD%"=="1" (
    echo Cleaning cache directory...
    if exist "%CACHE_DIR%" rmdir /s /q "%CACHE_DIR%"
)

:: Create directories
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

:: Kill any running tml.exe to prevent link errors (LNK1168)
taskkill /F /IM tml.exe >nul 2>&1
:: Only kill tml_mcp.exe if we're actually building it (target is empty=all, or tml_mcp)
if "%BUILD_TARGET%"=="" taskkill /F /IM tml_mcp.exe >nul 2>&1
if "%BUILD_TARGET%"=="tml_mcp" taskkill /F /IM tml_mcp.exe >nul 2>&1

:: Build Cranelift bridge if enabled
if "%ENABLE_CRANELIFT_BACKEND%"=="ON" (
    set "CRANELIFT_LIB=%ROOT_DIR%\build\cranelift\release\tml_cranelift_bridge.lib"
    if not exist "!CRANELIFT_LIB!" (
        echo Building Cranelift bridge...
        cd /d "%ROOT_DIR%\compiler\cranelift"
        cargo build --release
        if errorlevel 1 (
            echo Cranelift bridge build failed!
            exit /b 1
        )
        cd /d "%ROOT_DIR%"
    ) else (
        echo Cranelift bridge: Using cached library
    )
)

:: Set CMake build type
set "CMAKE_BUILD_TYPE=Debug"
if /i "%BUILD_TYPE%"=="release" set "CMAKE_BUILD_TYPE=Release"

:: Detect build toolchain
set "CMAKE_GENERATOR_ARGS="
set "USE_NINJA=0"
set "TOOLCHAIN_ARGS="

:: Auto-detect: prefer Zig CC when available (fastest builds)
if "%USE_ZIG_CC%"=="auto" (
    where zig.exe >nul 2>&1
    if !errorlevel!==0 (
        where ninja.exe >nul 2>&1
        if !errorlevel!==0 (
            set "USE_ZIG_CC=1"
        ) else (
            set "USE_ZIG_CC=0"
        )
    ) else (
        set "USE_ZIG_CC=0"
    )
)

if "%USE_ZIG_CC%"=="1" (
    :: Zig CC mode: use Ninja + zig cc/c++ via toolchain file
    where ninja.exe >nul 2>&1
    if !errorlevel!==0 (
        set "USE_NINJA=1"
    ) else (
        echo ERROR: Ninja is required for Zig CC builds. Install ninja-build.
        exit /b 1
    )
    where zig.exe >nul 2>&1
    if !errorlevel!==0 (
        echo Compiler:    Zig CC ^(Clang 20^)
        set "TOOLCHAIN_ARGS=-DCMAKE_TOOLCHAIN_FILE=%ROOT_DIR%\cmake\toolchains\zig.cmake"
    ) else (
        echo ERROR: zig.exe not found in PATH. Install: winget install zig.zig
        exit /b 1
    )
    echo Generator:   Ninja ^(zig cc^)
    set "CMAKE_GENERATOR_ARGS=-G Ninja"
) else if "%USE_CLANG%"=="1" (
    :: System Clang mode: use Ninja + clang/clang++
    where ninja.exe >nul 2>&1
    if !errorlevel!==0 (
        set "USE_NINJA=1"
    ) else (
        echo ERROR: Ninja is required for Clang builds. Install ninja-build.
        exit /b 1
    )
    where clang.exe >nul 2>&1
    if !errorlevel!==0 (
        echo Compiler:    Clang
    ) else (
        echo ERROR: clang.exe not found in PATH.
        exit /b 1
    )
    echo Generator:   Ninja ^(clang^)
    set "CMAKE_GENERATOR_ARGS=-G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
) else (
    :: MSVC mode: detect Ninja + cl.exe
    where ninja.exe >nul 2>&1
    if !errorlevel!==0 (
        where cl.exe >nul 2>&1
        if !errorlevel!==0 (
            set "USE_NINJA=1"
        ) else (
            :: Try to initialize MSVC environment automatically
            for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
                if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
                    call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
                    where cl.exe >nul 2>&1
                    if !errorlevel!==0 set "USE_NINJA=1"
                )
            )
        )
    )
    if "!USE_NINJA!"=="1" (
        echo Compiler:    MSVC ^(cl.exe^)
        echo Generator:   Ninja
        set "CMAKE_GENERATOR_ARGS=-G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl"
    ) else (
        echo Compiler:    MSVC ^(cl.exe^)
        echo Generator:   MSBuild
    )
)

:: Configure CMake
echo Configuring CMake...
cd /d "%CACHE_DIR%"

cmake "%ROOT_DIR%\compiler" ^
    %CMAKE_GENERATOR_ARGS% ^
    %TOOLCHAIN_ARGS% ^
    -DTML_BUILD_TOKEN=tml_script_build_2026 ^
    -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
    -DTML_BUILD_TESTS=%BUILD_TESTS% ^
    -DTML_ENABLE_ASAN=%ENABLE_ASAN% ^
    -DTML_ENABLE_UBSAN=%ENABLE_UBSAN% ^
    -DTML_USE_LLVM_BACKEND=%ENABLE_LLVM_BACKEND% ^
    -DTML_USE_CRANELIFT_BACKEND=%ENABLE_CRANELIFT_BACKEND% ^
    -DTML_BUILD_MODULAR=%ENABLE_MODULAR% ^
    -DTML_PROFILE=%ENABLE_PROFILE% ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DTML_OUTPUT_DIR="%OUTPUT_DIR%" ^
    -DTML_VERSION_MAJOR=!VER_MAJOR! ^
    -DTML_VERSION_MINOR=!VER_MINOR! ^
    -DTML_VERSION_BUILD=!VER_BUILD!

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

:: Build
echo.
echo Building TML compiler...

if "%BUILD_TARGET%"=="" (
    cmake --build . --config %CMAKE_BUILD_TYPE% --parallel
) else (
    echo Building target: %BUILD_TARGET%
    cmake --build . --config %CMAKE_BUILD_TYPE% --target %BUILD_TARGET% --parallel
)

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

:: Copy compile_commands.json to root for IDE support
if exist "compile_commands.json" (
    copy /y "compile_commands.json" "%ROOT_DIR%\" >nul 2>&1
)

:: Plugins now build directly to bin/plugins/ via CMake (TML_BIN_DIR/plugins)

:: Pack plugins if requested
if "%ENABLE_PACK%"=="1" (
    if not "%ENABLE_MODULAR%"=="ON" (
        echo WARNING: --pack requires modular build, but --monolithic is enabled, skipping plugin packing.
    ) else (
        echo.
        echo Packing plugins...
        cd /d "%ROOT_DIR%"
        set "PLUGINS_DIR=%OUTPUT_DIR%\plugins"
        if exist "!PLUGINS_DIR!" (
            "%OUTPUT_DIR%\tml.exe" run tools\plugin_pack.tml -- "!PLUGINS_DIR!"
            if errorlevel 1 (
                echo WARNING: Plugin packing failed ^(non-fatal^).
            ) else (
                echo.
                echo Verifying plugins...
                "%OUTPUT_DIR%\tml.exe" run tools\plugin_verify.tml -- "!PLUGINS_DIR!"
                if errorlevel 1 (
                    echo WARNING: Plugin verification failed!
                )
            )
        ) else (
            echo WARNING: No plugins directory found at !PLUGINS_DIR!, skipping pack.
        )
    )
)

:: Print result
echo.
echo ========================================
echo          Build Complete!
echo ========================================
echo.
echo Version:     !VER_MAJOR!.!VER_MINOR!.!VER_BUILD!
echo Compiler:    %OUTPUT_DIR%\tml.exe
if "%BUILD_TESTS%"=="ON" (
    echo Tests:       %OUTPUT_DIR%\tml_tests.exe
)
echo.

endlocal
