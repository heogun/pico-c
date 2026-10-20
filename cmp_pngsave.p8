pico-8 cartridge // http://www.pico8.com
version 43
__lua__
-- exercise the .p8.png writer: paint several RAM banks, then save as png.
-- (excluded from the oracle battery: the oracle ignores save() under -x)
for i=0,0x1fff do poke(0x0000+i, (i*7+3)%256) end
for i=0,0x0fff do poke(0x2000+i, (i*13)%256) end
for i=0,0x00ff do poke(0x3000+i, (i*29)%256) end
local ok = save("/tmp/p8png_out.p8.png")
printh("save="..tostr(ok))
stop()
