systemd_system_unit_dir = get_option('systemd_system_unit_dir')
if systemd_system_unit_dir == ''
  systemd_system_unit_dir = join_paths(get_option('prefix'), 'lib', 'systemd', 'system')
endif

install_data(
  'mpd.socket',
  install_dir: systemd_system_unit_dir,
)

configure_file(
  input: 'mpd.service.in',
  output: 'mpd.service',
  configuration: systemd_unit_conf,
  install_dir: systemd_system_unit_dir,
)
