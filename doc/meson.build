install_man(['mpd.1', 'mpd.conf.5'])

sphinx = find_program('sphinx-build')
sphinx_output = custom_target(
  'HTML documentation',
  output: 'html',
  input: [
    'index.rst', 'user.rst', 'developer.rst',
    'conf.py',
  ],
  command: [sphinx, '-q', '-b', 'html', '-d', '@OUTDIR@/doctrees', meson.current_source_dir(), '@OUTPUT@'],
  build_by_default: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'doc', meson.project_name()),
)

xmlto = find_program('xmlto')
xmlto_output = custom_target(
  'Protocol documentation',
  output: 'protocol',
  input: 'protocol.xml',
  command: [
    xmlto, '-o', '@OUTPUT@',
    '--stringparam=chunker.output.encoding=utf-8',
    'html',
    '--stringparam=use.id.as.filename=1',
    '@INPUT@',
  ],
  build_by_default: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'doc', meson.project_name()),
)

custom_target(
  'upload',
  input: [sphinx_output, xmlto_output],
  output: 'upload',
  build_always_stale: true,
  command: [
    'rsync', '-vpruz', '--delete', meson.current_source_dir(),
    'www.musicpd.org:/var/www/mpd/doc/',
    '--chmod=Dug+rwx,Do+rx,Fug+rw,Fo+r',
    '--include=protocol', '--include=protocol/**',
    '--include=html', '--include=html/**',
    '--exclude=*',
  ],
)
  
  
