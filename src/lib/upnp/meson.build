upnp_option = get_option('upnp')

if upnp_option == 'auto'
  if not curl_dep.found()
    warning('No UPnP because CURL is not enabled')
    upnp_option = 'disabled'
  elif not expat_dep.found()
    warning('No UPnP because expat is not enabled')
    upnp_option = 'disabled'
  endif
endif

if upnp_option == 'auto'
  upnp_dep = dependency('libupnp', version: '>= 1.8', required: false)
  conf.set('USING_PUPNP', upnp_dep.found())
  if not upnp_dep.found()
    upnp_dep = dependency('libnpupnp', version: '>= 1.8', required: false)
  endif
elif upnp_option == 'pupnp'
  upnp_dep = dependency('libupnp', version: '>= 1.8', required: true)
  conf.set('USING_PUPNP', true)
elif upnp_option == 'npupnp'
  upnp_dep = dependency('libnpupnp', required: true)
  conf.set('USING_PUPNP', false)
elif upnp_option == 'disabled'
  upnp_dep = dependency('', required: false)
  subdir_done()
endif

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
    log_dep,
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
