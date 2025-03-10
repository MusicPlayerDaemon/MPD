test(
  'TestPlaylistPlugins',
  executable(
    'TestPlaylistPlugins',
    'PlaylistUtil.cxx',
    'TestPlsPlaylistPlugin.cxx',
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
