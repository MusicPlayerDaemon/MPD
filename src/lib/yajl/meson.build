yajl_dep = dependency('yajl', required: get_option('yajl'))
if not yajl_dep.found()
  subdir_done()
endif

yajl = static_library(
  'yajl',
  'Handle.cxx',
  'ResponseParser.cxx',
  'ParseInputStream.cxx',
  include_directories: inc,
  dependencies: [
    yajl_dep,
  ],
)

yajl_dep = declare_dependency(
  link_with: yajl,
  dependencies: [
    yajl_dep,
  ],
)
