pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
reload(0,0,0x4300,"chess.p8")
save("chess_resaved.p8")
reload(0,0,0x4300,"chess_resaved.p8")
for b=0,0x42 do
  local s=0
  for i=0,255 do s=(s+peek(b*256+i))%65536 end
  printh(b..":"..s)
end
stop()
