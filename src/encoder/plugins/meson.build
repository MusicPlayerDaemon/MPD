encoder_plugins_sources = [
  'NullEncoderPlugin.cxx',
]

conf.set('ENABLE_FLAC_ENCODER', flac_dep.found())
if flac_dep.found()
  encoder_plugins_sources += 'FlacEncoderPlugin.cxx'
endif

if libopus_dep.found()
  encoder_plugins_sources += 'OpusEncoderPlugin.cxx'
endif

conf.set('ENABLE_VORBISENC', libvorbisenc_dep.found())
if libvorbisenc_dep.found()
  encoder_plugins_sources += 'VorbisEncoderPlugin.cxx'
endif

liblame_dep = c_compiler.find_library('mp3lame', required: get_option('lame'))
conf.set('ENABLE_LAME', liblame_dep.found())
if liblame_dep.found()
  encoder_plugins_sources += 'LameEncoderPlugin.cxx'
endif

libtwolame_dep = dependency('twolame', required: get_option('twolame'))
conf.set('ENABLE_TWOLAME', libtwolame_dep.found())
if libtwolame_dep.found()
  encoder_plugins_sources += 'TwolameEncoderPlugin.cxx'
endif

libshine_dep = dependency('shine', version: '>= 3.1', required: get_option('shine'))
conf.set('ENABLE_SHINE', libshine_dep.found())
if libshine_dep.found()
  encoder_plugins_sources += 'ShineEncoderPlugin.cxx'
endif

conf.set('ENABLE_WAVE_ENCODER', get_option('wave_encoder'))
if get_option('wave_encoder')
  encoder_plugins_sources += 'WaveEncoderPlugin.cxx'
endif

encoder_plugins = static_library(
  'encoder_plugins',
  encoder_plugins_sources,
  include_directories: inc,
  dependencies: [
    flac_dep,
    ogg_dep,
    libopus_dep,
    libvorbisenc_dep,
    libvorbis_dep,
    liblame_dep,
    libtwolame_dep,
    libshine_dep,
  ],
)

encoder_plugins_dep = declare_dependency(
  link_with: encoder_plugins,
  dependencies: [
    encoder_api_dep,
    tag_dep,
    pcm_dep,
    config_dep,
  ],
)
