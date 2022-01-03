time = static_library(
  'time',
  'Parser.cxx',
  'Convert.cxx',
  'ISO8601.cxx',
  'Math.cxx',
  'Zone.cxx',
  include_directories: inc,
)

time_dep = declare_dependency(
  link_with: time,
)
