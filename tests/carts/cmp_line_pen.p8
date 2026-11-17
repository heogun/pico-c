pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- line(x,y[,c]) goes on from the last end (0x5f3c/0x5f3e), line() forgets it;
-- a colour given to any primitive becomes the draw colour (0x5f25)
printh("start 5f35="..peek(0x5f35).." 5f3c="..peek2(0x5f3c).." 5f3e="..peek2(0x5f3e))
cls() line(5,5,8) printh("first line(x,y,c): "..pget(5,5).." "..pget(0,0).." "..pget(3,3))
line(9,5) printh("then line(x,y): "..pget(7,5).." "..pget(9,5))
printh("after 5f35="..peek(0x5f35).." 5f3c="..peek2(0x5f3c).." 5f3e="..peek2(0x5f3e))
line(1,2,30,40,7) printh("full 5f35="..peek(0x5f35).." 5f3c="..peek2(0x5f3c).." 5f3e="..peek2(0x5f3e))
line() printh("reset 5f35="..peek(0x5f35).." 5f3c="..peek2(0x5f3c).." 5f3e="..peek2(0x5f3e))
cls() poke(0x5f35,0) poke2(0x5f3c,50) poke2(0x5f3e,50) line(60,50,9) printh("poked start: "..pget(55,50).." "..pget(50,50))
cls() line(10,10,20,20,8) line(30,10) printh("line2: "..pget(25,10).." "..pget(30,10).." "..pget(21,15))
cls() color(7) line(10,10,20,10) line(20,20,12) printh("line3: "..pget(20,15).." "..pget(20,20).." "..pget(10,10))
cls() line(1,1,3,1,9) line() line(5,5) printh("line0: "..pget(5,5).." "..pget(4,3))
function t(name,f) color(6) f() printh(name.." 5f25="..peek(0x5f25)) end
t("pset",function() pset(1,1,8) end)
t("pget",function() pget(1,1) end)
t("line",function() line(1,1,5,5,9) end)
t("rect",function() rect(1,1,5,5,10) end)
t("rectfill",function() rectfill(1,1,5,5,11) end)
t("circ",function() circ(20,20,3,12) end)
t("circfill",function() circfill(20,20,3,13) end)
t("oval",function() oval(1,1,9,5,14) end)
t("ovalfill",function() ovalfill(1,1,9,5,15) end)
t("rrect",function() rrect(1,1,9,9,2,4) end)
t("print",function() print("x",0,0,3) end)
t("cls",function() cls(5) end)
t("rect hi",function() rect(1,1,5,5,0x18) end)
