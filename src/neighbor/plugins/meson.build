neighbor_plugins_sources = []
found_neighbor_plugin = false

if smbclient_dep.found()
  neighbor_plugins_sources += 'SmbclientNeighborPlugin.cxx'
  found_neighbor_plugin = true
endif

if enable_udisks
  neighbor_plugins_sources += 'UdisksNeighborPlugin.cxx'
  found_neighbor_plugin = true
endif

if upnp_dep.found()
  neighbor_plugins_sources += 'UpnpNeighborPlugin.cxx'
  found_neighbor_plugin = true
endif

if not found_neighbor_plugin
  subdir_done()
endif

neighbor_plugins = static_library(
  'neighbor_plugins',
  neighbor_plugins_sources,
  include_directories: inc,
  dependencies: [
    log_dep,
    dbus_dep,
    smbclient_dep,
    upnp_dep,
  ],
)

neighbor_plugins_dep = declare_dependency(
  link_with: neighbor_plugins,
  dependencies: [
    neighbor_api_dep,
    event_dep,
  ],
)
