if not get_option('html_manual')
  subdir_done()
endif

sphinx = find_program('sphinx-build', required: get_option('documentation'))
if not sphinx.found()
  subdir_done()
endif

if get_option('html_manual')
  sphinx_output = custom_target(
    'HTML documentation',
    output: 'html',
    input: [
      'index.rst', 'user.rst', 'developer.rst',
      'plugins.rst',
      'protocol.rst',
      'conf.py',
    ],
    command: [sphinx, '-q', '-b', 'html', '-d', '@OUTDIR@/doctrees', meson.current_source_dir(), '@OUTPUT@'],
    build_by_default: true,
    install: true,
    install_dir: join_paths(get_option('datadir'), 'doc', meson.project_name()),
  )

  custom_target(
    'upload',
    input: sphinx_output,
    output: 'upload',
    build_always_stale: true,
    command: [
      'rsync', '-vpruz', '--delete', meson.current_build_dir() + '/',
      'www.musicpd.org:/var/www/mpd/doc/',
      '--chmod=Dug+rwx,Do+rx,Fug+rw,Fo+r',
      '--include=html', '--include=html/**',
      '--exclude=*',
    ],
  )
endif

if get_option('manpages')
  # This installs all manpages in the "man1" subdirectory.  Due to
  # https://github.com/mesonbuild/meson/issues/1550 there appears to
  # be no good solution with Meson.  Sigh.
  custom_target(
    'Manpage documentation',
    output: 'man1',
    input: ['mpd.1.rst', 'conf.py'],
    command: [sphinx, '-q', '-b', 'man', '-d', '@OUTDIR@/man_doctrees', meson.current_source_dir(), '@OUTPUT@'],
    build_by_default: true,
    install: true,
    install_dir: get_option('mandir'),
  )
endif
