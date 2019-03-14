if is_windows
  # avoid the unused libwinpthread-1.dll dependency on Windows; MPD
  # doesn't use the pthread API on Windows, but this is what Meson
  # unhelpfully detects for us
  threads_dep = []
else
  threads_dep = dependency('threads')
endif

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
