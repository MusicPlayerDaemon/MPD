libavahi_client = dependency('avahi-client', required: get_option('zeroconf') == 'avahi')
if not libavahi_client.found()
  avahi_dep = dependency('', required: false)
  subdir_done()
endif

avahi = static_library(
  'avahi',
  'Helper.cxx',
  'Client.cxx',
  'Error.cxx',
  'Poll.cxx',
  'Publisher.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
    libavahi_client,
  ],
)

avahi_dep = declare_dependency(
  link_with: avahi,
  dependencies: [
    event_dep,
  ],
)
