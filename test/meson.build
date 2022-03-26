gtest_compile_args = [
  '-Wno-undef',
]

if compiler.get_id() == 'gcc'
  gtest_compile_args += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',
  ]
endif

if compiler.get_id() == 'clang' and compiler.version().version_compare('>=9')
  gtest_compile_args += [
    # work around clang warning caused by GTest's wrong "-lpthread"
    # compiler flag
    '-Wno-unused-command-line-argument',

    # work around "error: 'ScopedTrace' was marked unused but was
    # used"
    '-Wno-used-but-marked-unused',
  ]
endif

gtest_main_dep = dependency('gtest', main: true, fallback: ['gtest', 'gtest_main_dep'])
gtest_dep = declare_dependency(
  dependencies: [gtest_main_dep],
  compile_args: gtest_compile_args,
)

subdir('util')
subdir('net')
subdir('time')
subdir('tag')

executable(
  'read_conf',
  'read_conf.cxx',
  include_directories: inc,
  dependencies: [
    config_dep,
  ],
)

test(
  'TestRewindInputStream',
  executable(
    'TestRewindInputStream',
    'TestRewindInputStream.cxx',
    include_directories: inc,
    dependencies: [
      input_glue_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

test(
  'test_protocol',
  executable(
    'test_protocol',
    'test_protocol.cxx',
    '../src/protocol/ArgParser.cxx',
    include_directories: inc,
    dependencies: [
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

test(
  'test_queue_priority',
  executable(
    'test_queue_priority',
    'test_queue_priority.cxx',
    '../src/queue/Queue.cxx',
    include_directories: inc,
    dependencies: [
      util_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

test(
  'TestFs',
  executable(
    'TestFs',
    'TestFs.cxx',
    'TestLookupFile.cxx',
    include_directories: inc,
    dependencies: [
      fs_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

test(
  'TestIcu',
  executable(
    'TestIcu',
    'TestIcu.cxx',
    include_directories: inc,
    dependencies: [
      util_dep,
      icu_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

if zeroconf_dep.found()
  executable(
    'RunZeroconf',
    'RunZeroconf.cxx',
    'ShutdownHandler.cxx',
    include_directories: inc,
    dependencies: [
      zeroconf_dep,
      util_dep,
    ],
  )
endif

if enable_inotify
  executable(
    'run_inotify',
    'run_inotify.cxx',
    'ShutdownHandler.cxx',
    '../src/db/update/InotifyDomain.cxx',
    '../src/db/update/InotifySource.cxx',
    include_directories: inc,
    dependencies: [
      log_dep,
      event_dep,
      util_dep,
    ],
  )
endif

executable(
  'run_resolver',
  'run_resolver.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    util_dep,
  ],
)

#
# I/O
#

executable(
  'WriteFile',
  'WriteFile.cxx',
  include_directories: inc,
  dependencies: [
    fs_dep,
  ],
)

executable(
  'dump_text_file',
  'dump_text_file.cxx',
  include_directories: inc,
  dependencies: [
    input_glue_dep,
    archive_glue_dep,
  ],
)

if zlib_dep.found()
  executable(
    'run_gzip',
    'run_gzip.cxx',
    include_directories: inc,
    dependencies: [
      zlib_dep,
      util_dep,
    ],
  )

  executable(
    'run_gunzip',
    'run_gunzip.cxx',
    include_directories: inc,
    dependencies: [
      fs_dep,
    ],
  )
endif

#
# Filter
#

executable(
  'ParseSongFilter',
  'ParseSongFilter.cxx',
  include_directories: inc,
  dependencies: [
    song_dep,
    pcm_dep,
  ],
)

test(
  'TestSongFilter',
  executable(
    'TestSongFilter',
    'TestTagSongFilter.cxx',
    include_directories: inc,
    dependencies: [
      song_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

#
# Neighbor
#

if neighbor_glue_dep.found()
  executable(
    'run_neighbor_explorer',
    'run_neighbor_explorer.cxx',
    'ShutdownHandler.cxx',
    include_directories: inc,
    dependencies: [
      neighbor_glue_dep,
    ],
  )
endif

#
# Database
#

if enable_database
  executable(
    'run_storage',
    'run_storage.cxx',
    include_directories: inc,
    dependencies: [
      event_dep,
      storage_glue_dep,
    ],
  )

  executable(
    'DumpDatabase',
    'DumpDatabase.cxx',
    '../src/db/Registry.cxx',
    '../src/db/Selection.cxx',
    '../src/db/PlaylistVector.cxx',
    '../src/db/DatabaseLock.cxx',
    '../src/SongSave.cxx',
    '../src/TagSave.cxx',
    include_directories: inc,
    dependencies: [
      pcm_basic_dep,
      song_dep,
      fs_dep,
      event_dep,
      db_plugins_dep,
    ],
  )

  test(
    'test_translate_song',
    executable(
      'test_translate_song',
      'test_translate_song.cxx',
      '../src/playlist/PlaylistSong.cxx',
      '../src/SongLoader.cxx',
      '../src/LocateUri.cxx',
      include_directories: inc,
      dependencies: [
        log_dep,
        tag_dep,
        storage_glue_dep,
        gtest_dep,
      ],
    ),
    protocol: 'gtest',
  )
endif

#
# Input
#

run_input = executable(
  'run_input',
  'run_input.cxx',
  '../src/TagSave.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
    input_glue_dep,
    archive_glue_dep,
  ],
)

if curl_dep.found()
  executable(
    'RunCurl',
    'RunCurl.cxx',
    'ShutdownHandler.cxx',
    include_directories: inc,
    dependencies: [
      curl_dep,

      # Explicitly linking with zlib here works around a linker
      # failure on Windows, because our Windows CURL build is
      # statically linked and thus declares no dependency on zlib
      zlib_dep,
    ],
  )

  test(
    'test_icy_parser',
    executable(
      'test_icy_parser',
      'test_icy_parser.cxx',
      include_directories: inc,
      dependencies: [
        icu_dep,
        tag_dep,
        gtest_dep,
      ],
    ),
    protocol: 'gtest',
  )
endif

#
# Archive
#

if archive_glue_dep.found()
  executable(
    'visit_archive',
    'visit_archive.cxx',
    include_directories: inc,
    dependencies: [
      archive_glue_dep,
    ],
  )

  if libiso9660_dep.found()
    if find_program('mkisofs', required: false).found()
      test(
        'test_archive_iso9660',
        find_program('test_archive_iso9660.sh'),
        depends: run_input,
      )
    endif
  endif

  if libbz2_dep.found()
    if find_program('bzip2', required: false).found()
      test(
        'test_archive_bzip2',
        find_program('test_archive_bzip2.sh'),
        depends: run_input,
      )
    endif
  endif

  if libzzip_dep.found()
    if find_program('zip', required: false).found()
      test(
        'test_archive_zzip',
        find_program('test_archive_zzip.sh'),
        depends: run_input,
      )
    endif
  endif
endif

#
# Playlist
#

executable(
  'dump_playlist',
  'dump_playlist.cxx',
  '../src/TagSave.cxx',
  '../src/TagFile.cxx',
  include_directories: inc,
  dependencies: [
    playlist_glue_dep,
    input_glue_dep,
    archive_glue_dep,
    decoder_glue_dep,
  ],
)

#
# Tag
#

if chromaprint_dep.found()
  executable(
    'RunChromaprint',
    'RunChromaprint.cxx',
    '../src/lib/chromaprint/DecoderClient.cxx',
    include_directories: inc,
    dependencies: [
      decoder_glue_dep,
      input_glue_dep,
      archive_glue_dep,
      chromaprint_dep,
    ],
  )
endif

#
# Decoder
#

executable(
  'run_decoder',
  'run_decoder.cxx',
  'DumpDecoderClient.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
    decoder_glue_dep,
    input_glue_dep,
    archive_glue_dep,
  ],
)

executable(
  'read_tags',
  'read_tags.cxx',
  include_directories: inc,
  dependencies: [
    decoder_glue_dep,
    input_glue_dep,
    archive_glue_dep,
  ],
)

executable(
  'ContainerScan',
  'ContainerScan.cxx',
  '../src/SongSave.cxx',
  '../src/TagSave.cxx',
  include_directories: inc,
  dependencies: [
    decoder_glue_dep,
    input_glue_dep,
    archive_glue_dep,
  ],
)

executable(
  'ReadApeTags',
  'ReadApeTags.cxx',
  include_directories: inc,
  dependencies: [
    tag_dep,
    input_glue_dep,
    archive_glue_dep,
  ],
)

if libid3tag_dep.found()
  executable(
    'dump_rva2',
    'dump_rva2.cxx',
    include_directories: inc,
    dependencies: [
      tag_dep,
      input_glue_dep,
      archive_glue_dep,
    ],
  )
endif

if ogg_dep.found()
  executable(
    'DumpOgg',
    'DumpOgg.cxx',
    include_directories: inc,
    dependencies: [
      ogg_dep,
      input_glue_dep,
      archive_glue_dep,
    ],
  )
endif
  
#
# Filter
#

test(
  'test_pcm',
  executable(
    'test_pcm',
    'TestAudioFormat.cxx',
    'test_pcm_dither.cxx',
    'test_pcm_pack.cxx',
    'test_pcm_channels.cxx',
    'test_pcm_format.cxx',
    'test_pcm_volume.cxx',
    'test_pcm_mix.cxx',
    'test_pcm_interleave.cxx',
    'test_pcm_export.cxx',
    include_directories: inc,
    dependencies: [
      pcm_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

executable(
  'run_filter',
  'run_filter.cxx',
  'ReadFrames.cxx',
  include_directories: inc,
  dependencies: [
    filter_glue_dep,
  ],
)

executable(
  'software_volume',
  'software_volume.cxx',
  include_directories: inc,
  dependencies: [
    pcm_dep,
  ],
)

executable(
  'run_normalize',
  'run_normalize.cxx',
  include_directories: inc,
  dependencies: [
    pcm_dep,
  ],
)

executable(
  'run_convert',
  'run_convert.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
    pcm_dep,
    config_dep,
  ],
)

executable(
  'RunReplayGainAnalyzer',
  'RunReplayGainAnalyzer.cxx',
  'ReadFrames.cxx',
  include_directories: inc,
  dependencies: [
    pcm_dep,
    io_dep,
  ],
)

executable(
  'RunMixRampAnalyzer',
  'RunMixRampAnalyzer.cxx',
  'ReadFrames.cxx',
  include_directories: inc,
  dependencies: [
    pcm_dep,
    io_dep,
  ],
)

#
# Encoder
#

if encoder_glue_dep.found()
  executable(
    'run_encoder',
    'run_encoder.cxx',
    include_directories: inc,
    dependencies: [
      encoder_glue_dep,
    ],
  )

  executable(
    'test_vorbis_encoder',
    'test_vorbis_encoder.cxx',
    include_directories: inc,
    dependencies: [
      encoder_glue_dep,
    ],
  )
endif
  
#
# Output
#

executable(
  'run_output',
  'run_output.cxx',
  include_directories: inc,
  dependencies: [
    output_glue_dep,
    encoder_glue_dep,
    event_dep,
  ],
)

#
# Mixer
#

if alsa_dep.found()
  # this debug program is still ALSA specific

  executable(
    'read_mixer',
    'read_mixer.cxx',
    include_directories: inc,
    dependencies: [
      mixer_glue_dep,
    ],
  )
endif
