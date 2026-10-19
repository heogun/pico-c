pico-8 cartridge // http://www.pico8.com
version 41
__lua__
-- reload the data area from the file our writer produced and dump it
reload(0,0,0x4300,"/tmp/p8save_out.p8")
printh("g0="..tostr(peek(0x0000)).." g1="..tostr(peek(0x0001)).." g2="..tostr(peek(0x0002)).." g3="..tostr(peek(0x0003)))
printh("g16="..tostr(peek(0x0010)).." g1fff="..tostr(peek(0x1fff)))
printh("ff0="..tostr(peek(0x3000)).." ffff="..tostr(peek(0x30ff)))
printh("m2000="..tostr(peek(0x2000)).." m2001="..tostr(peek(0x2001)).." m2fff="..tostr(peek(0x2fff)))
printh("s0="..tostr(peek(0x3200))..","..tostr(peek(0x3201)).." h="..tostr(peek(0x3240))..","..tostr(peek(0x3241)))
printh("s2="..tostr(peek(0x3200+2*68))..","..tostr(peek(0x3200+2*68+1)).." h="..tostr(peek(0x3200+2*68+64))..","..tostr(peek(0x3200+2*68+65)))
printh("mus="..tostr(peek(0x3100))..","..tostr(peek(0x3101))..","..tostr(peek(0x3102))..","..tostr(peek(0x3103)))
stop()
