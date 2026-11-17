@echo off
rem Build pico-c natively with Visual Studio (MSVC) and CMake.
rem
rem   platforms\windows\build-msvc.bat
rem
rem Needs CMake, Git and SDL2 for Visual C++.  With vcpkg ("vcpkg install
rem sdl2:x64-windows") set VCPKG_ROOT; otherwise set SDL2_DIR to the cmake
rem folder of the SDL2 development package for Visual C++.  The binary lands
rem in build\msvc\Release\pico-c.exe, next to SDL2.dll.
rem
rem Visual Studio(MSVC)와 CMake로 pico-c를 네이티브 빌드한다. vcpkg를 쓰면
rem VCPKG_ROOT를, 아니면 SDL2 VC 개발 패키지의 cmake 폴더를 SDL2_DIR에 둔다.
setlocal
set "ROOT=%~dp0..\.."
cd /d "%ROOT%"

set "TOOLCHAIN="
if defined VCPKG_ROOT set "TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

cmake -S . -B build\msvc %TOOLCHAIN% || exit /b 1
cmake --build build\msvc --config Release || exit /b 1
echo built %CD%\build\msvc\Release\pico-c.exe
