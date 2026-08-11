@echo off
setlocal

set "ROOT=%~dp0.."
set "EXE=%ROOT%\build\Debug\TankSandbox.exe"
set "WORKING_DIR=%ROOT%\build"

if not exist "%EXE%" (
    echo TankSandbox.exe was not found. Run scripts\build.bat TankSandbox first.
    exit /b 1
)

pushd "%WORKING_DIR%"
start "" "%EXE%" %*
set "RESULT=%ERRORLEVEL%"
popd

exit /b %RESULT%
