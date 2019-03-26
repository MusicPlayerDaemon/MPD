rc = find_program('rc')
xres = find_program('xres')

rsrc = custom_target(
  'mpd.rsrc',
  output: 'mpd.rsrc',
  input: 'mpd.rdef',
  command: [rc, '-o', '@OUTPUT@', '@INPUT@'],
)

addres = files('add_resources.sh')
