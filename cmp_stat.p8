pico-8 cartridge // http://www.pico8.com
version 30
__lua__
poke(0x5f2d,1)
printh("s30t="..type(stat(30)).." v="..tostr(stat(30)))
printh("s31t="..type(stat(31)).." v=["..tostr(stat(31)).."]")
printh("s32="..stat(32).." s33="..stat(33).." s34="..stat(34))
printh("s36="..stat(36).." s38="..stat(38).." s39="..stat(39))
printh("s7="..stat(7))
printh("s57="..tostr(stat(57)).." s54="..stat(54).." s55="..stat(55).." s56="..stat(56))
printh("s46="..stat(46).." s47="..stat(47).." s48="..stat(48).." s49="..stat(49))
printh("s80="..stat(80).." s81="..stat(81).." s82="..stat(82).." s83="..stat(83).." s84="..stat(84))
printh("s90="..stat(90).." s91="..stat(91).." s92="..stat(92).." s93="..stat(93).." s94="..stat(94))
printh("s100="..tostr(stat(100)))
printh("s1t="..type(stat(1)))
stop()
