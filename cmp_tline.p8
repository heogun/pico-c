pico-8 cartridge // http://www.pico-8.com
version 30
__lua__
for sy=0,7 do for sx=0,7 do sset(8+sx, sy, (sx+sy)%16+1) end end
mset(0,0,1)
cls(0)
tline(0,0,8,8,0,0)
printh("p0_0="..tostr(pget(0,0)))
printh("p4_4="..tostr(pget(4,4)))
printh("p8_8="..tostr(pget(8,8)))
stop()