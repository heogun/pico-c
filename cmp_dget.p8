pico-8 cartridge // http://www.pico-8.com
version 30
__lua__
cartdata("cmp")
if dget(0)==0 then dset(0, 123.5) end
printh("val="..tostr(dget(0)))
stop()