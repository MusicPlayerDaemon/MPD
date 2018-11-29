ytdl_feature = get_option('youtube-dl')
if ytdl_feature.disabled()
  enable_ytdl = false
else
  enable_ytdl = curl_dep.found() and yajl_dep.found()
  if not enable_ytdl and ytdl_feature.enabled()
    error('youtube-dl requires CURL and libyajl')
  endif
endif
conf.set('ENABLE_YTDL', enable_ytdl)
if not enable_ytdl
  subdir_done()
endif

ytdl = static_library(
  'youtube-dl',
  'Init.cxx',
  'Invoke.cxx',
  'Parser.cxx',
  'TagHandler.cxx',
  include_directories: inc,
  dependencies: [
    curl_dep,
    yajl_dep,
  ],
)

ytdl_dep = declare_dependency(
  link_with: ytdl,
  dependencies: [
    event_dep,
    util_dep,
    curl_dep,
    yajl_dep,
  ],
)
