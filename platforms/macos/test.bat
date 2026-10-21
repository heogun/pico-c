@echo off
rem ---------------------------------------------------------------------------
rem pico-c is tested on macOS with platforms\macos\test.sh, a POSIX shell
rem script.  This batch file only exists so that every platform folder offers
rem the same set of entry points.
rem
rem   macOS:   platforms/macos/test.sh
rem   Windows: platforms/windows/test.bat
rem ---------------------------------------------------------------------------
echo [error] macOS tests are driven by platforms\macos\test.sh, not by a batch file.
echo         Run:  platforms/macos/test.sh
exit /b 1
