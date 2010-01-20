dnl
dnl Usage:
dnl AC_CHECK_LIBWRAP([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl

AC_DEFUN([AC_CHECK_LIBWRAP],[
	AC_CHECK_HEADERS([tcpd.h],
		AC_CHECK_LIB([wrap], [request_init],
			[LIBWRAP_CFLAGS=""
			LIBWRAP_LDFLAGS="-lwrap"
			$1],
			$2),
		$2)
])
