@echo off
REM Run TML tests using the EXE-based subprocess execution (Go-style)
REM This is the v2 test system - uses tml test-v2 instead of tml test

%~dp0\..\build\debug\tml.exe test-v2 %*
