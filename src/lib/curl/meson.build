curl_dep = dependency('libcurl', version: '>= 7.33', required: get_option('curl'))
conf.set('ENABLE_CURL', curl_dep.found())
if not curl_dep.found()
  subdir_done()
endif

curl = static_library(
  'curl',
  'Delegate.cxx',
  'Version.cxx',
  'Init.cxx',
  'Global.cxx',
  'Request.cxx',
  'Form.cxx',
  include_directories: inc,
  dependencies: [
    curl_dep,
  ],
)

curl_dep = declare_dependency(
  link_with: curl,
  dependencies: [
    event_dep,
    util_dep,
    curl_dep,
  ],
)
