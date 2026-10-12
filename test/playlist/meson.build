test_playlist_plugins_sources = [
  'PlaylistUtil.cxx',
  'TestPlsPlaylistPlugin.cxx',
]

if expat_dep.found()
  test_playlist_plugins_sources += 'TestXspfPlaylistPlugin.cxx'
endif

test(
  'TestPlaylistPlugins',
  executable(
    'TestPlaylistPlugins',
    test_playlist_plugins_sources,
    '../../src/SongSave.cxx',
    '../../src/TagSave.cxx',
    '../../src/TagFile.cxx',
    include_directories: inc,
    dependencies: [
      gtest_dep,
      playlist_glue_dep,
      archive_glue_dep,
      input_glue_dep,
      decoder_glue_dep,
    ],
  ),
)
