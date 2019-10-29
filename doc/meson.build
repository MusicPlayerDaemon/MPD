install_man(['mpd.1', 'mpd.conf.5'])

sphinx = find_program('sphinx-build')
py = import('python').find_installation()
if run_command(py, '-c', 'import sphinx_rtd_theme').returncode() != 0
  error('python module "sphinx_rtd_theme" not found')
endif

sphinx_output = custom_target(
  'HTML documentation',
  output: 'html',
  input: [
    'index.rst', 'user.rst', 'developer.rst',
    'plugins.rst',
    'protocol.rst',
    'conf.py',
  ],
  command: [sphinx, '-q', '-b', 'html', '-d', '@OUTDIR@/doctrees', meson.current_source_dir(), '@OUTPUT@', '-D',  'html_theme=sphinx_rtd_theme'],
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
  
  
