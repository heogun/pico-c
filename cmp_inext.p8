pico-8 cartridge // http://www.pico8.com
version 41
__lua__
local t={10,20,30}
local i,v=inext(t)
printh("a i="..tostr(i).." v="..tostr(v))
i,v=inext(t,1)
printh("b i="..tostr(i).." v="..tostr(v))
i,v=inext(t,3)
printh("c i="..tostr(i).." v="..tostr(v))
local u={} u[2]="x" u[5]="y"
i,v=inext(u)
printh("d i="..tostr(i).." v="..tostr(v))
i,v=inext(u,2)
printh("e i="..tostr(i).." v="..tostr(v))
stop()
