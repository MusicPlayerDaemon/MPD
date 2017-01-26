dnl Declare a dependency of one feature on another.  If the depending
dnl feature is disabled, the former must be disabled as well.  If the
dnl former was explicitly enabled, abort with an error message.
dnl
dnl Parameters: varname1, varname2 (=dependency), errmsg
AC_DEFUN([MPD_DEPENDS], [
	if test x$$2 = xno; then
		if test x$$1 = xauto; then
			$1=no
		elif test x$$1 = xyes; then
			AC_MSG_ERROR([$3])
		fi
	fi
])
