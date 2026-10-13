protocol = static_library(
  'protocol',
  'Verify.cxx',
  'FixString.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
  ],
)

protocol_dep = declare_dependency(
  link_with: protocol,
)
