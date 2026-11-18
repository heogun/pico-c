@echo off
rem Compare pico-c with the PICO-8 installed on this machine (needs Python 3).
rem
rem   platforms\windows\oracle.bat [--bin PICO_C] [--record] [cart ...]
rem
rem See platforms\windows\oracle.py for the details.  PICO8 overrides where
rem pico8.exe is (default C:\Program Files (x86)\PICO-8\pico8.exe).
rem
rem 이 PC에 설치된 PICO-8과 pico-c를 비교한다(Python 3 필요). 자세한 것은
rem platforms\windows\oracle.py. pico8.exe 위치는 PICO8로 바꿀 수 있다.
setlocal
set "ROOT=%~dp0..\.."
cd /d "%ROOT%"
python platforms\windows\oracle.py %*
