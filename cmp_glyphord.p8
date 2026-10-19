pico-8 cartridge // http://www.pico8.com
version 41
__lua__
local g="⬅️"
printh("len="..tostr(#g))
printh("ord1="..tostr(ord(g,1)))
printh("ord2="..tostr(ord(g,2)))
printh("ord3="..tostr(ord(g,3)))
printh("sub1="..tostr(sub(g,1,1)))
printh("typ="..type(g))
local raw="\226\172\133\239\184\143"
printh("rawlen="..tostr(#raw))
printh("eqraw="..tostr(g==raw))
for k,v in pairs(_ENV) do
  if type(k)=="string" then
    local c=ord(k,1)
    if c and c>127 then printh("K code="..tostr(c).." len="..tostr(#k).." name="..k) end
  end
end
printh("cmp_ascii1="..tostr("add" < "Zed"))
printh("cmp_ascii2="..tostr("Apple" < "zebra"))
printh("cmp_ascii3="..tostr("a" < "B"))
printh("cmp_glyph1="..tostr(ˇ < …))
printh("cmp_glyph2="..tostr(ˇ > …))
printh("cmp_glyph3="..tostr(█ < ▒))
printh("cmp_glyphnum="..tostr(ˇ < 5))
printh("cmp_glyphstr="..tostr(ˇ < "zzz"))
stop()
