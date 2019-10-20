enable_noson = get_option('noson')
if enable_noson.disabled()
  enable_noson = false
elif enable_noson.auto()
  enable_noson = false

endif

conf.set('HAVE_NOSON', enable_noson)
