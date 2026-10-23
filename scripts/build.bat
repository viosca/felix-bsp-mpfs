@echo off
REM ============================================================================
REM build.bat - Headless CLI Build Helper for Felix OS (DDR & LIM Targets)
REM Usage:
REM   scripts\build.bat                       (Builds both DDR and LIM)
REM   scripts\build.bat -Target ddr -Payload  (Builds DDR and generates payload.bin)
REM   scripts\build.bat -Target lim -Clean    (Clean builds LIM)
REM ============================================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %ERRORLEVEL%
