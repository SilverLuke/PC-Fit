gtkdep = dependency('gtk+-3.0')
threadsdep = dependency('threads')

cc = meson.get_compiler('c')
xwiimote = cc.find_library('xwiimote')
math = cc.find_library('m', required : true)

sources = files(
	'main.c',
	'gui.c',
	'config.c',
	'pc-fit.c',
	'mytime.c',
	'lib/tomlc99/toml.c'
)

executable('main', sources, gui_app: true, install: true, dependencies : [gtkdep, threadsdep, xwiimote, math])

