dnl
dnl Usage:
dnl AC_CHECK_LIBWRAP([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl

AC_DEFUN([AC_CHECK_LIBWRAP],
         [dnl start

    AC_ARG_ENABLE([libwrap],
        [AS_HELP_STRING([--disable-libwrap],
        [use libwrap (default enabled)])], ,
        [
           AC_CHECK_HEADERS([tcpd.h],
                           [],
                           [AC_MSG_ERROR([tpcd.h libwrap header not found])]
                           $3)

           AC_CHECK_LIB([wrap],
                        [request_init],
                        [],
                        [AC_MSG_ERROR([libwrap not found !])]
                        $3)

           AC_DEFINE(HAVE_LIBWRAP, 1, [define to enable libwrap library])

           LIBWRAP_CFLAGS=""
           LIBWRAP_LDFLAGS="-lwrap"

           AC_SUBST([LIBWRAP_CFLAGS])
           AC_SUBST([LIBWRAP_LDFLAGS])

           dnl ACTION-IF-FOUND
           $2

        ]) dnl AC_ARG_ENABLE

]) dnl AC_DEFUN
