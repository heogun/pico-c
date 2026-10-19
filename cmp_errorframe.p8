pico-8 cartridge // http://www.pico8.com
version 43
__lua__
local function inner(x)
  return x.y
end
local function outer()
  inner(nil)
end
outer()
