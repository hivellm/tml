@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   TML Profile - Run All Benchmarks
echo ============================================
echo.

cd /d "%~dp0"

:: Check if binaries exist
if not exist "results\bin" (
    echo ERROR: No binaries found. Run build.bat first.
    exit /b 1
)

:: Create timestamp for this run
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /format:list') do set datetime=%%I
set TIMESTAMP=%datetime:~0,8%_%datetime:~8,6%
set REPORT_FILE=results\report_%TIMESTAMP%.txt

echo Running benchmarks...
echo Results will be saved to: %REPORT_FILE%
echo.

:: Initialize report
echo ============================================ > "%REPORT_FILE%"
echo   TML vs C++ Performance Report >> "%REPORT_FILE%"
echo   Generated: %date% %time% >> "%REPORT_FILE%"
echo ============================================ >> "%REPORT_FILE%"
echo. >> "%REPORT_FILE%"

:: Categories to benchmark
set CATEGORIES=math string json collections memory text

for %%c in (%CATEGORIES%) do (
    echo.
    echo ========================================
    echo   Running %%c benchmarks
    echo ========================================

    echo. >> "%REPORT_FILE%"
    echo ======================================== >> "%REPORT_FILE%"
    echo   %%c Benchmarks >> "%REPORT_FILE%"
    echo ======================================== >> "%REPORT_FILE%"

    :: Run C++ version
    set CPP_EXE=results\bin\%%c_bench.exe
    if exist "!CPP_EXE!" (
        echo.
        echo --- C++ ---
        echo. >> "%REPORT_FILE%"
        echo --- C++ Results --- >> "%REPORT_FILE%"
        "!CPP_EXE!" >> "%REPORT_FILE%" 2>&1
        "!CPP_EXE!"
    ) else (
        echo [C++ benchmark not found: !CPP_EXE!]
        echo [C++ benchmark not found] >> "%REPORT_FILE%"
    )

    :: Run TML version
    set TML_EXE=results\bin\%%c_bench.exe
    :: TML builds with same name, check if it's different
    :: For now, run the TML source directly if no separate exe

    set TML_SRC=tml\%%c_bench.tml
    if exist "!TML_SRC!" (
        echo.
        echo --- TML ---
        echo. >> "%REPORT_FILE%"
        echo --- TML Results --- >> "%REPORT_FILE%"

        set TML_COMPILER=..\build\release\tml.exe
        if not exist "!TML_COMPILER!" set TML_COMPILER=..\build\debug\tml.exe

        if exist "!TML_COMPILER!" (
            "!TML_COMPILER!" run "!TML_SRC!" --release >> "%REPORT_FILE%" 2>&1
            "!TML_COMPILER!" run "!TML_SRC!" --release
        ) else (
            echo [TML compiler not found]
            echo [TML compiler not found] >> "%REPORT_FILE%"
        )
    ) else (
        echo [TML benchmark not found: !TML_SRC!]
    )
)

echo.
echo ============================================
echo   Benchmark Run Complete
echo ============================================
echo.
echo Full report saved to: %REPORT_FILE%
echo.

endlocal
