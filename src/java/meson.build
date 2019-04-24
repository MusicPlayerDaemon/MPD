java = static_library(
  'java',
  'Global.cxx',
  'Exception.cxx',
  'File.cxx',
  'Object.cxx',
  'String.cxx',
  include_directories: inc,
  dependencies: [
  ],
)

java_dep = declare_dependency(
  link_with: java,
  dependencies: [
    util_dep,
    fs_dep,
  ],
)
