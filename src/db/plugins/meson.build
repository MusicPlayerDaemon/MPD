db_plugins_sources = [
  '../../PlaylistDatabase.cxx',
  '../Registry.cxx',
  '../Helpers.cxx',
  '../VHelper.cxx',
  '../UniqueTags.cxx',
  'simple/DatabaseSave.cxx',
  'simple/DirectorySave.cxx',
  'simple/Directory.cxx',
  'simple/Song.cxx',
  'simple/SongSort.cxx',
  'simple/Mount.cxx',
  'simple/SimpleDatabasePlugin.cxx',
]

if upnp_dep.found()
  db_plugins_sources += [
    'upnp/UpnpDatabasePlugin.cxx',
    'upnp/Tags.cxx',
    'upnp/ContentDirectoryService.cxx',
    'upnp/Directory.cxx',
    'upnp/Object.cxx',
  ]
endif

libmpdclient_dep = dependency('libmpdclient', version: '>= 2.11', required: get_option('libmpdclient'))
conf.set('ENABLE_LIBMPDCLIENT', libmpdclient_dep.found())
if libmpdclient_dep.found()
  db_plugins_sources += 'ProxyDatabasePlugin.cxx'
endif

db_plugins = static_library(
  'db_plugins',
  db_plugins_sources,
  include_directories: inc,
  dependencies: [
    upnp_dep,
    pcre_dep,
    libmpdclient_dep,
    log_dep,
  ],
)

db_plugins_dep = declare_dependency(
  link_with: db_plugins,
  dependencies: [
    db_api_dep,
    storage_api_dep,
    config_dep,
  ],
)
