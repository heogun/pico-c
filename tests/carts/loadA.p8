pico-8 cartridge // http://www.pico8.com
version 41
__lua__
printh("A: before")
local ok=load("loadB.p8","BACKTOA","hello42")
printh("A: load returned "..tostr(ok))
stop()
