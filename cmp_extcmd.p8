pico-8 cartridge // http://www.pico8.com
version 41
__lua__
printh("run","runtest2.txt")
local seen=false
local t=ls(".")
if t then for f in all(t) do if f=="guard.p8" then seen=true end end end
if not seen then printh("g","guard.p8"); extcmd("reset") end
stop()
