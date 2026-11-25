systemd = dependency('systemd', required: false)
if systemd.found()
  systemd_sysusers_dir = systemd.get_variable(
    pkgconfig: 'sysusersdir',
    pkgconfig_define: ['rootprefix', get_option('prefix')],
    default_value: '',
  )
else
  systemd_sysusers_dir = ''
endif
if systemd_sysusers_dir == ''
  systemd_sysusers_dir = join_paths(get_option('prefix'), 'lib', 'sysusers.d')
endif

install_data(
  'mpd.conf',
  install_dir: systemd_sysusers_dir,
)
