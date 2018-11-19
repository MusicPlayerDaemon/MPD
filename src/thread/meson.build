threads_dep = dependency('threads')

conf.set('HAVE_PTHREAD_SETNAME_NP', compiler.has_function('pthread_setname_np', dependencies: threads_dep))

thread = static_library(
  'thread',
  'Util.cxx',
  'Thread.cxx',
  include_directories: inc,
  dependencies: [
    threads_dep,
  ],
)

thread_dep = declare_dependency(
  link_with: thread,
)
