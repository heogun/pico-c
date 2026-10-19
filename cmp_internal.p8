pico-8 cartridge // http://www.pico8.com
version 41
__lua__
printh("t1="..tostr(__type(1)))
printh("t2="..tostr(__type("x")))
printh("t3="..tostr(__type(nil)))
printh("t4="..tostr(__type({})))
printh("t5="..tostr(__type(print)))
printh("t6="..tostr(__type(1.5)))
printh("pausemenu="..type(_pausemenu))
local c=0
for k,v in pairs(_pausemenu) do c=c+1 end
printh("pm_keys="..c)
printh("gmis="..tostr(_get_menu_item_selected()))
printh("setfps="..tostr(_set_fps(30)))
printh("markcpu="..tostr(_mark_cpu()))
printh("sfrm="..tostr(_startframe()))
printh("mlx="..tostr(_set_mainloop_exists(1)))
printh("ufr="..tostr(_update_framerate()))
printh("ubtn="..tostr(_update_buttons()))
printh("sds="..tostr(set_draw_slice(0,0,127,127)))
printh("sful="..tostr(_set_mainloop_exists()))
stop()
