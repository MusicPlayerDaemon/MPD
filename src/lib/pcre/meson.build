pcre_dep = dependency('libpcre2-8', required: get_option('pcre'))
conf.set('HAVE_PCRE', pcre_dep.found())
if not pcre_dep.found()
  subdir_done()
endif

pcre_dep = declare_dependency(
  compile_args: '-DPCRE2_CODE_UNIT_WIDTH=0',
  dependencies: pcre_dep,
)

pcre = static_library(
  'pcre',
  'Error.cxx',
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
