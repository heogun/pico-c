pico-8 cartridge // http://www.pico8.com
version 41
__lua__
local s="\226\172\133\239\184\143"
printh("rawlen="..#s)
local g="⬅️"
printh("glyphlen="..#g)
printh("g1="..ord(g,1).." g2="..ord(g,2).." g3="..ord(g,3).." g4="..ord(g,4))
for k,v in pairs(_ENV) do
  if type(k)=="string" and #k>0 and ord(k,1)>127 then
    local b=""
    for i=1,#k do b=b..ord(k,i).."," end
    printh("K len="..#k.." b="..b)
  end
end
printh("lt_caret_dots="..tostr(ˇ < …))
printh("gt_caret_dots="..tostr(ˇ > …))
printh("lt_block_dot="..tostr(█ < ▒))
printh("gt_block_dot="..tostr(█ > ▒))
printh("lt_caret_ascii="..tostr(ˇ < "zzz"))
printh("gt_caret_ascii="..tostr(ˇ > "zzz"))
printh("eq="..tostr(ˇ == …))
printh("lt_add="..tostr("add" < "Zed"))
printh("lt_A_z="..tostr("Apple" < "zebra"))
stop()
