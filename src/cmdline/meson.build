cmdline = static_library(
  'cmdline',
  'OptionParser.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

cmdline_dep = declare_dependency(
  link_with: cmdline,
)
