icu_dep = dependency('icu-i18n', version: '>= 50', required: get_option('icu'))
conf.set('HAVE_ICU', icu_dep.found())

icu_sources = [
  'CaseFold.cxx',
  'Compare.cxx',
  'Collate.cxx',
  'Converter.cxx',
]

if is_windows
  icu_sources += 'Win32.cxx'
endif

if icu_dep.found()
  icu_sources += [
    'Util.cxx',
    'Init.cxx',
  ]
elif not get_option('iconv').disabled()
  have_iconv = compiler.has_function('iconv', prefix : '#include <iconv.h>')
  conf.set('HAVE_ICONV', have_iconv)
  if not have_iconv and get_option('iconv').enabled()
    error('iconv() not available')
  endif
endif

icu = static_library(
  'icu',
  icu_sources,
  include_directories: inc,
  dependencies: [
    icu_dep,
    fmt_dep,
  ],
)

icu_dep = declare_dependency(
  link_with: icu,
  dependencies: [
    util_dep,
  ],
)
