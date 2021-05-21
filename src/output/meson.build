output_features = configuration_data()

output_api = static_library(
  'output_api',
  'Interface.cxx',
  'Timer.cxx',
  include_directories: inc,
)

output_api_dep = declare_dependency(
  link_with: output_api,
  dependencies: [
    filter_plugins_dep,
    mixer_plugins_dep,
  ],
)

subdir('plugins')

output_glue = static_library(
  'output_glue',
  'Defaults.cxx',
  'Filtered.cxx',
  'Registry.cxx',
  'MultipleOutputs.cxx',
  'SharedPipeConsumer.cxx',
  'Source.cxx',
  'Thread.cxx',
  'Domain.cxx',
  'Control.cxx',
  'State.cxx',
  'Print.cxx',
  'OutputCommand.cxx',
  'OutputPlugin.cxx',
  'Finish.cxx',
  'Init.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

output_glue_dep = declare_dependency(
  link_with: output_glue,
  dependencies: [
    filter_glue_dep,
    mixer_plugins_dep,
    output_plugins_dep,
  ],
)


configure_file(output: 'Features.h', configuration: output_features)
