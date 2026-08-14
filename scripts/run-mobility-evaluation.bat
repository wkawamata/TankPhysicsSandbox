@echo off
setlocal

set "MODE=%~1"
if "%MODE%"=="" set "MODE=Snapshot"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-mobility-evaluation.ps1" -Settings "%MODE%"
exit /b %ERRORLEVEL%
