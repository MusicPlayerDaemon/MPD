smbclient_dep = dependency('smbclient', version: '>= 0.2', required: get_option('smbclient'))
conf.set('ENABLE_SMBCLIENT', smbclient_dep.found())
if not smbclient_dep.found()
  subdir_done()
endif

smbclient = static_library(
  'smbclient',
  'Domain.cxx',
  'Init.cxx',
  'Context.cxx',
  include_directories: inc,
  dependencies: [
    smbclient_dep,
  ],
)

smbclient_dep = declare_dependency(
  link_with: smbclient,
  dependencies: [
    smbclient_dep,
  ],
)
