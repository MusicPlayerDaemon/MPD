encoder_features = configuration_data()

encoder_features.set('ENABLE_ENCODER', need_encoder)

if not need_encoder
  if need_wave_encoder
    # Special case for the Snapcast output plugin which only needs the
    # PCM wave encoder encoder plugin
    encoder_glue = static_library(
      'encoder_glue',
      'plugins/WaveEncoderPlugin.cxx',
      include_directories: inc,
    )

    encoder_glue_dep = declare_dependency(
      link_with: encoder_glue,
    )

    configure_file(output: 'Features.h', configuration: encoder_features)
    subdir_done()
  endif

  encoder_glue_dep = dependency('', required: false)
  configure_file(output: 'Features.h', configuration: encoder_features)
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
  dependencies: [
    fmt_dep,
  ],
)

encoder_glue_dep = declare_dependency(
  link_with: encoder_glue,
  dependencies: [
    encoder_plugins_dep,
  ],
)

configure_file(output: 'Features.h', configuration: encoder_features)
