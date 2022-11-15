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
  if meson.version().version_compare('>= 0.60')
    iconv_dep = dependency('iconv', required: get_option('iconv'))
    conf.set('HAVE_ICONV', iconv_dep.found())
  elif not get_option('iconv').disabled()
    iconv_open_snippet = '''#include <iconv.h>
      int main() {
      iconv_open("","");
      }'''
    have_iconv = compiler.links(iconv_open_snippet, name: 'iconv_open')
    if not have_iconv
      iconv_dep = compiler.find_library('iconv', required: false)
      have_iconv = compiler.links(iconv_open_snippet, dependencies: iconv_dep, name: 'iconv_open')
    endif
    if not have_iconv and get_option('iconv').enabled()
      error('iconv() not available')
    endif
    conf.set('HAVE_ICONV', have_iconv)
  endif
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
