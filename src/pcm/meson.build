pcm_sources = [
  '../CheckAudioFormat.cxx',
  '../AudioFormat.cxx',
  '../AudioParser.cxx',
  'SampleFormat.cxx',
  'Interleave.cxx',
  'Buffer.cxx',
  'Export.cxx',
  'Convert.cxx',
  'Dop.cxx',
  'Volume.cxx',
  'Silence.cxx',
  'Mix.cxx',
  'PcmChannels.cxx',
  'Pack.cxx',
  'PcmFormat.cxx',
  'FormatConverter.cxx',
  'ChannelsConverter.cxx',
  'Order.cxx',
  'GlueResampler.cxx',
  'FallbackResampler.cxx',
  'ConfiguredResampler.cxx',
  'Dither.cxx',
]

if get_option('dsd')
  pcm_sources += [
    'Dsd16.cxx',
    'Dsd32.cxx',
    'PcmDsd.cxx',
    'Dsd2Pcm.cxx',
  ]
endif

libsamplerate_dep = dependency('samplerate', version: '>= 0.1.3', required: get_option('libsamplerate'))
if libsamplerate_dep.found()
  conf.set('ENABLE_LIBSAMPLERATE', true)
  pcm_sources += 'LibsamplerateResampler.cxx'
endif

soxr_dep = dependency('soxr', required: get_option('soxr'))
if soxr_dep.found()
  conf.set('ENABLE_SOXR', true)
  pcm_sources += 'SoxrResampler.cxx'
endif

pcm = static_library(
  'pcm',
  pcm_sources,
  include_directories: inc,
  dependencies: [
    util_dep,
    libsamplerate_dep,
    soxr_dep,
  ],
)

pcm_dep = declare_dependency(
  link_with: pcm,
)
