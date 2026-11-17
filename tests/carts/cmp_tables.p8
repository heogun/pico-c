pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- add/del/deli/count and the walk of all()/foreach() while the table changes
function show(t) local s="" for i=1,#t do s..=tostr(t[i]).."," end return s.."#"..#t end
t={1,2,3,2,4} r=del(t,2) printh("del first: "..tostr(r).." "..show(t))
printh("del missing: "..tostr(del(t,99)).." "..show(t))
t={5,6,7} printh("deli: "..tostr(deli(t,2)).." "..show(t).." "..tostr(deli(t)).." "..show(t))
t={1,2,3,4,5,6} s="" foreach(t,function(v) s..=v.."," if v==2 or v==3 then del(t,v) end end) printh("foreach del cur: "..s.." "..show(t))
t={1,2,3,4,5,6} s="" for v in all(t) do s..=v.."," if v%2==1 then del(t,v) end end printh("all del odd: "..s.." "..show(t))
t={1,2,3} s="" for v in all(t) do s..=v.."," if v==1 then add(t,9) end end printh("all add: "..s)
t={1,2,3,4} s="" for v in all(t) do s..=v.."," if v==1 then del(t,3) end end printh("all del later: "..s)
t={1,2,3,4} s="" for v in all(t) do s..=v.."," if v==3 then del(t,1) end end printh("all del earlier: "..s)
t={1,2,3} s="" foreach(t,function(v) s..=v.."," if v==1 then t[2]=nil end end) printh("foreach hole: "..s)
printh("count nil: "..tostr(count(nil)))
foreach(nil,print) printh("foreach nil ok")
for v in all(nil) do end printh("all nil ok")
printh("add nil: "..tostr(add(nil,1)))
printh("del nil: "..tostr(del(nil,1)))
printh("deli nil: "..tostr(deli(nil,1)))
printh("end")
t={1,2,3} r=add(t,9) printh("add ret: "..tostr(r).." "..show(t))
t={1,2,3} r=add(t,9,2) printh("add idx: "..tostr(r).." "..show(t))
t={1,2,3} r=add(t,nil) printh("add nil val: "..tostr(r).." "..show(t))
t={"a","b","a",1,1.5} printh("count str: "..count(t,"a").." num: "..count(t,1).." frac: "..count(t,1.5).." all: "..count(t))
t={1,2,3} printh("deli 0: "..tostr(deli(t,0)).." "..show(t).." deli 9: "..tostr(deli(t,9)).." "..show(t))
t={} printh("deli empty: "..tostr(deli(t)).." "..show(t))
t={1,{},3} x=t[2] r=del(t,x) printh("del table: "..tostr(r==x).." "..#t)
printh("end")
