pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- charset parity: string semantics, identifiers, output transcoding
-- ⬆️ = glyph 148, あ = 154, ☉ = 136, ▶ = 23, ▮ = 16, っ = 200
-- 😀 and 한 are unknown to PICO-8 and stay raw UTF-8 bytes
local s="⬆️あ😀한☉▶"
printh("len="..#s)
for i=1,#s do printh("o"..i.."="..ord(s,i)) end
printh("eq="..tostr("⬆️"==chr(148)))
printh("len1="..#"⬆️".." asc="..ord("⬆️"))
printh("kana="..#"▮っ")
printh("kord="..ord("▮っ",1)..","..ord("▮っ",2))
local ☉=7
printh("id="..☉)
local u="한"
printh("u="..#u..","..ord(u,1)..","..ord(u,2)..","..ord(u,3))
printh("out=<"..s..">")
printh("chr154=<"..chr(154)..">")
printh("chr139=<"..chr(139)..">")
printh("chr0=<"..chr(0)..">")
printh("sub=<"..sub(s,1,2)..">")
printh("subk=<"..sub("あ😀한",1,1)..">")
printh("cat=<"..chr(1).." "..chr(127).." "..chr(31)..">")
stop()
