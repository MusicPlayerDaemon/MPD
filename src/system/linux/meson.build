system_linux_features = configuration_data()

if host_machine.system() == 'linux'
  have_native_openat2 = compiler.has_header_symbol('fcntl.h', 'openat2')
  # openat2() support was added to glibc 2.43
  system_linux_features.set('HAVE_NATIVE_OPENAT2', have_native_openat2)
  # if not provided by the C library, we roll our own using syscall()
  system_linux_features.set('HAVE_OPENAT2', have_native_openat2 or compiler.has_header('linux/openat2.h'))
endif

configure_file(output: 'Features.h', configuration: system_linux_features)
