@echo off
:: TML Test Runner Script
:: Usage: scripts\test.bat [options]

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

:: Detect target triple (Windows MSVC)
set "TARGET=x86_64-pc-windows-msvc"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "TARGET=aarch64-pc-windows-msvc"

:: Find compiler (check debug first, then release)
set "TML_EXE=%ROOT_DIR%\build\%TARGET%\debug\Debug\tml.exe"
if not exist "%TML_EXE%" set "TML_EXE=%ROOT_DIR%\build\%TARGET%\debug\tml.exe"
if not exist "%TML_EXE%" set "TML_EXE=%ROOT_DIR%\build\%TARGET%\release\Release\tml.exe"
if not exist "%TML_EXE%" set "TML_EXE=%ROOT_DIR%\build\%TARGET%\release\tml.exe"

if not exist "%TML_EXE%" (
    echo Error: Compiler not found at build\%TARGET%\
    echo Run scripts\build.bat first.
    exit /b 1
)

echo Running TML tests...
echo.

:: Pass all arguments to tml test
"%TML_EXE%" test %*
