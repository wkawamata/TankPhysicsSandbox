@echo off
setlocal
set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set BUILD_DIR=build

if "%1"=="" (set TARGET=TankSandbox) else (set TARGET=%1)
%CMAKE% --build "%BUILD_DIR%" --config Debug --target %TARGET%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo --- Build done: %TARGET% ---
endlocal
