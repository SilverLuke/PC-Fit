project('pcfit', 'c')
gtkdep = dependency('gtk+-3.0')
threadsdep = dependency('threads')

cc = meson.get_compiler('c')
wiiusedep = cc.find_library('wiiuse')

executable('main', 'main.c', dependencies : [gtkdep, threadsdep, wiiusedep])
