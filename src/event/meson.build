event = static_library(
  'event',
  'PollGroupPoll.cxx',
  'PollGroupWinSelect.cxx',
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
  include_directories: inc,
  dependencies: [
    boost_dep,
  ],
)

event_dep = declare_dependency(
  link_with: event,
  dependencies: [
    thread_dep,
    system_dep,
    boost_dep,
  ],
)
