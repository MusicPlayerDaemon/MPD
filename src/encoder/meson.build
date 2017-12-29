conf.set('ENABLE_ENCODER', need_encoder)

if not need_encoder
  encoder_glue_dep = dependency('', required: false)
  subdir_done()
endif

encoder_api_dep = declare_dependency()

subdir('plugins')

encoder_glue = static_library(
  'encoder_glue',
  'Configured.cxx',
  'ToOutputStream.cxx',
  'EncoderList.cxx',
  include_directories: inc,
)

encoder_glue_dep = declare_dependency(
  link_with: encoder_glue,
  dependencies: [
    encoder_plugins_dep,
  ],
)
