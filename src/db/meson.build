db_api = static_library(
  'db_api',
  'DatabaseLock.cxx',
  'Selection.cxx',
  include_directories: inc,
)

db_api_dep = declare_dependency(
  link_with: db_api,
)

if not enable_database
  db_glue_dep = db_api_dep
  subdir_done()
endif

subdir('plugins')

db_glue_sources = [
  'Count.cxx',
  'update/UpdateDomain.cxx',
  'update/Config.cxx',
  'update/Service.cxx',
  'update/Queue.cxx',
  'update/UpdateIO.cxx',
  'update/Editor.cxx',
  'update/Walk.cxx',
  'update/UpdateSong.cxx',
  'update/Container.cxx',
  'update/Playlist.cxx',
  'update/Remove.cxx',
  'update/ExcludeList.cxx',
  'update/VirtualDirectory.cxx',
  'update/SpecialDirectory.cxx',
  'DatabaseGlue.cxx',
  'Configured.cxx',
  'DatabaseSong.cxx',
  'DatabasePrint.cxx',
  'DatabaseQueue.cxx',
  'DatabasePlaylist.cxx',
]

if enable_inotify
  db_glue_sources += [
    'update/InotifyDomain.cxx',
    'update/InotifySource.cxx',
    'update/InotifyQueue.cxx',
    'update/InotifyUpdate.cxx',
  ]
endif

db_glue = static_library(
  'db_glue',
  db_glue_sources,
  include_directories: inc,
  dependencies: [
    boost_dep,
    fmt_dep,
    log_dep,
  ],
)

db_glue_dep = declare_dependency(
  link_with: db_glue,
  dependencies: [
    db_plugins_dep,
  ],
)
