pico-8 cartridge // http://www.pico8.com
version 41
__lua__
local a=info()
printh("info_ret="..type(a))
local b=assert(true,"x")
printh("assert_true_ret="..type(b).." val="..tostr(b))
stop()
