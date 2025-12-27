@echo off
setlocal enabledelayedexpansion

:: TML Compiler - Lint Script for Windows
:: Usage: scripts\lint.bat [--fix]
::
:: Runs:
::   1. tml lint - for TML files (if compiler available)
::   2. clang-tidy - for C/C++ files (if available)

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

set "FIX_MODE="
if /i "%~1"=="--fix" set "FIX_MODE=--fix"

:: ============================================
:: TML Files Lint (using tml lint command)
:: ============================================
echo Linting TML files...

set "TML_EXE="
if exist "build\debug\tml.exe" set "TML_EXE=build\debug\tml.exe"
if exist "build\release\tml.exe" if not defined TML_EXE set "TML_EXE=build\release\tml.exe"

if defined TML_EXE (
    "%TML_EXE%" lint %FIX_MODE% packages examples
    if errorlevel 1 exit /b 1
) else (
    echo   TML compiler not found. Build first: scripts\build.bat
)

:: ============================================
:: C/C++ Files Lint (using clang-tidy)
:: ============================================
echo Linting C/C++ files...

where clang-tidy >nul 2>&1
if errorlevel 1 (
    echo   clang-tidy not found, skipping C++ lint
    goto :done
)

set "CLANG_ARGS="
if defined FIX_MODE set "CLANG_ARGS=--fix"

if exist "packages\compiler\src\main.cpp" (
    echo   Checking: packages\compiler\src\main.cpp
    clang-tidy %CLANG_ARGS% packages\compiler\src\main.cpp -- -std=c++17 2>nul
)

:done
echo Lint complete

endlocal
