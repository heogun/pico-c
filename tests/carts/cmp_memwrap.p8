pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- addresses wrap around at 64kb; peek4/poke4 move the raw 16.16 bits
poke(0,9,9,9) poke(0xfffe,1,2,3,4)
printh("poke wrap: "..peek(0xfffe).." "..peek(0xffff).." "..peek(0).." "..peek(1).." "..peek(2))
poke(0,9,9,9) poke2(0xffff,0x0807)
printh("poke2 edge: "..peek(0xffff).." "..peek(0))
poke(0,9,9,9) memset(0xfffe,5,4)
printh("memset edge: "..peek(0xfffe).." "..peek(0xffff).." "..peek(0).." "..peek(1))
printh("peek4 edge: "..tostr(peek4(0xfffe),true))
printh("peek n edge: "..tostr(peek(0xfffe,4)))
poke4(0x4300,0x1234.5678) printh(tostr(peek4(0x4300),true))
poke(0x4300,1,2,3,4) printh(tostr(peek4(0x4300),true))
