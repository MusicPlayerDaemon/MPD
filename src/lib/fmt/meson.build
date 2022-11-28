libfmt = dependency('fmt', version: '>= 7', fallback: ['fmt', 'fmt_dep'])

if compiler.get_id() == 'clang' and compiler.version().version_compare('<15')
  libfmt = declare_dependency(
    dependencies: libfmt,
    # suppress bogus clang 14 warning (the version in Android NDK r25b)
    compile_args: ['-Wno-unused-local-typedef'],
  )
endif

fmt_dep = declare_dependency(
  dependencies: libfmt,
)
