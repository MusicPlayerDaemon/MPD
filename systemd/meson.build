systemd_unit_conf = configuration_data()
systemd_unit_conf.set('prefix', get_option('prefix'))

address_families = ['AF_INET', 'AF_INET6', 'AF_UNIX']
if get_option('smbclient').enabled()
  # AF_NETLINK is required by libsmbclient, or it will exit() .. *sigh*
  address_families += 'AF_NETLINK'
endif
systemd_unit_conf.set('address_families', ' '.join(address_families))

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
