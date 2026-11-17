pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- serial(0x805) writes ram bytes to stdout and returns the count
for i=0,9 do poke(0x4300+i,72+i) end
poke(0x430a,10)
printh("before")
printh("ret "..tostr(serial(0x805,0x4300,11)))
printh("ret "..tostr(serial(0x805,0x4300,0)))
printh("ret "..tostr(serial(0x123,0x4300,4)))
printh("after")
