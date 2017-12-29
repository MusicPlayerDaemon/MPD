gcrypt_dep = c_compiler.find_library('gcrypt', required: get_option('qobuz'))
if not gcrypt_dep.found()
  subdir_done()
endif

gcrypt = static_library(
  'gcrypt',
  'MD5.cxx',
  include_directories: inc,
  dependencies: [
    gcrypt_dep,
  ],
)

gcrypt_dep = declare_dependency(
  link_with: gcrypt,
  dependencies: [
    gcrypt_dep,
  ],
)
