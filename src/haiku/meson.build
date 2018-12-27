rc = find_program('rc')
xres = find_program('xres')

rsrc = custom_target(
  'mpd2.rsrc',
  output: 'mpd2.rsrc',
  input: 'mpd.rdef',
  command: [rc, '-q -o', '@OUTPUT@', '@INPUT@'],
)

custom_target(
  'mpd.rsrc',
  output: 'mpd',
  input: [mpd, rsrc],
  command: [xres, '-o', '@OUTPUT@', '--', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
