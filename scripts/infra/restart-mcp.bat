@echo off
REM Restart TML MCP Server
powershell -ExecutionPolicy Bypass -File "%~dp0restart-mcp.ps1"
