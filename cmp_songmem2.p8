pico-8 cartridge // http://www.pico8.com
version 30
__lua__
printh("song0-7="..peek(0x3100)..","..peek(0x3101)..","..peek(0x3102)..","..peek(0x3103)..","..peek(0x3104)..","..peek(0x3105)..","..peek(0x3106)..","..peek(0x3107))
printh("song63="..peek(0x31fc)..","..peek(0x31fd)..","..peek(0x31fe)..","..peek(0x31ff))
printh("sfx1="..peek(0x3244)..","..peek(0x3245))
printh("gff0="..peek(0x3000).." rob="..peek(0x4300))
stop()
