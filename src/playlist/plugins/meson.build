playlist_plugins_sources = [
  'ExtM3uPlaylistPlugin.cxx',
  'M3uPlaylistPlugin.cxx',
  'PlsPlaylistPlugin.cxx',
]

playlist_plugins_deps = [
  log_dep,
  expat_dep,
  flac_dep,
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

soundcloud_feature = get_option('soundcloud')
if soundcloud_feature.disabled()
  enable_soundcloud = false
else
  enable_soundcloud = curl_dep.found() and yajl_dep.found()
  if not enable_soundcloud and soundcloud_feature.enabled()
    error('SoundCloud requires CURL and libyajl')
  endif
endif
playlist_features.set('ENABLE_SOUNDCLOUD', enable_soundcloud)
if enable_soundcloud
  playlist_plugins_sources += 'SoundCloudPlaylistPlugin.cxx'
  playlist_plugins_deps += yajl_dep
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
