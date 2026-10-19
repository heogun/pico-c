pico-8 cartridge // http://www.pico-8.com
version 30
__lua__
cls()
rrectfill(10,10,20,15,4,8)
printh("p10_10="..tostr(pget(10,10)))
printh("p12_12="..tostr(pget(12,12)))
printh("p35_30="..tostr(pget(35,30)))
stop()