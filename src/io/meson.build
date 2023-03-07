io = static_library(
  'io',
  'FileDescriptor.cxx',
  'Open.cxx',
  'PeekReader.cxx',
  'BufferedReader.cxx',
  'BufferedOutputStream.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

io_dep = declare_dependency(
  link_with: io,
  dependencies: [
  ],
)

io_fs = static_library(
  'io_fs',
  'FileReader.cxx',
  'FileOutputStream.cxx',
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
    fmt_dep,
    io_dep,
    fs_dep,
  ],
)
