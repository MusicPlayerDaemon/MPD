zlib_dep = dependency('zlib', required: get_option('zlib'))
conf.set('ENABLE_ZLIB', zlib_dep.found())
if not zlib_dep.found()
  subdir_done()
endif

zlib = static_library(
  'zlib',
  'Error.cxx',
  'GunzipReader.cxx',
  'GzipOutputStream.cxx',
  'AutoGunzipReader.cxx',
  include_directories: inc,
  dependencies: [
    zlib_dep,
  ],
)

zlib_dep = declare_dependency(
  link_with: zlib,
  dependencies: [
    zlib_dep,
    io_dep,
  ],
)
