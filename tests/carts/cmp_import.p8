pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- import() of a .png from a running cart: the picture goes into the sprite
-- sheet with the nearest of the 16 colours, in the ram and in the cart rom
-- (reload() keeps it).  the expected values were measured on pico-8 0.2.7
-- with import_grad.png in its own carts folder.
function sum() local s=0 for i=0,8191 do s+=peek(i) end return s end
printh("before "..sum())
import("import_grad.png")
printh("after import "..sum())
reload(0,0,0x2000)
printh("after reload "..sum())
