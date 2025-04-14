pcm_features = configuration_data()
pcm_features.set('ENABLE_DSD', get_option('dsd'))

pcm_basic_sources = [
  'CheckAudioFormat.cxx',
  'AudioFormat.cxx',
  'AudioParser.cxx',
  'SampleFormat.cxx',
  'Interleave.cxx',
  'Buffer.cxx',
  'Export.cxx',
  'Dop.cxx',
  'Volume.cxx',
  'Silence.cxx',
  'Mix.cxx',
  'Pack.cxx',
  'Order.cxx',
  'Dither.cxx',
]

if get_option('dsd')
  pcm_basic_sources += [
    'Dsd16.cxx',
    'Dsd32.cxx',
    'PcmDsd.cxx',
    'Dsd2Pcm.cxx',
  ]
endif

pcm_basic = static_library(
  'pcm_basic',
  pcm_basic_sources,
  include_directories: inc,
  dependencies: [
    util_dep,
    fmt_dep,
  ],
)

pcm_basic_dep = declare_dependency(
  link_with: pcm_basic,
)

pcm_sources = [
  'Convert.cxx',
  'PcmChannels.cxx',
  'PcmFormat.cxx',
  'FormatConverter.cxx',
  'ChannelsConverter.cxx',
  'GlueResampler.cxx',
  'FallbackResampler.cxx',
  'ConfiguredResampler.cxx',
  'Normalizer.cxx',
  'ReplayGainAnalyzer.cxx',
  'MixRampAnalyzer.cxx',
  'MixRampGlue.cxx',
]

libsamplerate_dep = dependency('samplerate', version: '>= 0.1.3', required: get_option('libsamplerate'))
pcm_features.set('ENABLE_LIBSAMPLERATE', libsamplerate_dep.found())
if libsamplerate_dep.found()
  pcm_sources += 'LibsamplerateResampler.cxx'
endif

soxr_dep = dependency('soxr', version: '>= 0.1.2', required: get_option('soxr'))
pcm_features.set('ENABLE_SOXR', soxr_dep.found())
if soxr_dep.found()
  pcm_sources += 'SoxrResampler.cxx'
endif

pcm = static_library(
  'pcm',
  pcm_sources,
  include_directories: inc,
  dependencies: [
    util_dep,
    pcm_basic_dep,
    libsamplerate_dep,
    soxr_dep,
    log_dep,
    config_dep,
  ],
)

pcm_dep = declare_dependency(
  link_with: pcm,
)

configure_file(output: 'Features.h', configuration: pcm_features)
