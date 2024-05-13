nfs_dep = dependency('libnfs', version: '>= 4', required: get_option('nfs'))
conf.set('ENABLE_NFS', nfs_dep.found())
if not nfs_dep.found()
  subdir_done()
endif

if nfs_dep.version().version_compare('>=6')
  # libnfs has no version macro therefore we must detect the API
  # version 2 at configure time
  nfs_dep = declare_dependency(compile_args: '-DLIBNFS_API_2',
                               dependencies: nfs_dep)
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
    nfs_dep,
    log_dep,
  ],
)

nfs_dep = declare_dependency(
  link_with: nfs,
  dependencies: [
    nfs_dep,
  ],
)
