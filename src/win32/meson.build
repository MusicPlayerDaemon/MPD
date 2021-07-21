if not is_windows
  win32_dep = dependency('', required: false)
  subdir_done()
endif

win32 = static_library(
  'win32',
  'ComWorker.cxx',
  'HResult.cxx',
  'PropVariant.cxx',
  'WinEvent.cxx',
  include_directories: inc,
)

win32_dep = declare_dependency(
  link_with: win32,
  dependencies: [
    thread_dep,
  ],
)
