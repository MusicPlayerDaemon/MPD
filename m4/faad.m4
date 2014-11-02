AC_DEFUN([AM_PATH_FAAD],
[dnl ##
dnl faad
dnl ##

AC_ARG_ENABLE(aac,
	AS_HELP_STRING([--disable-aac],
		[disable AAC support (default: enable)]),,
	enable_aac=yes)

if test x$enable_aac = xyes; then
	FAAD_LIBS="-lfaad"
	FAAD_CFLAGS=""

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $FAAD_CFLAGS"
	LIBS="$LIBS $FAAD_LIBS"
	CPPFLAGS=$CFLAGS
	AC_CHECK_HEADER(faad.h,,enable_aac=no)
	if test x$enable_aac = xyes; then
		AC_CHECK_DECL(FAAD2_VERSION,,enable_aac=no,[#include <faad.h>])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_LIB(faad,NeAACDecInit2,,enable_aac=no)
	fi
	if test x$enable_aac = xyes; then
		AC_MSG_CHECKING(that FAAD2 can even be used)
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <faad.h>

int main() {
	char buffer;
	NeAACDecHandle decoder;
	NeAACDecFrameInfo frameInfo;
	NeAACDecConfigurationPtr config;
	unsigned char channels;
	long sampleRate;
	long bufferlen = 0;

	decoder = NeAACDecOpen();
	config = NeAACDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
	NeAACDecSetConfiguration(decoder,config);
	NeAACDecInit(decoder,&buffer,bufferlen,&sampleRate,&channels);
	NeAACDecInit2(decoder,&buffer,bufferlen,&sampleRate,&channels);
	NeAACDecDecode(decoder,&frameInfo,&buffer,bufferlen);
	NeAACDecClose(decoder);

	return 0;
}
])],AC_MSG_RESULT(yes),[AC_MSG_RESULT(no);enable_aac=no])
	fi
	if test x$enable_aac = xyes; then
		AC_DEFINE(HAVE_FAAD,1,[Define to use FAAD2 for AAC decoding])
	else
		AC_MSG_WARN([faad2 lib needed for MP4/AAC support -- disabling MP4/AAC support])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
fi

if test x$enable_aac = xno; then
	FAAD_LIBS=""
	FAAD_CFLAGS=""
fi

AC_SUBST(FAAD_CFLAGS)
AC_SUBST(FAAD_LIBS)

])
