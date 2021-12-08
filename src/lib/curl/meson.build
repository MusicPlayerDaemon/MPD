curl_dep = dependency('libcurl', version: '>= 7.33', required: get_option('curl'))
conf.set('ENABLE_CURL', curl_dep.found())
if not curl_dep.found()
  subdir_done()
endif

if is_windows
  # Our Windows build generates a static libcurl build, but libcurl's
  # CMakeLists.txt omits the -DCURL_STATICLIB compiler option
  curl_dep = declare_dependency(compile_args: '-DCURL_STATICLIB',
                                dependencies: curl_dep)
endif

curl = static_library(
  'curl',
  'Delegate.cxx',
  'Version.cxx',
  'Init.cxx',
  'Global.cxx',
  'Request.cxx',
  'Setup.cxx',
  'Adapter.cxx',
  'Escape.cxx',
  'Form.cxx',
  include_directories: inc,
  dependencies: [
    log_dep,
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
