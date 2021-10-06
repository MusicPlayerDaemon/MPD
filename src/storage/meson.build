storage_api = static_library(
  'storage_api',
  'StorageInterface.cxx',
  'StoragePlugin.cxx',
  include_directories: inc,
)

storage_api_dep = declare_dependency(
  link_with: storage_api,
)

subdir('plugins')

storage_glue = static_library(
  'storage_glue',
  'Registry.cxx',
  'CompositeStorage.cxx',
  'MemoryDirectoryReader.cxx',
  'Configured.cxx',
  'StorageState.cxx',
  include_directories: inc,
  dependencies: [
    boost_dep,
    log_dep,
  ],
)

storage_glue_dep = declare_dependency(
  link_with: storage_glue,
  dependencies: [
    storage_plugins_dep,
  ],
)

