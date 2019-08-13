if gcrypt_dep.found()
  crypto_md5 = static_library(
    'crypto_md5',
    'MD5.cxx',
    include_directories: inc,
    dependencies: [
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
