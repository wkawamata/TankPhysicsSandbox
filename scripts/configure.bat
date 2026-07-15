@echo off
setlocal
set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set VCPKG_CHAIN=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
set BUILD_DIR=build

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
%CMAKE% -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_CHAIN%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo --- Configure done ---
endlocal
