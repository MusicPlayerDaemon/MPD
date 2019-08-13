decoder_plugins_sources = [
  'PcmDecoderPlugin.cxx',
]

if get_option('dsd')
  decoder_plugins_sources += [
    'HybridDsdDecoderPlugin.cxx',
    'DsdiffDecoderPlugin.cxx',
    'DsfDecoderPlugin.cxx',
    'DsdLib.cxx',
  ]
endif

if ffmpeg_dep.found()
  decoder_plugins_sources += [
    'FfmpegIo.cxx',
    'FfmpegMetaData.cxx',
    'FfmpegDecoderPlugin.cxx',
  ]
endif

adplug_dep = dependency('adplug', required: get_option('adplug'))
conf.set('ENABLE_ADPLUG', adplug_dep.found())
if adplug_dep.found()
  decoder_plugins_sources += 'AdPlugDecoderPlugin.cxx'
endif

conf.set('ENABLE_FLAC', flac_dep.found())
if flac_dep.found()
  decoder_plugins_sources += [
    'FlacDecoderPlugin.cxx',
    'FlacInput.cxx',
    'FlacPcm.cxx',
    'FlacDomain.cxx',
    'FlacCommon.cxx',
  ]
endif

conf.set('ENABLE_VORBIS_DECODER', libvorbis_dep.found() or libvorbisidec_dep.found())
conf.set('HAVE_TREMOR', libvorbisidec_dep.found())
if libvorbis_dep.found() or libvorbisidec_dep.found()
  decoder_plugins_sources += [
    'VorbisDecoderPlugin.cxx',
    'VorbisDomain.cxx',
  ]
endif

conf.set('ENABLE_OPUS', libopus_dep.found())
if libopus_dep.found()
  decoder_plugins_sources += [
    'OpusDecoderPlugin.cxx',
    'OpusDomain.cxx',
    'OpusHead.cxx',
    'OpusTags.cxx',
  ]
endif

if ogg_dep.found()
  decoder_plugins_sources += 'OggDecoder.cxx'
endif

if xiph_dep.found()
  decoder_plugins_sources += 'OggCodec.cxx'
endif

fluidsynth_dep = dependency('fluidsynth', version: '>= 1.1', required: get_option('fluidsynth'))
conf.set('ENABLE_FLUIDSYNTH', fluidsynth_dep.found())
if fluidsynth_dep.found()
  decoder_plugins_sources += 'FluidsynthDecoderPlugin.cxx'
endif

libaudiofile_dep = dependency('audiofile', version: '>= 0.3', required: get_option('audiofile'))
conf.set('ENABLE_AUDIOFILE', libaudiofile_dep.found())
if libaudiofile_dep.found()
  decoder_plugins_sources += 'AudiofileDecoderPlugin.cxx'
endif

libfaad_dep = c_compiler.find_library('faad', required: get_option('faad'))
conf.set('ENABLE_FAAD', libfaad_dep.found())
if libfaad_dep.found()
  decoder_plugins_sources += 'FaadDecoderPlugin.cxx'
endif

libgme_dep = c_compiler.find_library('gme', required: get_option('gme'))
conf.set('ENABLE_GME', libgme_dep.found())
if libgme_dep.found()
  decoder_plugins_sources += 'GmeDecoderPlugin.cxx'
endif

libmad_dep = c_compiler.find_library('mad', required: get_option('mad'))
conf.set('ENABLE_MAD', libmad_dep.found())
if libmad_dep.found()
  decoder_plugins_sources += 'MadDecoderPlugin.cxx'
endif

libmikmod_dep = dependency('libmikmod', version: '>= 3.2', required: get_option('mikmod'))
conf.set('ENABLE_LIBMIKMOD', libmikmod_dep.found())
if libmikmod_dep.found()
  decoder_plugins_sources += 'MikmodDecoderPlugin.cxx'
endif

libmodplug_dep = dependency('libmodplug', required: get_option('modplug'))
conf.set('ENABLE_MODPLUG', libmodplug_dep.found())
if libmodplug_dep.found()
  decoder_plugins_sources += 'ModplugDecoderPlugin.cxx'
endif

libmpcdec_dep = c_compiler.find_library('mpcdec', required: get_option('mpcdec'))
conf.set('ENABLE_MPCDEC', libmpcdec_dep.found())
if libmpcdec_dep.found()
  decoder_plugins_sources += 'MpcdecDecoderPlugin.cxx'
endif

libmpg123_dep = dependency('libmpg123', required: get_option('mpg123'))
conf.set('ENABLE_MPG123', libmpg123_dep.found())
if libmpg123_dep.found()
  decoder_plugins_sources += 'Mpg123DecoderPlugin.cxx'
endif

libsndfile_dep = dependency('sndfile', required: get_option('sndfile'))
conf.set('ENABLE_SNDFILE', libsndfile_dep.found())
if libsndfile_dep.found()
  decoder_plugins_sources += 'SndfileDecoderPlugin.cxx'
endif

wavpack_dep = dependency('wavpack', required: get_option('wavpack'))
conf.set('ENABLE_WAVPACK', wavpack_dep.found())
if wavpack_dep.found()
  decoder_plugins_sources += 'WavpackDecoderPlugin.cxx'
endif

wildmidi_dep = c_compiler.find_library('WildMidi', required: get_option('wildmidi'))
conf.set('ENABLE_WILDMIDI', wildmidi_dep.found())
if wildmidi_dep.found()
  decoder_plugins_sources += 'WildmidiDecoderPlugin.cxx'
endif

if not get_option('sidplay').disabled()
  libsidplayfp_dep = dependency('libsidplayfp', required: false)
  conf.set('HAVE_SIDPLAYFP', libsidplayfp_dep.found())

  if libsidplayfp_dep.found()
    libsidplay_dep = libsidplayfp_dep
  else
    libsidplay2_dep = dependency('libsidplay2', required: false)
    if libsidplay2_dep.found()
      libsidutils_dep = dependency('libsidutils')
      libresid_builder_dep = compiler.find_library('resid-builder')
      libsidplay_dep = declare_dependency(dependencies: [libsidplay2_dep, libsidutils_dep, libresid_builder_dep])
    elif get_option('sidplay').enabled()
      error('Neither libsidplayfp nor libsidplay2 found')
    else
      libsidplay_dep = libsidplay2_dep
    endif
  endif
else
  libsidplay_dep = dependency('', required: false)
endif
conf.set('ENABLE_SIDPLAY', libsidplay_dep.found())
if libsidplay_dep.found()
  decoder_plugins_sources += 'SidplayDecoderPlugin.cxx'
endif

decoder_plugins = static_library(
  'decoder_plugins',
  decoder_plugins_sources,
  include_directories: inc,
  dependencies: [
    adplug_dep,
    ffmpeg_dep,
    flac_dep,
    fluidsynth_dep,
    libaudiofile_dep,
    libfaad_dep,
    libgme_dep,
    libmad_dep,
    libmikmod_dep,
    libmodplug_dep,
    libmpcdec_dep,
    libmpg123_dep,
    libopus_dep,
    libsidplay_dep,
    libsndfile_dep,
    libvorbis_dep,
    libvorbisidec_dep,
    ogg_dep,
    wavpack_dep,
    wildmidi_dep,
  ],
)

decoder_plugins_dep = declare_dependency(
  link_with: decoder_plugins,
  dependencies: [
    crypto_base64_dep,
    decoder_api_dep,
    pcm_dep,
  ],
)
