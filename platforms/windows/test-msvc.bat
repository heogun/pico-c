@echo off
rem Run the regression suite against the MSVC build (needs Python 3).
rem
rem   platforms\windows\test-msvc.bat [cart ...]
rem
rem MSVC 빌드로 회귀 스위트를 돌린다(Python 3 필요).
setlocal
set "ROOT=%~dp0..\.."
cd /d "%ROOT%"
python tests\tools\run_tests.py --bin build\msvc\Release\pico-c.exe %*
