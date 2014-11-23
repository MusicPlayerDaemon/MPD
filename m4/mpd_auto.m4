AC_DEFUN([MPD_AUTO_ENABLED], [
	if test x$[]enable_$1 = xauto; then
		AC_MSG_NOTICE([auto-detected $2])
		enable_$1=no
	fi
])

AC_DEFUN([MPD_AUTO_DISABLED], [
	if test x$[]enable_$1 = xauto; then
		AC_MSG_WARN([$3 -- disabling $2])
		enable_$1=no
	elif test x$[]enable_$1 = xyes; then
		AC_MSG_ERROR([$2: $3])
	fi
])

dnl Check whether a prerequisite for a feature was found.  This is
dnl very similar to MPD_AUTO_RESULT, but does not finalize the
dnl detection; it assumes that more checks will follow.
AC_DEFUN([MPD_AUTO_PRE], [
	if test x$[]enable_$1 != xno && test x$[]found_$1 = xno; then
                MPD_AUTO_DISABLED([$1], [$2], [$3])
	fi
])

AC_DEFUN([MPD_AUTO_RESULT], [
	if test x$[]enable_$1 = xno; then
		found_$1=no
	fi

	if test x$[]found_$1 = xyes; then
                MPD_AUTO_ENABLED([$1], [$2])
	else
                MPD_AUTO_DISABLED([$1], [$2], [$3])
	fi
])

dnl Parameters: varname1, description, errmsg, check
AC_DEFUN([MPD_AUTO], [
	if test x$[]enable_$1 != xno; then
		$4
	fi
	MPD_AUTO_RESULT([$1], [$2], [$3])
])

AC_DEFUN([MPD_AUTO_PKG], [
	MPD_AUTO([$1], [$4], [$5],
		[PKG_CHECK_MODULES([$2], [$3],
			[found_$1=yes],
			[found_$1=no])])
])

dnl Check with pkg-config first, fall back to AC_CHECK_LIB.
dnl
dnl Parameters: varname1, varname2, pkgname, libname, symname, libs, cflags, description, errmsg
AC_DEFUN([MPD_AUTO_PKG_LIB], [
	MPD_AUTO([$1], [$8], [$9],
		[PKG_CHECK_MODULES([$2], [$3],
			[found_$1=yes],
			AC_CHECK_LIB($4, $5,
				[found_$1=yes $2_LIBS='$6' $2_CFLAGS='$7'],
				[found_$1=no],
				[$6]))])
])

dnl Wrapper for AC_CHECK_LIB.
dnl
dnl Parameters: varname1, varname2, libname, symname, libs, cflags, description, errmsg
AC_DEFUN([MPD_AUTO_LIB], [
	AC_SUBST([$2_LIBS], [])
	AC_SUBST([$2_CFLAGS], [])

	MPD_AUTO([$1], [$7], [$8],
		[AC_CHECK_LIB($3, $4,
			[found_$1=yes $2_LIBS='$5' $2_CFLAGS='$6'],
			[found_$1=no],
			[$5])])
])

dnl Wrapper for AC_ARG_ENABLE and MPD_AUTO_PKG
dnl
dnl Parameters: varname1, varname2, pkg, description, errmsg, default, pre
AC_DEFUN([MPD_ENABLE_AUTO_PKG], [
	AC_ARG_ENABLE(translit([$1], [_], [-]),
		AS_HELP_STRING([--enable-]translit([$1], [_], [-]),
			[enable $4 (default: auto)]),,
		[enable_$1=]ifelse([$6], [], [auto], [$6]))

	$7

	MPD_AUTO_PKG($1, $2, $3, $4, $5)
	if test x$[]enable_$1 = xyes; then
		AC_DEFINE(ENABLE_$2, 1,
			[Define to enable $4])
	fi
	AM_CONDITIONAL(ENABLE_$2, test x$[]enable_$1 = xyes)
])

dnl Wrapper for AC_ARG_ENABLE and MPD_AUTO_PKG_LIB
dnl
dnl Parameters: varname1, varname2, pkg, libname, symname, libs, cflags, description, errmsg, default, pre
AC_DEFUN([MPD_ENABLE_AUTO_PKG_LIB], [
	AC_ARG_ENABLE(translit([$1], [_], [-]),
		AS_HELP_STRING([--enable-]translit([$1], [_], [-]),
			[enable $4 (default: auto)]),,
		[enable_$1=]ifelse([$10], [], [auto], [$10]))

	$11

	MPD_AUTO_PKG_LIB($1, $2, $3, $4, $5, $6, $7, $8, $9)
	if test x$[]enable_$1 = xyes; then
		AC_DEFINE(ENABLE_$2, 1,
			[Define to enable $4])
	fi
	AM_CONDITIONAL(ENABLE_$2, test x$[]enable_$1 = xyes)
])

dnl Wrapper for MPD_ENABLE_AUTO_PKG and MPD_DEPENDS
dnl
dnl Parameters: varname1, varname2, pkg, description, errmsg, default, dep_variable, dep_errmsg
AC_DEFUN([MPD_ENABLE_AUTO_PKG_DEPENDS], [
	MPD_ENABLE_AUTO_PKG([$1], [$2], [$3], [$4], [$5], [$6],
		[MPD_DEPENDS([enable_$1], [$7], [$8])])
])
