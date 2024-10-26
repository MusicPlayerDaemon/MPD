icu_i18n_dep = dependency('icu-i18n', version: '>= 50', required: get_option('icu'))
icu_uc_dep = dependency('icu-uc', version: '>= 50', required: get_option('icu'))
have_icu = icu_i18n_dep.found() and icu_uc_dep.found()
conf.set('HAVE_ICU', have_icu)

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
if have_icu
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
    icu_i18n_dep,
    icu_uc_dep,
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
