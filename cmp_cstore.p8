pico-8 cartridge // http://www.pico8.com
version 41
__lua__
poke(0x100,0xab) poke(0x101,0xcd)
cstore(0x1000, 0x100, 2, "cstore_base.p8")
reload(0x200, 0x1000, 2, "cstore_base.p8")
printh("v="..peek(0x200)..","..peek(0x201))
stop()
