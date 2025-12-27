@echo off
setlocal enabledelayedexpansion
:: TML Compiler - Git Hooks Setup Script for Windows
:: Usage: scripts\setup-hooks.bat
::
:: Installs pre-commit and pre-push hooks to .git\hooks\

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

set "HOOKS_DIR=%ROOT_DIR%\.git\hooks"

echo Setting up git hooks...

:: Check if .git directory exists
if not exist "%ROOT_DIR%\.git" (
    echo Error: Not a git repository
    exit /b 1
)

:: Create hooks directory if it doesn't exist
if not exist "%HOOKS_DIR%" mkdir "%HOOKS_DIR%"

:: Create pre-commit hook (uses bash via Git Bash)
(
echo #!/bin/bash
echo # TML Compiler - Pre-commit Hook
echo # Runs lint checks on staged TML files
echo.
echo set -e
echo.
echo ROOT_DIR="$^(git rev-parse --show-toplevel^)"
echo cd "$ROOT_DIR"
echo.
echo echo "Running pre-commit checks..."
echo.
echo # Find TML compiler
echo TML_EXE=""
echo if [ -f "./build/debug/tml.exe" ]; then
echo     TML_EXE="./build/debug/tml.exe"
echo elif [ -f "./build/release/tml.exe" ]; then
echo     TML_EXE="./build/release/tml.exe"
echo fi
echo.
echo # Get staged TML files
echo STAGED_TML=$^(git diff --cached --name-only --diff-filter=ACM ^| grep '\.tml$' ^|^| true^)
echo.
echo if [ -n "$STAGED_TML" ] ^&^& [ -n "$TML_EXE" ]; then
echo     echo "Linting staged TML files..."
echo     for file in $STAGED_TML; do
echo         if [ -f "$file" ]; then
echo             "$TML_EXE" lint "$file" ^|^| {
echo                 echo "Lint failed for: $file"
echo                 exit 1
echo             }
echo         fi
echo     done
echo fi
echo.
echo echo "Pre-commit checks passed!"
) > "%HOOKS_DIR%\pre-commit"

echo Installed: pre-commit hook

:: Create pre-push hook
(
echo #!/bin/bash
echo # TML Compiler - Pre-push Hook
echo # Runs build and tests before pushing
echo.
echo set -e
echo.
echo ROOT_DIR="$^(git rev-parse --show-toplevel^)"
echo cd "$ROOT_DIR"
echo.
echo echo "Running pre-push checks..."
echo.
echo # Build the project
echo if [ -f "./scripts/build.sh" ]; then
echo     ./scripts/build.sh --no-tests
echo fi
echo.
echo # Find TML compiler
echo TML_EXE=""
echo if [ -f "./build/debug/tml.exe" ]; then
echo     TML_EXE="./build/debug/tml.exe"
echo elif [ -f "./build/release/tml.exe" ]; then
echo     TML_EXE="./build/release/tml.exe"
echo fi
echo.
echo # Run TML lint
echo if [ -n "$TML_EXE" ]; then
echo     echo "Running TML lint..."
echo     "$TML_EXE" lint packages examples
echo fi
echo.
echo echo "Pre-push checks passed!"
) > "%HOOKS_DIR%\pre-push"

echo Installed: pre-push hook

echo.
echo Git hooks installed successfully!
echo.
echo Hooks configured:
echo   - pre-commit: Runs tml lint on staged TML files
echo   - pre-push: Runs build, lint, and tests before push
echo.
echo To skip hooks temporarily:
echo   git commit --no-verify
echo   git push --no-verify

endlocal
