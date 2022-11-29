mixer_api = static_library(
  'mixer_api',
  'Mixer.cxx',
  'Control.cxx',
  include_directories: inc,
  dependencies: [
    thread_dep,
  ],
)

mixer_api_dep = declare_dependency(
  link_with: mixer_api,
  dependencies: [
    thread_dep,
  ],
)

subdir('plugins')

mixer_glue = static_library(
  'mixer_glue',
  'Type.cxx',
  'All.cxx',
  include_directories: inc,
  dependencies: [
    mixer_api_dep,
    log_dep,
  ],
)

mixer_glue_dep = declare_dependency(
  link_with: mixer_glue,
  dependencies: [
    mixer_plugins_dep,
  ],
)
