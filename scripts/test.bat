@echo off
:: TML Test Runner Script
:: Usage: scripts\test.bat [options]

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

:: Find compiler
set "TML_EXE=%ROOT_DIR%\build\debug\Debug\tml.exe"
if not exist "%TML_EXE%" set "TML_EXE=%ROOT_DIR%\build\debug\tml.exe"
if not exist "%TML_EXE%" set "TML_EXE=%ROOT_DIR%\build\release\Release\tml.exe"
if not exist "%TML_EXE%" set "TML_EXE=%ROOT_DIR%\build\release\tml.exe"

if not exist "%TML_EXE%" (
    echo Error: Compiler not found. Run scripts\build.bat first.
    exit /b 1
)

echo Running TML tests...
echo.

:: Pass all arguments to tml test
"%TML_EXE%" test %*
