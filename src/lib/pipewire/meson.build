pipewire_dep = dependency('libpipewire-0.3', required: get_option('pipewire'))
conf.set('ENABLE_PIPEWIRE', pipewire_dep.found())
if not pipewire_dep.found()
  subdir_done()
endif

pipewire_dep = declare_dependency(
  dependencies: pipewire_dep,

  # disabling -Wpedantic because libpipewire's headers are not
  # compatible with C++; using the "#pragma" is not enough, we need to
  # disable it at the command line
  compile_args: ['-Wno-pedantic'],
)

pipewire = static_library(
  'pipewire',
  'Error.cxx',
  include_directories: inc,
  dependencies: [
    pipewire_dep,
  ],
)

pipewire_dep = declare_dependency(
  link_with: pipewire,
  dependencies: pipewire_dep,
)
