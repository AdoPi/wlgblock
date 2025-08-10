project(
  'gbcc',
  'c',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'optimization=3',
    'buildtype=release',
    'warning_level=3',
    'b_lto=true',
    'b_lto_threads=-1',
    'b_pie=true',
    'prefix=/usr'
  ],
)

debug = get_option('buildtype').startswith('debug')

if debug
  add_project_arguments('-DDEBUG', language : 'c')
endif

data_location = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'gbcc'
)

data_location = '.'

project_source = meson.project_source_root()

tileset_location = join_paths(
  data_location,
  'tileset.png'
)

printer_sound_location = join_paths(
  data_location,
  'print.wav'
)

camera_location = join_paths(
  data_location,
  'camera.png'
)

gtk_ui_location = join_paths(
  data_location,
  'gbcc.ui'
)

shader_location = join_paths(
  data_location,
  'shaders/'
)

icon_location = join_paths(
  data_location,
  ''
)

completion_location = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'bash-completion',
  'completions'
)

add_project_arguments(
  [
    '-pedantic',
    #'-Wconversion',
    '-Wshadow',
    '-Wno-unused-parameter',
    '-D_POSIX_C_SOURCE=200809L',
    '-D_GNU_SOURCE',
    '-DTILESET_PATH="@0@"'.format(tileset_location),
    '-DPRINTER_SOUND_PATH="@0@"'.format(printer_sound_location),
    '-DGTK_UI_PATH="@0@"'.format(gtk_ui_location),
    '-DCAMERA_PATH="@0@"'.format(camera_location),
    '-DSHADER_PATH="@0@"'.format(shader_location),
    '-DICON_PATH="@0@"'.format(icon_location),
    #'-fprofile-instr-use',
    #'-fprofile-instr-generate',
  ],
  language: 'c'
)

if target_machine.system() == 'linux'
  camera_platform = 'src/camera_platform/v4l2.c'
else
  camera_platform = 'src/camera_platform/null.c'
endif

common_sources = files(
  'src/apu.c',
  'src/args.c',
  'src/audio.c',
  'src/audio_platform/openal.c',
  'src/bit_utils.c',
  'src/camera.c',
  camera_platform,
  'src/cheats.c',
  'src/colour.c',
  'src/config.c',
  'src/core.c',
  'src/cpu.c',
  'src/debug.c',
  'src/fontmap.c',
  'src/gbcc.c',
  'src/hdma.c',
  'src/input.c',
  'src/mbc.c',
  'src/memory.c',
  'src/menu.c',
  'src/ops.c',
  'src/palettes.c',
  'src/paths.c',
  'src/ppu.c',
  'src/printer.c',
  'src/printer_platform/terminal.c',
  'src/save.c',
  'src/screenshot.c',
  'src/time_diff.c',
  'src/wav.c',
  'src/window.c',
  'src/vram_window.c'
)

wayland_sources = files(
  'src/wayland/main.c',
)

cc = meson.get_compiler('c')
png = dependency('libpng')
gl = dependency('gl')
epoxy = dependency('epoxy')
openal = dependency('openal')
thread = dependency('threads')
wayland_client = dependency('wayland-client')
wayland_egl = dependency('wayland-egl')
egl = dependency('egl')

mathm = cc.find_library('m')

lock_sources = files('src/ext-session-lock-v1-client-protocol.c')

executable(
  'wlgblock',
  wayland_sources + common_sources + lock_sources,
  dependencies: [wayland_client, wayland_egl, egl, epoxy, openal, png, gl, thread, mathm],
  install: true,
)

install_data(
  'tileset.png'
)

install_data(
  'print.wav'
)

install_data(
  'camera.png'
)

install_subdir(
  'shaders',
  install_dir: data_location
)

install_subdir(
  'icons',
  install_dir: data_location
)

license_dir = join_paths(
  get_option('datadir'),
  'licenses',
  'gbcc'
)

install_data(
  'LICENSE',
  install_dir: license_dir
)

install_data(
  'completions/gbcc',
  install_dir: completion_location
)

scdoc = find_program('scdoc', required: get_option('man-pages'))
if scdoc.found()
  sed = find_program('sed')
  sh = find_program('sh')
  mandir = get_option('mandir')
  manpage = 'doc/gbcc.6.scd'
  output = 'gbcc.6'
  custom_target(
    output,
    input: manpage,
    output: output,
    command: [
      sh, '-c', '@0@ < @INPUT@ | sed "s|TILESET_PATH|@1@|g; s|SHADER_PATH|@2@|g; s|PRINTER_SOUND_PATH|@3@|g; s|ICON_PATH|@4@|g;" > @5@'.format(scdoc.path(), tileset_location, shader_location, printer_sound_location, icon_location, output)
    ],
    install: true,
    install_dir: '@0@/man6'.format(mandir)
  )
endif
