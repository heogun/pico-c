pico-8 cartridge // http://www.pico8.com
version 41
__lua__
printh("pcall="..type(pcall))
printh("xpcall="..type(xpcall))
printh("select="..type(select))
printh("pairs="..type(pairs))
printh("setmetatable="..type(setmetatable))
printh("rawget="..type(rawget))
printh("tostring="..type(tostring))
printh("loadstring="..type(loadstring))
printh("dofile="..type(dofile))
printh("require="..type(require))
printh("os="..type(os))
printh("io="..type(io))
printh("_ENV="..type(_ENV))
printh("_G="..type(_G))
stop()
