input_plugins_sources = [
  'FileInputPlugin.cxx',
]

if uring_dep.found()
  input_plugins_sources += 'UringInputPlugin.cxx'
endif

if alsa_dep.found()
  input_plugins_sources += 'AlsaInputPlugin.cxx'
endif

libcdio_paranoia_dep = dependency('libcdio_paranoia', version: '>= 10.2+0.93+1', required: get_option('cdio_paranoia'))
input_features.set('ENABLE_CDIO_PARANOIA', libcdio_paranoia_dep.found())
if libcdio_paranoia_dep.found()
  input_plugins_sources += 'CdioParanoiaInputPlugin.cxx'
endif

if curl_dep.found()
  input_plugins_sources += [
    'CurlInputPlugin.cxx',
    '../IcyInputStream.cxx',
    '../../tag/IcyMetaDataParser.cxx',
  ]
endif

if ffmpeg_dep.found()
  input_plugins_sources += 'FfmpegInputPlugin.cxx'
endif

libmms_dep = dependency('libmms', version: '>= 0.4', required: get_option('mms'))
input_features.set('ENABLE_MMS', libmms_dep.found())
if libmms_dep.found()
  input_plugins_sources += 'MmsInputPlugin.cxx'
endif

if nfs_dep.found()
  input_plugins_sources += 'NfsInputPlugin.cxx'
endif

if smbclient_dep.found()
  input_plugins_sources += 'SmbclientInputPlugin.cxx'
endif

qobuz_feature = get_option('qobuz')
if qobuz_feature.disabled()
  enable_qobuz = false
else
  enable_qobuz = curl_dep.found() and yajl_dep.found() and crypto_md5_dep.found()
  if not enable_qobuz and qobuz_feature.enabled()
    error('Qobuz requires CURL, libyajl and libgcrypt')
  endif
endif
input_features.set('ENABLE_QOBUZ', enable_qobuz)
if enable_qobuz
  input_plugins_sources += [
    'QobuzClient.cxx',
    'QobuzErrorParser.cxx',
    'QobuzLoginRequest.cxx',
    'QobuzTrackRequest.cxx',
    'QobuzTagScanner.cxx',
    'QobuzInputPlugin.cxx',
  ]
endif

input_plugins = static_library(
  'input_plugins',
  input_plugins_sources,
  include_directories: inc,
  dependencies: [
    log_dep,
    alsa_dep,
    curl_dep,
    ffmpeg_dep,
    libcdio_paranoia_dep,
    libmms_dep,
    nfs_dep,
    smbclient_dep,
    yajl_dep,
    crypto_md5_dep,
  ],
)

input_plugins_dep = declare_dependency(
  link_with: input_plugins,
  dependencies: [
    input_api_dep,
    pcm_dep,
  ],
)
