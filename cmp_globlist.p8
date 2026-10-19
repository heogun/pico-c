pico-8 cartridge // http://www.pico8.com
version 41
__lua__
local names={}
for k,v in pairs(_ENV) do add(names,k) end
local function cmp(a,b) return a<b end
-- simple insertion sort (PICO-8 has sort, but keep it safe)
for i=2,#names do local x=names[i] local j=i-1 while j>=1 and names[j]>x do names[j+1]=names[j] j=j-1 end names[j+1]=x end
for n in all(names) do printh(n.."="..type(_ENV[n])) end
stop()
