libflac_dep = dependency('flac', version: '>= 1.2', required: get_option('flac'))
libopus_dep = dependency('opus', required: get_option('opus'))

if get_option('tremor').enabled()
  # no libvorbis if Tremor was explicitly enabled
  libvorbis_dep = dependency('', required: false)
else
  libvorbis_dep = dependency('vorbis', required: get_option('vorbis'))
endif

if libvorbis_dep.found()
  # no Tremor if libvorbis is used
  libvorbisidec_dep = dependency('', required: false)
else
  # attempt to auto-detect Tremor only if libvorbis was disabled or not found
  libvorbisidec_dep = dependency('vorbisidec', required: get_option('tremor'))
endif

if get_option('vorbis').enabled() and get_option('tremor').enabled()
	error('Cannot build both, the Vorbis decoder AND the Tremor (Vorbis fixed-point) decoder')
endif

libvorbisenc_dep = dependency('', required: false)
if need_encoder and not get_option('vorbisenc').disabled()
  if libvorbis_dep.found()
    libvorbisenc_dep = dependency('vorbisenc', required: get_option('vorbisenc'))
  elif get_option('vorbisenc').enabled()
    error('Cannot build the Vorbis encoder without the Vorbis decoder')
  else
    message('Disabling the Vorbis encoder because the Vorbis decoder is disabled as well')
  endif
endif

if libopus_dep.found() or libvorbis_dep.found() or libvorbisenc_dep.found() or libvorbisidec_dep.found()
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
  'ScanVorbisComment.cxx',
  'VorbisComments.cxx',
  'VorbisPicture.cxx',
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
