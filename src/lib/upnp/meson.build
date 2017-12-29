upnp_dep = dependency('libupnp', required: get_option('upnp'))
conf.set('ENABLE_UPNP', upnp_dep.found())
if not upnp_dep.found()
  subdir_done()
endif

if not curl_dep.found()
  error('UPnP requires CURL')
endif

if not expat_dep.found()
  error('UPnP requires expat')
endif

upnp = static_library(
  'upnp',
  'Init.cxx',
  'ClientInit.cxx',
  'Device.cxx',
  'ContentDirectoryService.cxx',
  'Discovery.cxx',
  'ixmlwrap.cxx',
  'Util.cxx',
  include_directories: inc,
  dependencies: [
    upnp_dep,
    curl_dep,
    expat_dep,
  ],
)

upnp_dep = declare_dependency(
  link_with: upnp,
  dependencies: [
    upnp_dep,
    curl_dep,
    expat_dep,
    event_dep,
  ],
)
