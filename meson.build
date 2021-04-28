project('pcfit', 'c',
	  license: 'GPL3',
	  version: '0.1')

sources = files(
	'src/main.c',
	'src/gui.c',
	'src/config.c',
	'src/pc-fit.c',
	'src/mytime.c',
	'lib/tomlc99/toml.c',
	'lib/libxdg-basedir/src/basedir.c',
)

cc = meson.get_compiler('c')

deps = [
    dependency('gtk+-3.0'),
    dependency('threads'),
    cc.find_library('xwiimote'),
    cc.find_library('m'),
    cc.find_library('bluetooth')
]

executable('main', sources, gui_app: true, dependencies: deps)

