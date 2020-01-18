config = static_library(
  'fs',
  'Path.cxx',
  'Check.cxx',
  'Data.cxx',
  'Block.cxx',
  'Param.cxx',
  'Parser.cxx',
  'File.cxx',
  'Migrate.cxx',
  'Templates.cxx',
  'Domain.cxx',
  'Net.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
  ],
)

config_dep = declare_dependency(
  link_with: config,
  dependencies: [
    fs_dep,
  ],
)
