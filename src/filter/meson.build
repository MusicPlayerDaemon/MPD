filter_api = static_library(
  'filter_api',
  'Observer.cxx',
  'Filter.cxx',
  include_directories: inc,
)

filter_api_dep = declare_dependency(
  link_with: filter_api,
)

subdir('plugins')

filter_glue = static_library(
  'filter_glue',
  'Registry.cxx',
  'Factory.cxx',
  'LoadOne.cxx',
  'LoadChain.cxx',
  include_directories: inc,
)

filter_glue_dep = declare_dependency(
  link_with: filter_glue,
  dependencies: [
    filter_plugins_dep,
  ],
)
