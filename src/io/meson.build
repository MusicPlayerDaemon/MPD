io = static_library(
  'io',
  'FileDescriptor.cxx',
  'Open.cxx',
  'Reader.cxx',
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
    fmt_dep,
  ],
)
