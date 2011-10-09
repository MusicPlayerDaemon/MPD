AC_DEFUN([AM_PATH_FAAD],
[dnl ##
dnl faad
dnl ##

AC_ARG_ENABLE(aac,
	AS_HELP_STRING([--disable-aac],
		[disable AAC support (default: enable)]),,
	enable_aac=yes)

AC_ARG_WITH(faad,
	AS_HELP_STRING([--with-faad=PFX],
		[prefix where faad2 is installed (optional)]),,
	faad_prefix="")
AC_ARG_WITH(faad-libraries,
	AS_HELP_STRING([--with-faad-libraries=DIR],
		[directory where faad2 library is installed (optional)]),,
	faad_libraries="")
AC_ARG_WITH(faad-includes,
	AS_HELP_STRING([--with-faad-includes=DIR],
		[directory where faad2 header files are installed (optional)]),,
	faad_includes="")

if test x$enable_aac = xyes; then
	if test "x$faad_libraries" != "x" ; then
		FAAD_LIBS="-L$faad_libraries"
	elif test "x$faad_prefix" != "x" ; then
		FAAD_LIBS="-L$faad_prefix/lib"
	fi

	FAAD_LIBS="$FAAD_LIBS -lfaad"

	if test "x$faad_includes" != "x" ; then
		FAAD_CFLAGS="-I$faad_includes"
	elif test "x$faad_prefix" != "x" ; then
		FAAD_CFLAGS="-I$faad_prefix/include"
	fi

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $FAAD_CFLAGS -I."
	LIBS="$LIBS $FAAD_LIBS"
	CPPFLAGS=$CFLAGS
	AC_CHECK_HEADER(faad.h,,enable_aac=no)
	if test x$enable_aac = xyes; then
		AC_CHECK_DECL(FAAD2_VERSION,,enable_aac=no,[#include <faad.h>])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_DECL(faacDecInit2,,enable_aac=no,[#include <faad.h>])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_LIB(faad,faacDecInit2,,enable_aac=no)
		if test x$enable_aac = xno; then
			enable_aac=yes
			AC_CHECK_LIB(faad,NeAACDecInit2,,enable_aac=no)
		fi
	fi
	if test x$enable_aac = xyes; then
  		AC_MSG_CHECKING(that FAAD2 uses buffer and bufferlen)
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <faad.h>

int main() {
	char buffer;
	long bufferlen = 0;
	faacDecHandle decoder;
	faacDecFrameInfo frameInfo;
	faacDecConfigurationPtr config;
	unsigned char channels;
	long sampleRate;
        mp4AudioSpecificConfig mp4ASC;

	decoder = faacDecOpen();
	config = faacDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
	faacDecSetConfiguration(decoder,config);
	AudioSpecificConfig(&buffer, bufferlen, &mp4ASC);
	faacDecInit(decoder,&buffer,bufferlen,&sampleRate,&channels);
	faacDecInit2(decoder,&buffer,bufferlen,&sampleRate,&channels);
	faacDecDecode(decoder,&frameInfo,&buffer,bufferlen);

	return 0;
}
])],[AC_MSG_RESULT(yes);AC_DEFINE(HAVE_FAAD_BUFLEN_FUNCS,1,[Define if FAAD2 uses buflen in function calls])],[AC_MSG_RESULT(no);
		AC_MSG_CHECKING(that FAAD2 can even be used)
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <faad.h>

int main() {
	char buffer;
	faacDecHandle decoder;
	faacDecFrameInfo frameInfo;
	faacDecConfigurationPtr config;
	unsigned char channels;
	long sampleRate;
	long bufferlen = 0;
	unsigned long dummy1_32;
        unsigned char dummy2_8, dummy3_8, dummy4_8, dummy5_8, dummy6_8,
                                dummy7_8, dummy8_8;

	decoder = faacDecOpen();
	config = faacDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
	faacDecSetConfiguration(decoder,config);
	AudioSpecificConfig(&buffer,&dummy1_32,&dummy2_8,
                                &dummy3_8,&dummy4_8,&dummy5_8,
                                &dummy6_8,&dummy7_8,&dummy8_8);
	faacDecInit(decoder,&buffer,&sampleRate,&channels);
	faacDecInit2(decoder,&buffer,bufferlen,&sampleRate,&channels);
	faacDecDecode(decoder,&frameInfo,&buffer);
	faacDecClose(decoder);

	return 0;
}
])],AC_MSG_RESULT(yes),[AC_MSG_RESULT(no);enable_aac=no])
		])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_MEMBERS([faacDecConfiguration.downMatrix,faacDecConfiguration.dontUpSampleImplicitSBR,faacDecFrameInfo.samplerate],,,[#include <faad.h>])
		AC_DEFINE(HAVE_FAAD,1,[Define to use FAAD2 for AAC decoding])
	else
		AC_MSG_WARN([faad2 lib needed for MP4/AAC support -- disabling MP4/AAC support])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
fi

if test x$enable_aac = xyes; then
	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $FAAD_CFLAGS -Werror"
	LIBS="$LIBS $FAAD_LIBS"
	CPPFLAGS=$CFLAGS

	AC_MSG_CHECKING(for broken libfaad headers)
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <faad.h>
#include <stddef.h>
#include <stdint.h>

int main() {
	unsigned char channels;
	uint32_t sample_rate;

	faacDecInit2(NULL, NULL, 0, &sample_rate, &channels);
	return 0;
}
	])],
		[AC_MSG_RESULT(correct)],
		[AC_MSG_RESULT(broken);
		AC_DEFINE(HAVE_FAAD_LONG, 1, [Define if faad.h uses the broken "unsigned long" pointers])])

	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
fi

if test x$enable_aac = xyes; then
	enable_mp4=yes
	MP4FF_LIBS="-lmp4ff"

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $FAAD_CFLAGS"
	LIBS="$LIBS $FAAD_LIBS $MP4FF_LIBS"
	CPPFLAGS=$CFLAGS

	AC_CHECK_HEADER(mp4ff.h,,enable_mp4=no)

	if test x$enable_mp4 = xyes; then
		AC_CHECK_LIB(mp4ff,mp4ff_open_read,,enable_mp4=no)
	fi

	if test x$enable_mp4 = xyes; then
		AC_SUBST(MP4FF_LIBS)
		AC_DEFINE(HAVE_MP4, 1, [Define to use FAAD2+mp4ff for MP4 decoding])
	else
		AC_MSG_WARN([libmp4ff needed for MP4 support -- disabling MP4 support])
		unset MP4FF_LIBS
	fi

	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
else
	enable_mp4=no
	FAAD_CFLAGS=""
	FAAD_LIBS=""
fi

AC_SUBST(FAAD_CFLAGS)
AC_SUBST(FAAD_LIBS)

])
