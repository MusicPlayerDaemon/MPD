system_linux_features = configuration_data()

if host_machine.system() == 'linux'
  system_linux_features.set('HAVE_OPENAT2', compiler.has_header('linux/openat2.h'))
endif

configure_file(output: 'Features.h', configuration: system_linux_features)
