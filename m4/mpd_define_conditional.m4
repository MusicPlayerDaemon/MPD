dnl Wrapper for AC_DEFINE and AM_CONDITIONAL
dnl
dnl Parameters: varname1, varname2, description
AC_DEFUN([MPD_DEFINE_CONDITIONAL], [dnl
	AM_CONDITIONAL($2, test x$[]$1 = xyes)
	if test x$[]$1 = xyes; then
		AC_DEFINE($2, 1, [Define to enable $3])
	fi])
