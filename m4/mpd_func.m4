dnl MPD_OPTIONAL_FUNC(name, func, macro)
dnl
dnl Allow the user to enable or disable the use of a function.  If the
dnl option is not specified, the function is auto-detected.
AC_DEFUN([MPD_OPTIONAL_FUNC], [
	AC_ARG_ENABLE([$1],
		AS_HELP_STRING([--enable-$1],
			[use the function "$1" (default: auto)]),
		[test xenable_$1 = xyes && AC_DEFINE([$3], 1, [Define to use $1])],
		[AC_CHECK_FUNC([$2],
			[AC_DEFINE([$3], 1, [Define to use $1])],)])
])

dnl MPD_OPTIONAL_FUNC_NODEF(name, func)
dnl
dnl Allow the user to enable or disable the use of a function.
dnl Works similar to MPD_OPTIONAL_FUNC, however MPD_OPTIONAL_FUNC_NODEF
dnl does not invoke AC_DEFINE when function is enabled. Shell variable
dnl enable_$name is set to "yes" instead.
AC_DEFUN([MPD_OPTIONAL_FUNC_NODEF], [
	AC_ARG_ENABLE([$1],
		AS_HELP_STRING([--enable-$1],
			[use the function "$1" (default: auto)]),,
		[AC_CHECK_FUNC([$2], [enable_$1=yes],)])
])
