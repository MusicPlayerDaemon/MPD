zeroconf_option = get_option('zeroconf')

avahi_dep = dependency('', required: false)

if zeroconf_option == 'auto'
  if is_android or is_windows
    zeroconf_option = 'disabled'
  elif dbus_dep.found()
    zeroconf_option = 'avahi'
  else
    zeroconf_option = 'disabled'
  endif
endif

if zeroconf_option == 'disabled'
  zeroconf_dep = dependency('', required: false)
  subdir_done()
endif

subdir('avahi')

if not avahi_dep.found()
  zeroconf_dep = dependency('', required: false)
  subdir_done()
endif

conf.set('HAVE_AVAHI', true)

zeroconf = static_library(
  'zeroconf',
  'Glue.cxx',
  'Helper.cxx',
  include_directories: inc,
  dependencies: [
    avahi_dep,
    dbus_dep,
    time_dep,
    log_dep,
  ],
)

zeroconf_dep = declare_dependency(
  link_with: zeroconf,
)

conf.set('HAVE_ZEROCONF', true)
