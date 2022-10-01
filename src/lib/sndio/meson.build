libsndio_dep = dependency('sndio', required: get_option('sndio'))
conf.set('ENABLE_SNDIO', libsndio_dep.found())
if not libsndio_dep.found()
  subdir_done()
endif

sndio = static_library(
  'sndio',
  include_directories: inc,
  dependencies: [
    libsndio_dep,
  ],
)

libsndio_dep = declare_dependency(
  link_with: sndio,
  dependencies: libsndio_dep,
)
