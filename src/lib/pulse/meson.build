pulse_dep = dependency('libpulse', version: '>= 0.9.16', required: get_option('pulse'))
conf.set('ENABLE_PULSE', pulse_dep.found())
if not pulse_dep.found()
  subdir_done()
endif

pulse = static_library(
  'pulse',
  'LogError.cxx',
  'Error.cxx',
  'Domain.cxx',
  include_directories: inc,
  dependencies: [
    pulse_dep,
    log_dep,
  ],
)

pulse_dep = declare_dependency(
  link_with: pulse,
  dependencies: [
    pulse_dep,
  ],
)
