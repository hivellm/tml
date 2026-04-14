@echo off
:: Zig AR wrapper for CMake static library creation
:: Delegates to `zig ar` which is LLVM-ar compatible.
::
:: CMake passes `qc` (quick-append, create) which APPENDS to an existing
:: archive without removing old members. This causes stale obj files to
:: accumulate during incremental rebuilds, making the linker pick old symbols.
:: Delete the archive before recreating to ensure only current obj files are
:: included (equivalent to using `rc` replace mode but simpler to implement).
if exist %2 del /f /q %2 2>nul
zig ar %*
