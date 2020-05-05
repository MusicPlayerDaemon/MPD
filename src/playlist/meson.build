playlist_features = configuration_data()

playlist_api = static_library(
  'playlist_api',
  'PlaylistPlugin.cxx',
  'MemorySongEnumerator.cxx',
  include_directories: inc,
)

playlist_api_dep = declare_dependency(
  link_with: playlist_api,
)

subdir('plugins')

playlist_glue = static_library(
  'playlist_glue',
  'PlaylistRegistry.cxx',
  include_directories: inc,
)

playlist_glue_dep = declare_dependency(
  link_with: playlist_glue,
  dependencies: [
    playlist_plugins_dep,
  ],
)

configure_file(output: 'Features.h', configuration: playlist_features)
