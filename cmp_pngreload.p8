pico-8 cartridge // http://www.pico8.com
version 43
__lua__
-- read back the .p8.png our writer produced and dump a few data-area bytes
-- (excluded from the oracle battery: the oracle ignores save() under -x)
reload(0,0,0x4300,"/tmp/p8png_out.p8.png")
printh("g17="..tostr(peek(0x0011)).." g1fff="..tostr(peek(0x1fff)))
printh("m2001="..tostr(peek(0x2001)).." m2fff="..tostr(peek(0x2fff)))
printh("ffff="..tostr(peek(0x30ff)).." m2f00="..tostr(peek(0x2f00)))
stop()
