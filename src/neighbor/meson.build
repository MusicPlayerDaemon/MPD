if not get_option('neighbor') or not enable_database
  conf.set('ENABLE_NEIGHBOR_PLUGINS', false)
  neighbor_glue_dep = dependency('', required: false)
  subdir_done()
endif

neighbor_api_dep = declare_dependency()

subdir('plugins')

conf.set('ENABLE_NEIGHBOR_PLUGINS', found_neighbor_plugin)
if not found_neighbor_plugin
  neighbor_glue_dep = dependency('', required: false)
  subdir_done()
endif

neighbor_glue = static_library(
  'neighbor_glue',
  'Glue.cxx',
  'Registry.cxx',
  include_directories: inc,
)

neighbor_glue_dep = declare_dependency(
  link_with: neighbor_glue,
  dependencies: [
    neighbor_plugins_dep,
    config_dep,
  ],
)

