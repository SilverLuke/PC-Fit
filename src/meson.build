project('pcfit', 'c')
gtkdep = dependency('gtk+-3.0')
threadsdep = dependency('threads')

cc = meson.get_compiler('c')
wiiusedep = cc.find_library('wiiuse')
math = cc.find_library('m', required : true)

executable('main', 'main.c', 'pc-fit.c', dependencies : [gtkdep, threadsdep, wiiusedep, math])
