zeroconf_option = get_option('zeroconf')

libavahi_client_dep = dependency('', required: false)

if zeroconf_option == 'auto'
  if is_darwin
    # Bonjour disabled for now because its build is broken
    #zeroconf_option = 'bonjour'
    zeroconf_option = 'disabled'
  elif is_android or is_windows
    zeroconf_option = 'disabled'
  elif dbus_dep.found()
    libavahi_client_dep = dependency('avahi-client', required: false)
    if libavahi_client_dep.found()
      zeroconf_option = 'avahi'
    else
      zeroconf_option = 'disabled'
    endif
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

  if is_darwin
    bonjour_dep = declare_dependency(link_args: ['-framework', 'dnssd'])
  else
    bonjour_dep = declare_dependency(link_args: ['-ldns_sd'])
  endif
  conf.set('HAVE_BONJOUR', true)

  zeroconf = static_library(
    'zeroconf_bonjour',
    'ZeroconfGlue.cxx',
    'ZeroconfBonjour.cxx',
    include_directories: inc,
  )

  zeroconf_dep = declare_dependency(
    link_with: zeroconf,
    dependencies: [
      bonjour_dep,
    ],
  )
else
  if not libavahi_client_dep.found()
    libavahi_client_dep = dependency('avahi-client')
  endif

  conf.set('HAVE_AVAHI', true)
  
  zeroconf = static_library(
    'zeroconf_bonjour',
    'ZeroconfGlue.cxx',
    'ZeroconfAvahi.cxx',
    'AvahiPoll.cxx',
    include_directories: inc,
    dependencies: [
      libavahi_client_dep,
      dbus_dep,
      time_dep,
    ],
  )

  zeroconf_dep = declare_dependency(
    link_with: zeroconf,
  )
endif

conf.set('HAVE_ZEROCONF', true)
