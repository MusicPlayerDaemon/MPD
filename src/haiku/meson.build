haiku_conf = configuration_data()
haiku_conf.set('VERSION', meson.project_version())

splitted_version = meson.project_version().split('~')[0].split('.')
haiku_conf.set('VERSION_MAJOR', splitted_version[0])
haiku_conf.set('VERSION_MINOR', splitted_version.get(1, '0'))
haiku_conf.set('VERSION_REVISION', splitted_version.get(2, '0'))
haiku_conf.set('VERSION_EXTRA', splitted_version.get(3, '0'))

mpd_rdef = configure_file(
  input: 'mpd.rdef.in',
  output: 'mpd.rdef',
  configuration: haiku_conf,
)

rc = find_program('rc')
xres = find_program('xres')

rsrc = custom_target(
  'mpd.rsrc',
  output: 'mpd.rsrc',
  input: mpd_rdef,
  command: [rc, '-o', '@OUTPUT@', '@INPUT@'],
)

addres = files('add_resources.sh')
