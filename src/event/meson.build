event_sources = []

if uring_dep.found()
  event_sources += 'UringManager.cxx'
endif

if is_windows
  event_sources += 'PollGroupWinSelect.cxx'
elif is_linux and get_option('epoll')
  # epoll support is header-only
else
  event_sources += 'PollGroupPoll.cxx'
endif

event = static_library(
  'event',
  'SignalMonitor.cxx',
  'TimerEvent.cxx',
  'IdleMonitor.cxx',
  'DeferEvent.cxx',
  'MaskMonitor.cxx',
  'SocketMonitor.cxx',
  'BufferedSocket.cxx',
  'FullyBufferedSocket.cxx',
  'MultiSocketMonitor.cxx',
  'ServerSocket.cxx',
  'Call.cxx',
  'Thread.cxx',
  'Loop.cxx',
  event_sources,
  include_directories: inc,
  dependencies: [
    boost_dep,
    log_dep,
    uring_dep,
  ],
)

event_dep = declare_dependency(
  link_with: event,
  dependencies: [
    thread_dep,
    net_dep,
    system_dep,
    boost_dep,
    uring_dep,
  ],
)
