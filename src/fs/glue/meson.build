fs_glue = static_library(
  'fs_glue',
  'StandardDirectory.cxx',
  'CheckFile.cxx',
  include_directories: inc,
  dependencies: [
    fs_dep,
    fs_io_dep,
    fmt_dep,
    log_dep,
    util_dep,
  ],
)

fs_glue_dep = declare_dependency(
  link_with: fs_glue,
)
