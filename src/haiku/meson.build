rc = meson.find_program('rc')
xres = meson.find_program('xres')

rsrc = custom_target(
  'mpd.rsrc',
  output: 'mpd.rsrc',
  input: 'mpd.rdef',
  command: [rc, '-o', '@OUTPUT@', '@INPUT@'],
)

custom_target(
  'mpd.rsrc',
  output: 'mpd',
  input: [mpd, rsrc],
  command: [xres, '-o', '@OUTPUT@', '--', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
