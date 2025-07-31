if not is_windows
  system_windows_dep = dependency('', required: false)
  subdir_done()
endif

system_windows = static_library(
  'system_windows',
  'Event.cxx',
  include_directories: inc,
  dependencies: [
    system_dep,
  ],
)

system_windows_dep = declare_dependency(
  link_with: system_windows,
)
