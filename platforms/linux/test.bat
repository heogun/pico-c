@echo off
rem ---------------------------------------------------------------------------
rem pico-c is tested on Linux with platforms\linux\test.sh, a POSIX shell
rem script.  This batch file only exists so that every platform folder offers
rem the same set of entry points.
rem
rem   Linux:   platforms/linux/test.sh
rem   Windows: platforms/windows/test.bat
rem ---------------------------------------------------------------------------
echo [error] Linux tests are driven by platforms\linux\test.sh, not by a batch file.
echo         Run:  platforms/linux/test.sh
exit /b 1
