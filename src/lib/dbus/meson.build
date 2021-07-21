dbus_dep = dependency('dbus-1', required: get_option('dbus'))
conf.set('ENABLE_DBUS', dbus_dep.found())
if not dbus_dep.found()
  if get_option('udisks').enabled()
    error('udisks2 requires D-Bus')
  endif
  enable_udisks = false
  conf.set('ENABLE_UDISKS', enable_udisks)

  subdir_done()
endif

enable_udisks = not get_option('udisks').disabled()
conf.set('ENABLE_UDISKS', enable_udisks)

dbus = static_library(
  'dbus',
  'Connection.cxx',
  'Error.cxx',
  'FilterHelper.cxx',
  'Message.cxx',
  'UDisks2.cxx',
  'ScopeMatch.cxx',
  'Glue.cxx',
  'Watch.cxx',
  include_directories: inc,
  dependencies: [
    dbus_dep,
  ],
)

dbus_dep = declare_dependency(
  link_with: dbus,
  dependencies: [
    dbus_dep,
    event_dep,
  ],
)
