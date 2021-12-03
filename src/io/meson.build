io = static_library(
  'io',
  'FileDescriptor.cxx',
  'Open.cxx',
  'PeekReader.cxx',
  'FileReader.cxx',
  'BufferedReader.cxx',
  'FileOutputStream.cxx',
  'BufferedOutputStream.cxx',
  include_directories: inc,
)

io_dep = declare_dependency(
  link_with: io,
)
