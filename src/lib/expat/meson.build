expat_dep = dependency('expat',
                       fallback: ['expat', 'expat_dep'],
                       required: get_option('expat'))
conf.set('ENABLE_EXPAT', expat_dep.found())
if not expat_dep.found()
  subdir_done()
endif

expat = static_library(
  'expat',
  'ExpatParser.cxx',
  'StreamExpatParser.cxx',
  include_directories: inc,
  dependencies: [
    expat_dep,
  ],
)

expat_dep = declare_dependency(
  link_with: expat,
  dependencies: [
    expat_dep,
  ]
)
