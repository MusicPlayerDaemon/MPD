# This build rule lives in a subdirectory (and refers to the sources
# with "../") to work around a library dependency libio_fs.a ->
# libfs.a -> libio.a and therefore the build rules of libio.a and
# libio_fs.a cannot be in the same directory.

io_fs = static_library(
  'io_fs',
  '../FileReader.cxx',
  '../FileOutputStream.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    io_dep,
    fs_dep,
  ],
)

io_fs_dep = declare_dependency(
  link_with: io_fs,
  dependencies: [
    fs_dep,
  ],
)
