libfmt = dependency('fmt', version: '>= 7', fallback: ['fmt', 'fmt_dep'])

if compiler.get_id() == 'clang' and compiler.version().version_compare('<15')
  libfmt = declare_dependency(
    dependencies: libfmt,
    # suppress bogus clang 14 warning (the version in Android NDK r25b)
    compile_args: ['-Wno-unused-local-typedef'],
  )
endif

fmt = static_library(
  'fmt',
  'RuntimeError.cxx',
  'SystemError.cxx',
  include_directories: inc,
  dependencies: libfmt,
)

fmt_dep = declare_dependency(
  link_with: fmt,
  dependencies: libfmt,
)
