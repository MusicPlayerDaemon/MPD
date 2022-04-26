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

iconv_dep = []
if icu_dep.found()
  icu_sources += [
    'Util.cxx',
    'Init.cxx',
  ]
elif not get_option('iconv').disabled()
  # an installed iconv library will make the builtin iconv() unavailable,
  # so search for the library first and pass it as (possible) dependency
  iconv_dep = compiler.find_library('libiconv', required: false)
  have_iconv = compiler.has_function('iconv', 
    dependencies: iconv_dep, 
    prefix : '#include <iconv.h>')
  if not have_iconv and get_option('iconv').enabled()
    error('iconv() not available')
  endif
  conf.set('HAVE_ICONV', have_iconv)
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
