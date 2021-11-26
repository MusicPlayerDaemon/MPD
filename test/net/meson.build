test_net_sources = []

if have_tcp
  test_net_sources += 'TestIPv4Address.cxx'
  if have_ipv6
    test_net_sources += 'TestIPv6Address.cxx'
  endif
endif

if have_local_socket
  test_net_sources += 'TestLocalSocketAddress.cxx'
endif

test(
  'TestNet',
  executable(
    'TestNet',
    test_net_sources,
    include_directories: inc,
    dependencies: [
      net_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)
