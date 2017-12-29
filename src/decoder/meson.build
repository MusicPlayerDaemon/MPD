decoder_api = static_library(
  'decoder_api',
  'DecoderAPI.cxx',
  'Reader.cxx',
  'DecoderBuffer.cxx',
  'DecoderPlugin.cxx',
  include_directories: inc,
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
)

decoder_glue_dep = declare_dependency(
  link_with: decoder_glue,
  dependencies: [
    decoder_plugins_dep,
  ],
)
