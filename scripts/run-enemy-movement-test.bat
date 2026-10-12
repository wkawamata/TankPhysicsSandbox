@echo off
setlocal
set "ROOT=%~dp0.."
set "EXE=%ROOT%\build-working2\Release\TankSandbox.exe"
set "WORKING_DIR=%ROOT%\build-working2"
if not exist "%EXE%" (
    echo Build Release TankSandbox in build-working2 first.
    exit /b 1
)
pushd "%WORKING_DIR%"
start "" "%EXE%" --scene tracked-vehicle --map "%ROOT%\Assets\Map\EnemyMovementTest" %*
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%