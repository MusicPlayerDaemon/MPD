if libavutil_dep.found() or gcrypt_dep.found()
  crypto_md5 = static_library(
    'crypto_md5',
    'MD5.cxx',
    include_directories: inc,
    dependencies: [
      libavutil_dep,
      gcrypt_dep,
    ],
  )

  crypto_md5_dep = declare_dependency(
    link_with: crypto_md5,
  )
else
  crypto_md5_dep = dependency('', required: false)
endif

conf.set('HAVE_MD5', crypto_md5_dep.found())

if libavutil_dep.found()
  crypto_base64 = static_library(
    'crypto_base64',
    'Base64.cxx',
    include_directories: inc,
    dependencies: [
      libavutil_dep,
    ],
  )

  crypto_base64_dep = declare_dependency(
    link_with: crypto_base64,
  )
else
  crypto_base64_dep = dependency('', required: false)
endif

conf.set('HAVE_BASE64', crypto_base64_dep.found())
