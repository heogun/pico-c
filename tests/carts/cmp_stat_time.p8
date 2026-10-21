pico-8 cartridge // http://www.pico8.com
version 30
__lua__
-- The clock cannot be compared literally: the values change while the cart
-- runs, so they would make the transcript flaky.  This cart only prints
-- properties that hold at every point in time, which makes the transcript
-- stable and still comparable with the oracle.
local function inrange(lo,hi,v) return v>=lo and v<=hi end
printh("utc_year="..tostr(stat(80)>=2020 and stat(80)<=2100))
printh("utc_month="..tostr(inrange(1,12,stat(81))))
printh("utc_day="..tostr(inrange(1,31,stat(82))))
printh("utc_hour="..tostr(inrange(0,23,stat(83))))
printh("utc_min="..tostr(inrange(0,59,stat(84))))
printh("utc_sec="..tostr(inrange(0,59,stat(85))))
printh("local_year="..tostr(stat(90)>=2020 and stat(90)<=2100))
printh("local_month="..tostr(inrange(1,12,stat(91))))
printh("local_day="..tostr(inrange(1,31,stat(92))))
printh("local_hour="..tostr(inrange(0,23,stat(93))))
printh("local_min="..tostr(inrange(0,59,stat(94))))
printh("local_sec="..tostr(inrange(0,59,stat(95))))
stop()
