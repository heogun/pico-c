pico-8 cartridge // http://www.pico-8.com
version 41
__lua__
for i=1,2 do for y=0,7 do for x=0,7 do sset(i*8+x,y,7+i) end end end
mset(0,0,1) mset(0,1,2)
poke(0x5f39,2) poke(0x5f3b,1)
cls(0)
tline(0,0,0,15, 0,0, 0,0.125)
printh("my p0="..pget(0,0).." p8="..pget(0,8))
stop()
