icu_dep = dependency('icu-i18n', version: '>= 50', required: get_option('icu'))
conf.set('HAVE_ICU', icu_dep.found())

icu_sources = [
  'Canonicalize.cxx',
  'Compare.cxx',
  'Collate.cxx',
  'Converter.cxx',
]

if is_windows
  icu_sources += 'Win32.cxx'
endif

iconv_dep = []
if icu_dep.found()
  icu_sources += [
    'Init.cxx',
    'Util.cxx',
    'FoldCase.cxx',
    'Normalize.cxx',
    'Transliterator.cxx',
  ]
else
  iconv_dep = dependency('iconv', required: get_option('iconv'))
  conf.set('HAVE_ICONV', iconv_dep.found())
endif

icu = static_library(
  'icu',
  icu_sources,
  include_directories: inc,
  dependencies: [
    icu_dep,
    iconv_dep,
    fmt_dep,
  ],
)

icu_dep = declare_dependency(
  link_with: icu,
  dependencies: [
    util_dep,
  ],
)
