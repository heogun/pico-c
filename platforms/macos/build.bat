@echo off
rem ---------------------------------------------------------------------------
rem pico-c is built on macOS with platforms\macos\build.sh, a POSIX shell
rem script.  This batch file only exists so that every platform folder offers
rem the same set of entry points.
rem
rem   macOS:   platforms/macos/build.sh
rem   Windows: platforms/windows/build.bat
rem ---------------------------------------------------------------------------
echo [error] macOS builds are driven by platforms\macos\build.sh, not by a batch file.
echo         Run:  platforms/macos/build.sh
exit /b 1
