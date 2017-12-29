libsndio_dep = c_compiler.find_library('sndio', required: get_option('sndio'))
if libsndio_dep.found()
  if c_compiler.has_header_symbol('sndio.h', 'ROAR_VERSION')
    if get_option('sndio').enabled()
      error('Found libroarsndio, which is known to be broken.')
    else
      warning('Found libroarsndio, which is known to be broken; ignoring it.')
      libsndio_dep = dependency('', required: false)
    endif
  endif
endif
conf.set('ENABLE_SNDIO', libsndio_dep.found())
