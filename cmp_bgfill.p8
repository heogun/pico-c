pico-8 cartridge // http://www.pico-8.com
version 41
__lua__
for y=0,7 do for x=0,7 do sset(x,y,12) end end   -- sprite 0 = color 12
mset(0,0,0)
cls(1)
map(0,0, 0,0, 1,1)
printh("map_nofill p0="..pget(0,0))
cls(1)
poke(0x5f36,0x08)
map(0,0, 0,0, 1,1)
printh("map_fill p0="..pget(0,0))
cls(1)
tline(0,0,3,0, 0,0)
printh("tline_fill p0="..pget(0,0))
stop()
