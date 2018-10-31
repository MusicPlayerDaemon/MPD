mixer_plugins_sources = [
  'NullMixerPlugin.cxx',
  'SoftwareMixerPlugin.cxx',
]

if alsa_dep.found()
  mixer_plugins_sources += [
    'AlsaMixerPlugin.cxx',
    'volume_mapping.c',
  ]
endif

if is_haiku
  mixer_plugins_sources += 'HaikuMixerPlugin.cxx'
endif

if enable_oss
  mixer_plugins_sources += 'OssMixerPlugin.cxx'
endif

if is_darwin
  mixer_plugins_sources += 'OSXMixerPlugin.cxx'
endif

if pulse_dep.found()
  mixer_plugins_sources += 'PulseMixerPlugin.cxx'
endif

if libsndio_dep.found()
  mixer_plugins_sources += 'SndioMixerPlugin.cxx'
endif

if is_windows
  mixer_plugins_sources += 'WinmmMixerPlugin.cxx'
endif

mixer_plugins = static_library(
  'mixer_plugins',
  mixer_plugins_sources,
  include_directories: inc,
  dependencies: [
    alsa_dep,
    pulse_dep,
    libsndio_dep,
  ]
)

mixer_plugins_dep = declare_dependency(
  link_with: mixer_plugins,
  dependencies: [
    config_dep,
  ],
)
