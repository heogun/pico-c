pico-8 cartridge // http://www.pico8.com
version 43
__lua__
-- Token-limit probe.
--
-- A cart with more than 8192 tokens is loaded but never run: PICO-8
-- prints the exact count it computed, which is the only way to read a
-- cart's token count back out of the oracle.  Every repeated line below
-- pins one lexer rule measured for pico-c (progress.md, section 3-4), so
-- the reported total is a regression test for the whole tokenizer.
--
-- The cart is deliberately over the limit, so it never reaches the
-- parser; the last block below is only meaningful to the lexer.
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
a=1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
local t=-1
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=f(-1,{a.b,c:d()})
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a . b
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a : b()
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=a.b.c
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1 .. 2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1.5..2
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=1e5
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0x1f
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0b101
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=0x1p3
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-.5
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=-...
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=(...,-...)
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
x=...*-...
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
do x=1 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
if x then x=1 else x=2 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
while x do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for i=1,10 do x=1 end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
for k in pairs(t) do end
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
repeat x=1 until x
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
::lbl:: x=1
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1//comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --comment
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[[ block ]] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x=1 --[==[ deep ]==] +2
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x="a\"b"
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[[long]]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=[==[long]==]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a[1][2]
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=a and b or not c
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=not not a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=-a
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#t
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=#a+#b
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=1+2*3^4%5
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a<<b|c&d~e
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=a>>>b
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=1~=2
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a..=b
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=a+=1
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x=f()()
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={k=-1}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={[1]=2}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
x={...}
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
local function f() end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=function(a) return a end
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=a==b and c<=d or e>f
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=#t>0 and t[h] or nil
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=bnot a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
x=shl a
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
return 1
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
break
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
goto lbl
-- lexer-only probes
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
x=1 ; -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
local -1
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if -1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
if 1 then -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
for i=1,2 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
while 1 do -1=1 end
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
repeat -1=1 until 1
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
for k in -1 do end
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
goto -1
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
x=-..
stop()
