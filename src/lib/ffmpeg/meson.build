libavformat_dep = dependency('libavformat', version: '>= 56.1', required: get_option('ffmpeg'))
libavcodec_dep = dependency('libavcodec', version: '>= 56.1', required: get_option('ffmpeg'))
libavutil_dep = dependency('libavutil', version: '>= 54.3', required: get_option('ffmpeg'))

enable_ffmpeg = libavformat_dep.found() and libavcodec_dep.found() and libavutil_dep.found()
conf.set('ENABLE_FFMPEG', enable_ffmpeg)
if not enable_ffmpeg
  ffmpeg_dep = dependency('', required: false)
  subdir_done()
endif

ffmpeg = static_library(
  'ffmpeg',
  'Init.cxx',
  'LogError.cxx',
  'LogCallback.cxx',
  'Error.cxx',
  'Domain.cxx',
  include_directories: inc,
  dependencies: [
    libavformat_dep,
    libavcodec_dep,
    libavutil_dep,
  ],
)

ffmpeg_dep = declare_dependency(
  link_with: ffmpeg,
  dependencies: [
    libavformat_dep,
    libavcodec_dep,
    libavutil_dep,
  ],
)
