zeroconf_option = get_option('zeroconf')

avahi_dep = dependency('', required: false)

if zeroconf_option == 'auto'
  if is_darwin
    zeroconf_option = 'bonjour'
  elif is_android or is_windows
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

if zeroconf_option == 'bonjour'
  if not compiler.has_header('dns_sd.h')
    error('dns_sd.h not found')
  endif

  bonjour_deps = [
  ]

  if not is_darwin
    bonjour_deps += declare_dependency(link_args: ['-ldns_sd'])
  endif
  conf.set('HAVE_BONJOUR', true)

  zeroconf = static_library(
    'zeroconf_bonjour',
    'Glue.cxx',
    'Helper.cxx',
    'Bonjour.cxx',
    include_directories: inc,
    dependencies: [
      event_dep,
      log_dep,
    ],
  )

  zeroconf_dep = declare_dependency(
    link_with: zeroconf,
    dependencies: bonjour_deps,
  )
else
  subdir('avahi')

  if not avahi_dep.found()
    zeroconf_dep = dependency('', required: false)
    subdir_done()
  endif

  conf.set('HAVE_AVAHI', true)
  
  zeroconf = static_library(
    'zeroconf_bonjour',
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
endif

conf.set('HAVE_ZEROCONF', true)
