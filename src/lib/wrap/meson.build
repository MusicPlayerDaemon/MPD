libwrap_option = get_option('libwrap')
enable_libwrap = false
if not libwrap_option.disabled() and compiler.has_header('tcpd.h') and compiler.compiles('''
#include <tcpd.h>
bool CheckLibWrap(int fd, const char &progname) {
	struct request_info req;
	request_init(&req, RQ_FILE, fd, RQ_DAEMON, progname, 0);
	fromhost(&req);
	return hosts_access(&req);
}
''')
  libwrap_dep = compiler.find_library('wrap', required: libwrap_option)
else
  libwrap_dep = dependency('', required: libwrap_option)
endif

if not libwrap_dep.found() and libwrap_option.enabled()
  error('libwrap not found')
endif
  
conf.set('HAVE_LIBWRAP', libwrap_dep.found())
