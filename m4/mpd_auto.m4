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
	else
		AC_MSG_ERROR([$msg])
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
