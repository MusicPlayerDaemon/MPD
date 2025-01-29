nfs_dep = dependency('libnfs', version: ['>= 4', '< 6'], required: get_option('nfs'))
conf.set('ENABLE_NFS', nfs_dep.found())
if not nfs_dep.found()
  subdir_done()
endif

nfs = static_library(
  'nfs',
  'Connection.cxx',
  'Error.cxx',
  'Manager.cxx',
  'Glue.cxx',
  'Base.cxx',
  'FileReader.cxx',
  'Blocking.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
    nfs_dep,
    log_dep,
  ],
)

nfs_dep = declare_dependency(
  link_with: nfs,
  dependencies: [
    event_dep,
    nfs_dep,
  ],
)
