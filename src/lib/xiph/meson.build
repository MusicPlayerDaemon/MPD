libflac_dep = dependency('flac', version: '>= 1.2', required: get_option('flac'))
libopus_dep = dependency('opus', required: get_option('opus'))
libvorbis_dep = dependency('vorbis', required: get_option('vorbis'))

if need_encoder
  libvorbisenc_dep = dependency('vorbisenc', required: get_option('vorbisenc'))
else
  libvorbisenc_dep = dependency('', required: false)
endif

if libopus_dep.found() or libvorbis_dep.found() or libvorbisenc_dep.found()
  libogg_dep = dependency('ogg')
else
  libogg_dep = dependency('', required: false)
endif

if not libogg_dep.found() and not libflac_dep.found()
  xiph_dep = dependency('', required: false)
  ogg_dep = dependency('', required: false)
  flac_dep = dependency('', required: false)
  subdir_done()
endif

xiph = static_library(
  'xiph',
  'VorbisComments.cxx',
  'XiphTags.cxx',
  include_directories: inc,
)

xiph_dep = declare_dependency(
  link_with: xiph,
)

if libogg_dep.found()
  ogg = static_library(
    'ogg',
    'OggVisitor.cxx',
    'OggSerial.cxx',
    'OggSyncState.cxx',
    'OggFind.cxx',
    'OggPacket.cxx',
    include_directories: inc,
    dependencies: [
      libogg_dep,
    ],
  )

  ogg_dep = declare_dependency(
    link_with: ogg,
    dependencies: [
      xiph_dep,
      libogg_dep,
    ],
  )
else
  ogg_dep = dependency('', required: false)
endif

if libflac_dep.found()
  flac = static_library(
    'flac',
    'FlacIOHandle.cxx',
    'FlacMetadataChain.cxx',
    'FlacStreamMetadata.cxx',
    include_directories: inc,
    dependencies: [
      libflac_dep,
    ],
  )

  flac_dep = declare_dependency(
    link_with: flac,
    dependencies: [
      xiph_dep,
      libflac_dep,
    ],
  )
else
  flac_dep = dependency('', required: false)
endif
