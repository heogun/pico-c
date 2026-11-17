pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- what pal() stores: the draw palette keeps transparency + 4 bits, the screen
-- palette (0x5f10) and the secondary palette (0x5f60) the whole byte
for p=0,2 do
 local s="p"..p..":"
 for v in all({0x05,0x1c,0x8c,0x9c,0xfc,0x13}) do
  pal() pal(3,v,p) s..=" "..v.."->"..peek(({0x5f00,0x5f10,0x5f60})[p+1]+3)
 end
 printh(s)
end
pal() pal({[2]=0x8c,[3]=0x9c},1) printh("table: "..peek(0x5f12).." "..peek(0x5f13))
pal() palt(3,true) pal(3,0x8c,0) printh("draw keeps transp: "..peek(0x5f03))
s="" for i=0,15 do s..=peek(0x5f60+i).."," end printh("boot "..s)
pal(5,0x77,2) pal() s="" for i=0,15 do s..=peek(0x5f60+i).."," end printh("pal() "..s)
pal(5,0x77,2) pal(2) s="" for i=0,15 do s..=peek(0x5f60+i).."," end printh("pal(2) "..s)
