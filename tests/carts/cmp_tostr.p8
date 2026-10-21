pico-8 cartridge // http://www.pico8.com
version 41
__lua__
printh("a=["..tostr().."]")
local function f() end
printh("b=["..tostr(f()).."]")
printh("c=["..tostr(nil).."]")
printh("d=["..tostr(false).."]")
printh("e=["..tostr(1).."]")
printh("f=["..tostr(1.5).."]")
printh("g=["..tostr({}).."]")
stop()
