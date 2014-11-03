AC_DEFUN([MPD_DEPENDS], [
	if test x$$2 = xno; then
		if test x$$1 = xauto; then
			$1=no
		elif test x$$1 = xyes; then
			AC_MSG_ERROR([$3])
		fi
	fi
])
