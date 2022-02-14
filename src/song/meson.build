song = static_library(
  'song',
  'DetachedSong.cxx',
  'Escape.cxx',
  'StringFilter.cxx',
  'UriSongFilter.cxx',
  'BaseSongFilter.cxx',
  'TagSongFilter.cxx',
  'ModifiedSinceSongFilter.cxx',
  'PrioritySongFilter.cxx',
  'AudioFormatSongFilter.cxx',
  'AndSongFilter.cxx',
  'OptimizeFilter.cxx',
  'Filter.cxx',
  'LightSong.cxx',
  include_directories: inc,
  dependencies: [
    pcre_dep,
    fmt_dep,
  ],
)

song_dep = declare_dependency(
  link_with: song,
  dependencies: [
    icu_dep,
    pcre_dep,
    tag_dep,
    time_dep,
    util_dep,
  ],
)
