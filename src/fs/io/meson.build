fs_io = static_library(
  'fs_io',
  'TextFile.cxx',
  include_directories: inc,
  dependencies: [
    fs_dep,
    io_fs_dep,
    system_dep,
    zlib_dep,
  ],
)

fs_io_dep = declare_dependency(
  link_with: fs_io,
  dependencies: [
    io_dep,
  ],
)
