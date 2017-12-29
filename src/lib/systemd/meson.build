if not is_linux or is_android
  systemd_dep = dependency('', required: false)
  subdir_done()
endif

systemd_dep = dependency('libsystemd', required: get_option('systemd'))
conf.set('ENABLE_SYSTEMD_DAEMON', systemd_dep.found())
if not systemd_dep.found()
  subdir_done()
endif

systemd = static_library(
  'systemd',
  'Watchdog.cxx',
  include_directories: inc,
  dependencies: [
    systemd_dep,
  ],
)

systemd_dep = declare_dependency(
  link_with: systemd,
)
