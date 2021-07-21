decoder_features = configuration_data()

decoder_api = static_library(
  'decoder_api',
  'DecoderAPI.cxx',
  'Domain.cxx',
  'Reader.cxx',
  'DecoderBuffer.cxx',
  'DecoderPlugin.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
    pcm_basic_dep,
  ],
)

decoder_api_dep = declare_dependency(
  link_with: decoder_api,
  dependencies: [
    tag_dep,
    config_dep,
    input_api_dep,
  ],
)

subdir('plugins')

decoder_glue = static_library(
  'decoder_glue',
  'DecoderList.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
  ],
)

decoder_glue_dep = declare_dependency(
  link_with: decoder_glue,
  dependencies: [
    decoder_plugins_dep,
  ],
)

configure_file(output: 'Features.h', configuration: decoder_features)
