pico-8 cartridge // http://www.pico-8.com
version 41
__lua__
for i=1,4 do for y=0,7 do for x=0,7 do sset(i*8+x,y,7+i) end end end
fset(1,0x1) fset(2,0x5) fset(3,0x4) fset(4,0x0)
mset(0,0,1) mset(1,0,2) mset(2,0,3) mset(3,0,4)
cls(0)
map(0,0, 0,0, 4,1, 0x5)
printh("M t0="..pget(0,0).." t1="..pget(8,0).." t2="..pget(16,0).." t3="..pget(24,0))
cls(0)
map(0,0, 0,0, 4,1, 0x0)
printh("M0 t0="..pget(0,0).." t1="..pget(8,0).." t2="..pget(16,0).." t3="..pget(24,0))
stop()
