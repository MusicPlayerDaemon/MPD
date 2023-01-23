event_features = configuration_data()
event_features.set('USE_EVENTFD', is_linux and get_option('eventfd'))
event_features.set('USE_SIGNALFD', is_linux and get_option('signalfd'))
event_features.set('USE_EPOLL', is_linux and get_option('epoll'))
event_features.set('HAVE_THREADED_EVENT_LOOP', true)
event_features.set('NO_BOOST', true)
configure_file(output: 'Features.h', configuration: event_features)

event_sources = []

if uring_dep.found()
  event_sources += 'UringManager.cxx'
endif

if is_windows
  event_sources += 'WinSelectBackend.cxx'
elif is_linux and get_option('epoll')
  # epoll support is header-only
else
  event_sources += 'PollBackend.cxx'
endif

if enable_inotify
  event_sources += 'InotifyEvent.cxx'
endif

event = static_library(
  'event',
  'SignalMonitor.cxx',
  'TimerWheel.cxx',
  'TimerList.cxx',
  'CoarseTimerEvent.cxx',
  'FineTimerEvent.cxx',
  'IdleEvent.cxx',
  'InjectEvent.cxx',
  'DeferEvent.cxx',
  'MaskMonitor.cxx',
  'SocketEvent.cxx',
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
    uring_dep,
  ],
)
