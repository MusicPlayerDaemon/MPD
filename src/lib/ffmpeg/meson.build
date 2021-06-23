libavformat_dep = dependency('libavformat', version: '>= 57.40', required: get_option('ffmpeg'))
libavcodec_dep = dependency('libavcodec', version: '>= 57.48', required: get_option('ffmpeg'))
libavutil_dep = dependency('libavutil', version: '>= 55.27', required: get_option('ffmpeg'))
conf.set('HAVE_LIBAVUTIL', libavutil_dep.found())

enable_ffmpeg = libavformat_dep.found() and libavcodec_dep.found() and libavutil_dep.found()
conf.set('ENABLE_FFMPEG', enable_ffmpeg)

if enable_ffmpeg
  libavfilter_dep = dependency('libavfilter', required: false)
else
  libavfilter_dep = dependency('', required: false)
endif
conf.set('HAVE_LIBAVFILTER', libavfilter_dep.found())

if not enable_ffmpeg
  ffmpeg_dep = dependency('', required: false)
  subdir_done()
endif

ffmpeg_sources = []
if libavfilter_dep.found()
  ffmpeg_sources += [
    'Filter.cxx',
    'DetectFilterFormat.cxx',
  ]
endif

ffmpeg = static_library(
  'ffmpeg',
  'Init.cxx',
  'Interleave.cxx',
  'LogError.cxx',
  'LogCallback.cxx',
  'Error.cxx',
  'Domain.cxx',
  ffmpeg_sources,
  include_directories: inc,
  dependencies: [
    libavformat_dep,
    libavcodec_dep,
    libavfilter_dep,
    libavutil_dep,
    log_dep,
  ],
)

ffmpeg_dep = declare_dependency(
  link_with: ffmpeg,
  dependencies: [
    libavformat_dep,
    libavcodec_dep,
    libavfilter_dep,
    libavutil_dep,
  ],
)
