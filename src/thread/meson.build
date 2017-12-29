thread = static_library(
  'thread',
  'Util.cxx',
  'Thread.cxx',
  include_directories: inc,
  dependencies: [
    dependency('threads')
  ],
)

thread_dep = declare_dependency(
  link_with: thread,
)
