dnl Run code with the specified CFLAGS/CXXFLAGS and LIBS appended.
dnl Restores the old values afterwards.
dnl
dnl Parameters: cflags, libs, code
AC_DEFUN([MPD_WITH_FLAGS], [
	ac_save_CFLAGS="$[]CFLAGS"
	ac_save_CXXFLAGS="$[]CXXFLAGS"
	ac_save_LIBS="$[]LIBS"
	CFLAGS="$[]CFLAGS $1"
	CXXFLAGS="$[]CXXFLAGS $1"
	LIBS="$[]LIBS $2"
	$3
	CFLAGS="$[]ac_save_CFLAGS"
	CXXFLAGS="$[]ac_save_CXXFLAGS"
	LIBS="$[]ac_save_LIBS"
])

dnl Run code with the specified library's CFLAGS/CXXFLAGS and LIBS
dnl appended.  Restores the old values afterwards.
dnl
dnl Parameters: libname, code
AC_DEFUN([MPD_WITH_LIBRARY],
	[MPD_WITH_FLAGS([$[]$1_CFLAGS], [$[]$1_LIBS], [$2])])
