pico-8 cartridge // http://www.pico-8.com
version 30
__lua__
printh("mid="..tostr(mid(1,2,3)))
printh("flr="..tostr(flr(3.7)))
printh("ceil="..tostr(ceil(3.2)))
printh("band="..tostr(band(7,3)))
printh("shl="..tostr(shl(1,3)))
printh("maxmin="..tostr(max(5,2))..","..tostr(min(5,2)))
printh("abs="..tostr(abs(-5)))
local t={10,20,30}
add(t,40)
del(t,20)
printh("count="..tostr(count(t,40))..",len="..tostr(#t))
printh("sub="..sub("hello",2,4))
printh("tonum="..tostr(tonum("12.5")))
printh("chr_ord="..chr(65).."="..tostr(ord("A")))
local a=int_to_bytes and 0 or 0
poke(0x5f20, 123)  poke2(0x5f22, 1000)
printh("peek="..tostr(peek(0x5f20))..","..tostr(peek2(0x5f22)))
memcpy(0x5f30,0x5f20,4)
printh("memcpy_ok")
printh("split="..tostr(split("a,b,c")[2]))
printh("rnd_srand_ok")
srand(123)
printh("rnd1="..tostr(rnd(100)))
printh("rnd2="..tostr(rnd(100)))
local s=sqrt(16) printh("sqrt="..tostr(s))
stop()