@echo off
rem ---------------------------------------------------------------------------
rem Build pico-c on Windows (MSYS2 / MinGW-w64).
rem
rem The MinGW SDL2 lives in platforms\windows\deps\SDL2-2.30.11, the Lua
rem runtime is cloned into platforms\common\deps\z8lua.  The binary lands in
rem build\windows\pico-c.exe.
rem
rem   platforms\windows\build.bat          build
rem   platforms\windows\build.bat -j8      build in parallel
rem   platforms\windows\build.bat clean    remove build\windows
rem
rem Run this from an MSYS2 shell so that GNU make is on the PATH.
rem ---------------------------------------------------------------------------
setlocal

set "ROOT=%~dp0..\.."
cd /d "%ROOT%"

where make >nul 2>&1
if errorlevel 1 (
  if exist "C:\msys64\usr\bin\make.exe" (
    set "PATH=C:\msys64\usr\bin;C:\msys64\mingw64\bin;%PATH%"
  ) else (
    echo [error] GNU make not found.  Run this script from an MSYS2 shell.
    exit /b 1
  )
)

if "%CXX%"=="" set "CXX=x86_64-w64-mingw32-g++"
if "%AR%"=="" set "AR=x86_64-w64-mingw32-ar"

echo building for windows ...
make PLATFORM=windows CXX=%CXX% AR=%AR% %*
if errorlevel 1 exit /b 1

echo built build\windows\pico-c.exe
endlocal
