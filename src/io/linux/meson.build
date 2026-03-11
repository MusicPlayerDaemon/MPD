if not is_linux
  io_linux_dep = dependency('', required: false)
  subdir_done()
endif

io_linux = static_library(
  'io_linux',
  'ProcStatus.cxx',
  include_directories: inc,
  dependencies: [
    io_dep,
    util_dep,
  ],
)

io_linux_dep = declare_dependency(
  link_with: io_linux,
  dependencies: [
    fmt_dep,
    io_dep,
    util_dep,
  ],
)
