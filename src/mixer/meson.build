mixer_api_dep = declare_dependency()

subdir('plugins')

mixer_glue = static_library(
  'mixer_glue',
  'MixerControl.cxx',
  'MixerType.cxx',
  'MixerAll.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
  ],
)

mixer_glue_dep = declare_dependency(
  link_with: mixer_glue,
  dependencies: [
    mixer_plugins_dep,
  ],
)
