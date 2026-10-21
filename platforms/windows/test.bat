@echo off
rem ---------------------------------------------------------------------------
rem Run the pico-c regression suite on Windows (MSYS2 / MinGW-w64).
rem
rem Every cart of tests\carts that has a golden transcript in tests\expected is
rem run headless and compared with that transcript.  The transcripts were
rem produced by the real PICO-8 (see platforms\macos\oracle.sh).
rem
rem   platforms\windows\test.bat             the whole suite
rem   platforms\windows\test.bat cmp_all.p8  a single cart
rem   platforms\windows\test.bat --rebuild   build first
rem
rem The comparison itself is done by platforms\common\test_common.sh, so this
rem script needs bash (MSYS2 or Git for Windows).
rem ---------------------------------------------------------------------------
setlocal

set "ROOT=%~dp0..\.."
cd /d "%ROOT%"

where bash >nul 2>&1
if errorlevel 1 (
  if exist "C:\msys64\usr\bin\bash.exe" (
    set "PATH=C:\msys64\usr\bin;C:\msys64\mingw64\bin;%PATH%"
  ) else (
    echo [error] bash not found.  Run this script from an MSYS2 shell.
    exit /b 1
  )
)

rem Split the arguments: --rebuild asks for a build, the rest are carts.
set "REBUILD="
set "CARTS="
:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--rebuild" goto setrebuild
set "CARTS=%CARTS% %1"
shift
goto parse
:setrebuild
set "REBUILD=1"
shift
goto parse
:parsed

if defined REBUILD goto build
if exist "build\windows\pico-c.exe" goto run
:build
echo building for windows ...
call platforms\windows\build.bat
if errorlevel 1 exit /b 1

:run
bash platforms/common/test_common.sh windows%CARTS%
if errorlevel 1 exit /b 1
endlocal
