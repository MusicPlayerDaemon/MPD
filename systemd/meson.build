systemd_unit_conf = configuration_data()
systemd_unit_conf.set('prefix', get_option('prefix'))

systemd_socket_conf = configuration_data()
listen_streams = []
if get_option('local_socket')
  listen_streams += 'ListenStream=%t/mpd/socket'
endif
if get_option('tcp')
  listen_streams += 'ListenStream=6600'
endif
systemd_socket_conf.set('listen_streams', '\n'.join(listen_streams))

subdir('system')
subdir('user')
