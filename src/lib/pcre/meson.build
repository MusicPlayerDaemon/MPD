pcre_dep = dependency('libpcre', required: get_option('pcre'))
conf.set('HAVE_PCRE', pcre_dep.found())
if not pcre_dep.found()
  subdir_done()
endif

pcre = static_library(
  'pcre',
  'UniqueRegex.cxx',
  include_directories: inc,
  dependencies: [
    pcre_dep,
  ],
)

pcre_dep = declare_dependency(
  link_with: pcre,
  dependencies: [
    pcre_dep,
  ],
)
