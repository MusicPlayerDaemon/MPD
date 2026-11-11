if not is_linux
  tinyalsa_dep = dependency('', required: false)
  subdir_done()
endif

tinyalsa_dep = dependency('tinyalsa', version: '>= 2.0', required: get_option('tinyalsa'))
conf.set('ENABLE_TINYALSA', tinyalsa_dep.found())
