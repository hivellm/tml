@echo off
setlocal enabledelayedexpansion

:: TML Compiler - Format Script for Windows
:: Usage: scripts\format.bat [--check]

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

set "CHECK_MODE="
if /i "%~1"=="--check" set "CHECK_MODE=1"

echo Running clang-format...

:: Check if clang-format exists
where clang-format >nul 2>&1
if errorlevel 1 (
    echo clang-format not found, skipping format
    exit /b 0
)

:: Process source files
set "HAS_ERRORS=0"

if defined CHECK_MODE (
    echo Checking format...
    for /r "packages\compiler\src" %%f in (*.cpp *.c *.h *.hpp) do (
        clang-format --dry-run --Werror "%%f" >nul 2>&1
        if errorlevel 1 (
            echo   Needs formatting: %%f
            set "HAS_ERRORS=1"
        )
    )
    for /r "packages\compiler\runtime" %%f in (*.cpp *.c *.h *.hpp) do (
        clang-format --dry-run --Werror "%%f" >nul 2>&1
        if errorlevel 1 (
            echo   Needs formatting: %%f
            set "HAS_ERRORS=1"
        )
    )
    if "!HAS_ERRORS!"=="1" (
        echo Some files need formatting. Run scripts\format.bat to fix.
        exit /b 1
    )
    echo All files are properly formatted
) else (
    echo Formatting files...
    for /r "packages\compiler\src" %%f in (*.cpp *.c *.h *.hpp) do (
        echo   Formatting: %%f
        clang-format -i "%%f"
    )
    for /r "packages\compiler\runtime" %%f in (*.cpp *.c *.h *.hpp) do (
        echo   Formatting: %%f
        clang-format -i "%%f"
    )
    echo Format complete
)

endlocal
