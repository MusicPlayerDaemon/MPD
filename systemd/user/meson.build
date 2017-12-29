systemd_user_unit_dir = get_option('systemd_user_unit_dir')
if systemd_user_unit_dir == ''
  systemd_user_unit_dir = join_paths(get_option('prefix'), 'lib', 'systemd', 'user')
endif

configure_file(
  input: 'mpd.service.in',
  output: 'mpd.service',
  configuration: systemd_unit_conf,
  install_dir: systemd_user_unit_dir,
)
