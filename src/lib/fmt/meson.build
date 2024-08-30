libfmt = dependency('fmt', version: '>= 9', fallback: ['fmt', 'fmt_dep'])


if compiler.get_id() == 'gcc' and compiler.version().version_compare('>=13') and compiler.version().version_compare('<15')
  libfmt = declare_dependency(
    dependencies: libfmt,
    # suppress bogus GCC 13 warning: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109717
    compile_args: ['-Wno-array-bounds'],
  )
endif

fmt = static_library(
  'fmt',
  'ExceptionFormatter.cxx',
  'RuntimeError.cxx',
  'SocketAddressFormatter.cxx',
  'SystemError.cxx',
  include_directories: inc,
  dependencies: libfmt,
)

fmt_dep = declare_dependency(
  link_with: fmt,
  dependencies: libfmt,
)
