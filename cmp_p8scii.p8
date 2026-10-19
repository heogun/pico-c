pico-8 cartridge // http://www.pico8.com
version 43
__lua__
-- p8scii control codes: colours, repeat, \^i/w/t/p modes, cursor moves.
function dump()
  local s=""
  for y=0,34 do
    for x=0,47 do
      local b=peek(0x6000+y*64+flr(x/2))
      local n=0
      if x%2==0 then n=b%16 else n=flr(b/16) end
      s=s..tostr(n)
    end
    s=s.."\n"
  end
  return s
end
local function case(label,str,px,py)
  cls(1)
  local w=print(str,px or 4,py or 4,7)
  printh("CASE "..label.." w="..tostr(w).."\n"..dump())
end
case("plain","hi")
case("fg","\f8hi")
case("fg2","\f9hi\f2yo")
case("fg_invalid","\fGhi")
case("bg","\#3hi")
case("bginvalid","\#Ghi")
case("fg_bg","\#3\f8hi")
case("invert","\^ihi")
case("invert_bg","\#5\^ihi")
case("invert_off","\^iA\^-iB")
case("wide","\^whi")
case("wide_off","\^wA\^-wB")
case("wide_bg","\#3\^whi")
case("tall","\^thi")
case("tall_bg","\#3\^thi")
case("wide_tall","\^w\^tA")
case("pinball","\^pA")
case("pinball_bg","\#3\^pA")
case("repeat3","\*3a")
case("repeat0","\*0ab")
case("repeat_invalid","\*:ab")
case("repeat_after","\*3ab")
case("repeat_wide","\^w\*3a")
case("backspace","abc\bX")
case("tab","x\ty")
case("newline","x\ny")
case("carriage","x\ry")
case("wide128","\^w"..chr(128))
case("tall128","\^t"..chr(128))
case("small","\^w"..chr(16))
case("multi","\f8ab\n\f9cd")
-- print(str, col) keeps the cursor; the cursor then follows the last line.
cls(1)
cursor(30,20,9)
local a=print("QR",4)
local bx,by=peek(0x5f26),peek(0x5f27)
printh("CURSOR a="..tostr(a).." x="..tostr(bx).." y="..tostr(by).."\n"..dump())
cls(1)
local c=print("AB",6,8,7)
local dx,dy=peek(0x5f26),peek(0x5f27)
printh("CURSORXY c="..tostr(c).." x="..tostr(dx).." y="..tostr(dy).."\n"..dump())
cls(1)
local e=print("A\f8B")
printh("WIDTHS e="..tostr(e).."\n"..dump())
stop()
