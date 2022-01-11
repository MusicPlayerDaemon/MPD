net_sources = []

have_tcp = get_option('tcp')
conf.set('HAVE_TCP', have_tcp)

if have_tcp
  net_sources += 'DscpParser.cxx'
endif

if have_tcp and not get_option('ipv6').disabled()
  if is_windows
    have_ipv6 = c_compiler.has_header_symbol('winsock2.h', 'AF_INET6')
  else
    have_ipv6 = c_compiler.has_header_symbol('sys/socket.h', 'AF_INET6')
  endif
  if not have_ipv6 and get_option('ipv6').enabled()
    error('IPv6 not supported by OS')
  endif
else
  have_ipv6 = false
endif
conf.set('HAVE_IPV6', have_ipv6)

have_local_socket = not is_windows and get_option('local_socket')
conf.set('HAVE_UN', have_local_socket)

if have_local_socket
  conf.set('HAVE_STRUCT_UCRED', compiler.has_header_symbol('sys/socket.h', 'struct ucred') and compiler.has_header_symbol('sys/socket.h', 'SO_PEERCRED'))
  conf.set('HAVE_GETPEEREID', compiler.has_function('getpeereid'))
endif

if not have_tcp and not have_local_socket
  error('Must enable either "tcp" or "local_socket"')
endif

net = static_library(
  'net',
  net_sources,
  'ToString.cxx',
  'HostParser.cxx',
  'Resolver.cxx',
  'AddressInfo.cxx',
  'StaticSocketAddress.cxx',
  'AllocatedSocketAddress.cxx',
  'IPv4Address.cxx',
  'IPv6Address.cxx',
  'SocketAddress.cxx',
  'SocketUtil.cxx',
  'SocketDescriptor.cxx',
  'SocketError.cxx',
  include_directories: inc,
)

net_dep = declare_dependency(
  link_with: net,
  dependencies: [
    system_dep,
  ],
)
