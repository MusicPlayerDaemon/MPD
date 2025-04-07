playlist_plugins_sources = [
  'ExtM3uPlaylistPlugin.cxx',
  'M3uPlaylistPlugin.cxx',
  'PlsPlaylistPlugin.cxx',
]

playlist_plugins_deps = [
  log_dep,
  expat_dep,
  flac_dep,
  input_basic_dep,
  config_dep,
]

playlist_features.set('ENABLE_CUE', get_option('cue'))
if get_option('cue')
  playlist_plugins_sources += [
    '../cue/CueParser.cxx',
    'CuePlaylistPlugin.cxx',
    'EmbeddedCuePlaylistPlugin.cxx',
  ]
endif

if expat_dep.found()
  playlist_plugins_sources += [
    'XspfPlaylistPlugin.cxx',
    'AsxPlaylistPlugin.cxx',
    'RssPlaylistPlugin.cxx',
  ]
endif

if flac_dep.found()
  playlist_plugins_sources += 'FlacPlaylistPlugin.cxx'
endif

playlist_plugins = static_library(
  'playlist_plugins',
  playlist_plugins_sources,
  include_directories: inc,
  dependencies: playlist_plugins_deps,
)

playlist_plugins_dep = declare_dependency(
  link_with: playlist_plugins,
  dependencies: [
    playlist_api_dep,
    tag_dep,
  ],
)
