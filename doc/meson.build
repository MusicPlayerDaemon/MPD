if not get_option('html_manual') and not get_option('manpages')
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
      'client.rst',
      'protocol.rst',
      'conf.py',
    ],
    command: [sphinx, '-q', '-b', 'html', '-d', '@OUTDIR@/doctrees', meson.current_source_dir(), '@OUTPUT@'],
    build_by_default: true,
    install: true,
    install_dir: join_paths(get_option('datadir'), 'doc', meson.project_name()),
  )
endif

if get_option('manpages')
  custom_target(
    'Manpage documentation',
    output: ['mpd.1', 'mpd.conf.5'],
    input: ['mpd.1.rst', 'conf.py'],
    command: [sphinx, '-q', '-b', 'man', '-d', '@OUTDIR@/man_doctrees', meson.current_source_dir(), '@OUTDIR@'],
    build_by_default: true,
    install: true,
    install_dir: [join_paths(get_option('mandir'), 'man1'), join_paths(get_option('mandir'), 'man5')],
  )
endif

if get_option('doxygen')

  doxygen = find_program('doxygen', required: true)
  conf_data = configuration_data()
  conf_data.set('VERSION', meson.project_version())
  conf_data.set('abs_top_srcdir', meson.source_root())
  doxyfile = configure_file(
    input: 'doxygen.conf.in',
    output: 'Doxyfile',
    configuration: conf_data
  )
  
  custom_target('Doxygen',
    command: [doxygen, doxyfile],
    output: ['api'],
    build_by_default: true
  )
endif
