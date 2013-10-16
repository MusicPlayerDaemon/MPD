AC_DEFUN([MPD_AUTO_ENABLED], [
	var="enable_$1"
	feature="$2"

	if eval "test x`echo '$'$var` = xauto"; then
		AC_MSG_NOTICE([auto-detected $feature])
		eval "$var=yes"
	fi
])

AC_DEFUN([MPD_AUTO_DISABLED], [
	var="enable_$1"
	feature="$2"
	msg="$3"

	if eval "test x`echo '$'$var` = xauto"; then
		AC_MSG_WARN([$msg -- disabling $feature])
		eval "$var=no"
	elif eval "test x`echo '$'$var` = xyes"; then
		AC_MSG_ERROR([$feature: $msg])
	fi
])

dnl Check whether a prerequisite for a feature was found.  This is
dnl very similar to MPD_AUTO_RESULT, but does not finalize the
dnl detection; it assumes that more checks will follow.
AC_DEFUN([MPD_AUTO_PRE], [
	name="$1"
	var="enable_$1"
	found="found_$name"
	feature="$2"
	msg="$3"

	if eval "test x`echo '$'$var` != xno" && eval "test x`echo '$'$found` = xno"; then
                MPD_AUTO_DISABLED([$name], [$feature], [$msg])
	fi
])

AC_DEFUN([MPD_AUTO_RESULT], [
	name="$1"
	var="enable_$1"
	found="found_$name"
	feature="$2"
	msg="$3"

	if eval "test x`echo '$'$var` = xno"; then
		eval "$found=no"
	fi

	if eval "test x`echo '$'$found` = xyes"; then
                MPD_AUTO_ENABLED([$name], [$feature])
	else
                MPD_AUTO_DISABLED([$name], [$feature], [$msg])
	fi
])

AC_DEFUN([MPD_AUTO_PKG], [
	if eval "test x`echo '$'enable_$1` != xno"; then
		PKG_CHECK_MODULES([$2], [$3],
			[eval "found_$1=yes"],
			[eval "found_$1=no"])
	fi

	MPD_AUTO_RESULT([$1], [$4], [$5])
])

dnl Check with pkg-config first, fall back to AC_CHECK_LIB.
dnl
dnl Parameters: varname1, varname2, pkgname, libname, symname, libs, cflags, description, errmsg
AC_DEFUN([MPD_AUTO_PKG_LIB], [
	if eval "test x`echo '$'enable_$1` != xno"; then
		PKG_CHECK_MODULES([$2], [$3],
			[eval "found_$1=yes"],
			AC_CHECK_LIB($4, $5,
				[eval "found_$1=yes $2_LIBS='$6' $2_CFLAGS='$7'"],
				[eval "found_$1=no"],
				[$6]))
	fi

	MPD_AUTO_RESULT([$1], [$8], [$9])
])

dnl Wrapper for AC_CHECK_LIB.
dnl
dnl Parameters: varname1, varname2, libname, symname, libs, cflags, description, errmsg
AC_DEFUN([MPD_AUTO_LIB], [
	AC_SUBST([$2_LIBS], [])
	AC_SUBST([$2_CFLAGS], [])

	if eval "test x`echo '$'enable_$1` != xno"; then
		AC_CHECK_LIB($3, $4,
			[eval "found_$1=yes $2_LIBS='$5' $2_CFLAGS='$6'"],
			[eval "found_$1=no"],
			[$5])
	fi

	MPD_AUTO_RESULT([$1], [$7], [$8])
])
