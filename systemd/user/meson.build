systemd_user_unit_dir = get_option('systemd_user_unit_dir')
if systemd_user_unit_dir == ''
  systemd = dependency('systemd', required: false)
  if systemd.found()
    systemd_user_unit_dir = systemd.get_variable(
      pkgconfig: 'systemduserunitdir',
      pkgconfig_define: ['prefix', get_option('prefix')],
    )
  endif
endif
if systemd_user_unit_dir == ''
  systemd_user_unit_dir = join_paths(get_option('prefix'), 'lib', 'systemd', 'user')
endif

configure_file(
  input: join_paths('..', 'system', 'mpd.socket.in'),
  output: 'mpd.socket',
  configuration: systemd_socket_conf,
  install_dir: systemd_user_unit_dir,
)

configure_file(
  input: 'mpd.service.in',
  output: 'mpd.service',
  configuration: systemd_unit_conf,
  install_dir: systemd_user_unit_dir,
)
