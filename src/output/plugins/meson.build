output_plugins_sources = [
  'NullOutputPlugin.cxx',
]

output_plugins_deps = [
  output_api_dep,
  config_dep,
  tag_dep,
]

need_encoder = false

if alsa_dep.found()
  output_plugins_sources += 'AlsaOutputPlugin.cxx'
endif

libao_dep = dependency('ao', required: get_option('ao'))
conf.set('ENABLE_AO', libao_dep.found())
if libao_dep.found()
  output_plugins_sources += 'AoOutputPlugin.cxx'
endif

enable_fifo_output = get_option('fifo') and not is_windows
conf.set('HAVE_FIFO', enable_fifo_output)
if enable_fifo_output
  output_plugins_sources += 'FifoOutputPlugin.cxx'
endif

if is_haiku
  output_plugins_sources += 'HaikuOutputPlugin.cxx'
endif

conf.set('ENABLE_HTTPD_OUTPUT', get_option('httpd'))
if get_option('httpd')
  output_plugins_sources += [
    'httpd/IcyMetaDataServer.cxx',
    'httpd/Page.cxx',
    'httpd/HttpdClient.cxx',
    'httpd/HttpdOutputPlugin.cxx',
  ]
  output_plugins_deps += [ event_dep, net_dep ]
  need_encoder = true
endif

libjack_dep = dependency('jack', version: '>= 0.100', required: get_option('jack'))
conf.set('ENABLE_JACK', libjack_dep.found())
if libjack_dep.found()
  output_plugins_sources += 'JackOutputPlugin.cxx'
  conf.set('HAVE_JACK_SET_INFO_FUNCTION', compiler.has_header_symbol('jack/jack.h', 'jack_set_info_function'))
endif

openal_dep = dependency('', required: false)
if not get_option('openal').disabled()
  if is_darwin
    if compiler.has_header('OpenAL/al.h')
      openal_dep = declare_dependency(link_args: ['-framework', 'OpenAL'])
    endif
  else
    openal_dep = dependency('openal', required: false)
  endif

  if openal_dep.found()
    output_plugins_sources += 'OpenALOutputPlugin.cxx'
  elif get_option('openal').enabled()
    error('OpenAL not available')
  endif
endif
conf.set('HAVE_OPENAL', openal_dep.found())

if enable_oss
  output_plugins_sources += 'OssOutputPlugin.cxx'
endif

if is_darwin
  output_plugins_sources += 'OSXOutputPlugin.cxx'
  audiounit_dep = declare_dependency(
    link_args: [
      '-framework', 'AudioUnit', '-framework', 'CoreAudio', '-framework', 'CoreServices',
    ],
    dependencies: [
      boost_dep,
    ],
  )
else
  audiounit_dep = dependency('', required: false)
endif
conf.set('HAVE_OSX', is_darwin)

enable_pipe_output = get_option('pipe') and not is_windows
conf.set('ENABLE_PIPE_OUTPUT', enable_pipe_output)
if enable_pipe_output
  output_plugins_sources += 'PipeOutputPlugin.cxx'
endif

if pulse_dep.found()
  output_plugins_sources += 'PulseOutputPlugin.cxx'
endif

conf.set('ENABLE_RECORDER_OUTPUT', get_option('recorder'))
if get_option('recorder')
  output_plugins_sources += 'RecorderOutputPlugin.cxx'
  need_encoder = true
endif

libshout_dep = dependency('shout', required: get_option('shout'))
conf.set('HAVE_SHOUT', libshout_dep.found())
if libshout_dep.found()
  output_plugins_sources += 'ShoutOutputPlugin.cxx'
  need_encoder = true
endif

if is_android
  sles_dep = c_compiler.find_library('OpenSLES')
  output_plugins_sources += 'sles/SlesOutputPlugin.cxx'
else
  sles_dep = dependency('', required: false)
endif

if libsndio_dep.found()
  output_plugins_sources += 'SndioOutputPlugin.cxx'
endif

enable_solaris_output = get_option('solaris_output')
if enable_solaris_output.auto()
  enable_solaris_output = host_machine.system() == 'sunos' or host_machine.system() == 'solaris'
else
  enable_solaris_output = enable_solaris_output.enabled()
endif
conf.set('ENABLE_SOLARIS_OUTPUT', enable_solaris_output)
if enable_solaris_output
  output_plugins_sources += 'SolarisOutputPlugin.cxx'
endif

conf.set('ENABLE_WINMM_OUTPUT', is_windows)
if is_windows
  output_plugins_sources += 'WinmmOutputPlugin.cxx'
  winmm_dep = c_compiler.find_library('winmm')
else
  winmm_dep = dependency('', required: false)
endif

output_plugins = static_library(
  'output_plugins',
  output_plugins_sources,
  include_directories: inc,
  dependencies: [
    alsa_dep,
    audiounit_dep,
    libao_dep,
    libjack_dep,
    pulse_dep,
    libshout_dep,
    libsndio_dep,
    openal_dep,
    sles_dep,
    winmm_dep,
  ],
)

output_plugins_dep = declare_dependency(
  link_with: output_plugins,
  dependencies: output_plugins_deps,
)
