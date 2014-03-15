# Check if "struct ucred" is available.
#
# Author: Max Kellermann <max@duempel.org>

AC_DEFUN([STRUCT_UCRED],[
	AC_MSG_CHECKING([for struct ucred])
	AC_CACHE_VAL(mpd_cv_have_struct_ucred, [
		AC_TRY_COMPILE([#include <sys/socket.h>],
			[struct ucred cred;],
			mpd_cv_have_struct_ucred=yes,
			mpd_cv_have_struct_ucred=no)
		])

	AC_MSG_RESULT($mpd_cv_have_struct_ucred)
	if test x$mpd_cv_have_struct_ucred = xyes; then
		AC_DEFINE(HAVE_STRUCT_UCRED, 1, [Define if struct ucred is present from sys/socket.h])
	fi
])
