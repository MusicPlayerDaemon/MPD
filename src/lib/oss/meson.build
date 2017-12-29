enable_oss = get_option('oss')
if enable_oss.disabled()
  enable_oss = false
elif enable_oss.auto() and alsa_dep.found()
# don't bother auto-enabling OSS if ALSA is available
  enable_oss = false
elif compiler.has_header('sys/soundcard.h') or compiler.has_header('soundcard.h')
  enable_oss = true
elif enable_oss.auto()
  enable_oss = false
else
  error('sys/soundcard.h not found')
endif
  
conf.set('HAVE_OSS', enable_oss)
