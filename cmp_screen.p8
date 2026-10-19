pico-8 cartridge // http://www.pico8.com
version 43
__lua__
-- compares the screen RAM that print() leaves behind: ASCII, the funny-font
-- codes, the wide glyphs (real 7x5 bitmaps, 8px advance), raw UTF-8 bytes of
-- characters the charset does not know, odd coordinates and edge clipping.
-- (p8scii control codes like \f8 / \#3 are not covered yet -- see progress.md)
cls(0)
print("A⬆️b",0,0,7)
print("█▒⬇️☉한あ",0,8,7)
print("♪☉✽",0,15,6)
print(chr(16)..chr(31)..chr(127),3,22,7)
print("⬆️",124,30,7)
print("▓",60,36,1)
for y=0,47 do
  local s=""
  for x=0,63 do s=s..tostr(peek(0x6000+y*64+x)).."," end
  printh(s)
end
stop()
