if libavutil_dep.found()
  # if we have FFmpeg, we can use its MD5 implementation and we don't
  # need libgcrypt
  gcrypt_dep = dependency('', required: false)
  subdir_done()
endif

# Since version 0.49.0 Meson has native libgcrypt dependency support, which has
# the advantage over find_library() as it uses libgcrypt-config to query the
# required linker flags.
# However, we still need to use find_library() first, to prevent Meson
# falsly assuming a target libgcrypt is available in case there is no
# libgcrypt-config entry in the cross file and libgcrypt is installed on the
# host. In this case, Meson will falsly use the native libgcrypt-config and
# will falsly assume it has found the gcrypt library for the target.
#
# See: https://github.com/MusicPlayerDaemon/MPD/pull/495
gcrypt_dep = c_compiler.find_library('gcrypt', required: get_option('qobuz'))
if gcrypt_dep.found()
  gcrypt_dep = dependency('libgcrypt')
endif
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
