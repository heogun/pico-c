@echo off
rem ---------------------------------------------------------------------------
rem pico-c is built on Linux with platforms\linux\build.sh, a POSIX shell
rem script.  This batch file only exists so that every platform folder offers
rem the same set of entry points.
rem
rem   Linux:   platforms/linux/build.sh
rem   Windows: platforms/windows/build.bat
rem ---------------------------------------------------------------------------
echo [error] Linux builds are driven by platforms\linux\build.sh, not by a batch file.
echo         Run:  platforms/linux/build.sh
exit /b 1
