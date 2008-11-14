AC_PREREQ(2.60)
AC_INIT(mpd, 0.14~alpha3, musicpd-dev-team@lists.sourceforge.net)
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([foreign 1.9 dist-bzip2])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST(MPD_LIBS)
AC_SUBST(MPD_CFLAGS)

AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_DEFINE(PROTOCOL_VERSION, "0.14.0", [The mpd protocol version])

MPD_LIBS=""
MPD_CFLAGS=""

if test -z "$prefix" || test "x$prefix" = xNONE; then
	local_lib=
	local_include=

	# aren't autotools supposed to be smart enough to figure this out?  oh
	# well, the git-core Makefile managed to do some of the work for us :)
	case "$host_os" in
	darwin*)
		local_lib='/sw/lib /opt/local/lib'
		local_include='/sw/include /opt/local/include'
		;;
	freebsd* | openbsd*)
		local_lib=/usr/local/lib
		local_include=/usr/local/include
		;;
	netbsd*)
		local_lib=/usr/pkg/lib
		local_include=/usr/pkg/include
		LDFLAGS="$LDFLAGS -Wl,-rpath,/usr/pkg/lib"
		;;
	esac

	for d in $local_lib; do
		if test -d "$d"; then
			LDFLAGS="$LDFLAGS -L$d"
			break
		fi
	done
	for d in $local_include; do
		if test -d "$d"; then
			CFLAGS="$CFLAGS -I$d"
			break
		fi
	done
fi


dnl
dnl build options
dnl

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],
		[Treat warnings as errors (default: disabled)]),
	ENABLE_WERROR=$enableval,
	ENABLE_WERROR=no)

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug],
		[Enable debugging (default: disabled)]),
	ENABLE_DEBUG=$enableval,
	ENABLE_DEBUG=no)

AC_ARG_ENABLE(gprof,
	AS_HELP_STRING([--enable-gprof],
		[Enable profiling via gprof (default: disabled)]),
	ENABLE_GPROF=$enableval,
	ENABLE_GPROF=no)


dnl
dnl protocol options
dnl

AC_ARG_ENABLE(ipv6,
	AS_HELP_STRING([--disable-ipv6],
		[disable IPv6 support (default: enable)]),
	[enable_ipv6=$enableval],
	[enable_ipv6=yes])

AC_ARG_ENABLE(tcp,
	AS_HELP_STRING([--disable-tcp],
		[disable support for clients connecting via TCP (default: enable)]),
	[enable_tcp=$enableval],
	[enable_tcp=yes])

AC_ARG_ENABLE(un,
	AS_HELP_STRING([--enable-un],
		[enable support for clients connecting via unix domain sockets (default: disable)]),
	[enable_un=$enableval],
	[enable_un=yes])


dnl
dnl input options
dnl

AC_ARG_ENABLE(curl,
	AS_HELP_STRING([--disable-curl],
		[enable support obtaining song data via HTTP (default: enable)]),
	[enable_curl=$enableval],
	[enable_curl=yes])


dnl
dnl audio output plugins
dnl

AC_ARG_ENABLE(ao,
	AS_HELP_STRING([--enable-ao],
		[enable support for libao (default: disable)]),
	enable_ao=$enableval,
	enable_ao=no)

AC_ARG_ENABLE(shout_ogg,
	AS_HELP_STRING([--disable-shout_ogg],
		[disable support for ogg streaming through shout (default: enable)]),
	[enable_shout_ogg=$enableval],
	[enable_shout_ogg=yes])

AC_ARG_ENABLE(shout_mp3,
	AS_HELP_STRING([--disable-shout_mp3],
		[disable support for mp3 streaming through shout (default: enable)]),
	[enable_shout_mp3=$enableval],
	[enable_shout_mp3=yes])

AC_ARG_ENABLE(oss,
	AS_HELP_STRING([--disable-oss],
		[disable OSS support (default: enable)]),
	enable_oss=$enableval,
	enable_oss=yes)

AC_ARG_ENABLE(alsa,
	AS_HELP_STRING([--disable-alsa],
		[disable ALSA support (default: enable)]),
	enable_alsa=$enableval,
	enable_alsa=yes)

AC_ARG_ENABLE(jack,
	AS_HELP_STRING([--disable-jack],
		[disable jack support (default: enable)]),
	enable_jack=$enableval,
	enable_jack=yes)

AC_ARG_ENABLE(pulse,
	AS_HELP_STRING([--disable-pulse],
		[disable support for the PulseAudio sound server (default: enable)]),
	enable_pulse=$enableval,
	enable_pulse=yes)

AC_ARG_ENABLE(fifo,
	AS_HELP_STRING([--disable-fifo],
		[disable support for writing audio to a FIFO (default: enable)]),
	enable_fifo=$enableval,
	enable_fifo=yes)

AC_ARG_ENABLE(mvp,
	AS_HELP_STRING([--enable-mvp],
		[enable support for Hauppauge Media MVP (default: disable)]),
	enable_mvp=$enableval,
	enable_mvp=no)


dnl
dnl decoder plugins
dnl

AC_ARG_ENABLE(oggvorbis,
	AS_HELP_STRING([--disable-oggvorbis],
		[disable Ogg Vorbis support (default: enable)]),
	enable_oggvorbis=$enableval,
	enable_oggvorbis=yes)

AC_ARG_ENABLE(oggflac,
	AS_HELP_STRING([--disable-oggflac],
		[disable OggFLAC support (default: enable)]),
	enable_oggflac=$enableval,
	enable_oggflac=yes)

AC_ARG_ENABLE(flac,
	AS_HELP_STRING([--disable-flac],
		[disable flac support (default: enable)]),
	enable_flac=$enableval,
	enable_flac=yes)

AC_ARG_ENABLE(mp3,
	AS_HELP_STRING([--disable-mp3],
		[disable mp3 support (default: enable)]),
	enable_mp3=$enableval,
	enable_mp3=yes)

AC_ARG_ENABLE(lame,
	AS_HELP_STRING([--disable-lame],
		[disable lame support (default: enable)]),
	enable_lame=$enableval,
	enable_lame=yes)

AC_ARG_ENABLE(aac,
	AS_HELP_STRING([--disable-aac],
		[disable AAC support (default: enable)]),
	enable_aac=$enableval,
	enable_aac=yes)

AC_ARG_ENABLE(audiofile,
	AS_HELP_STRING([--disable-audiofile],
		[disable audiofile support, disables wave support (default: enable)]),
	enable_audiofile=$enableval,
	enable_audiofile=yes)

AC_ARG_ENABLE(mod,
	AS_HELP_STRING([--enable-mod],
		[enable MOD support (default: disable)]),
	enable_mod=$enableval,
	enable_mod=yes)

AC_ARG_ENABLE(mpc,
	AS_HELP_STRING([--disable-mpc],
		[disable musepack (MPC) support (default: enable)]),
	enable_mpc=$enableval,
	enable_mpc=yes)

AC_ARG_ENABLE(wavpack,
	AS_HELP_STRING([--disable-wavpack],
		[disable WavPack support (default: enable)]),
	enable_wavpack=$enableval,
	enable_wavpack=yes)

AC_ARG_ENABLE(ffmpeg,
	AS_HELP_STRING([--disable-ffmpeg],
		[enable FFMPEG support (default: enable)]),
	enable_ffmpeg=$enableval,
	enable_ffmpeg=yes)

AC_ARG_ENABLE(id3,
	AS_HELP_STRING([--disable-id3],
		[disable id3 support (default: enable)]),
	enable_id3=$enableval,
	enable_id3=yes)

AC_ARG_ENABLE(lsr,
	AS_HELP_STRING([--disable-lsr],
		[disable libsamplerate support (default: enable)]),
	enable_lsr=$enableval,
	enable_lsr=yes)


AC_ARG_WITH(tremor,[[  --with-tremor[=PFX]         Use Tremor(vorbisidec) integer Ogg-Vorbis decoder (with optional prefix)]], use_tremor=yes; test x$withval != xyes && tremor_prefix="$withval",)
AC_ARG_WITH(tremor-libraries,[  --with-tremor-libraries=DIR Directory where Tremor library is installed (optional)], tremor_libraries="$withval", tremor_libraries="")
AC_ARG_WITH(tremor-includes,[  --with-tremor-includes=DIR  Directory where Tremor header files are installed (optional)], tremor_includes="$withval", tremor_includes="")

AC_ARG_WITH(faad,[  --with-faad=PFX             Prefix where faad2 is installed], faad_prefix="$withval", faad_prefix="")
AC_ARG_WITH(faad-libraries,[  --with-faad-libraries=DIR   Directory where faad2 library is installed (optional)], faad_libraries="$withval", faad_libraries="")
AC_ARG_WITH(faad-includes,[  --with-faad-includes=DIR    Directory where faad2 header files are installed (optional)], faad_includes="$withval", faad_includes="")
AC_ARG_WITH(zeroconf,[[  --with-zeroconf=[auto|avahi|bonjour|no]    Enable zeroconf backend (default=auto)]], with_zeroconf="$withval", with_zeroconf="auto")

AC_C_BIGENDIAN

AC_CHECK_HEADER(sys/inttypes.h,AC_DEFINE(HAVE_SYS_INTTYPES_H,1,[Define if sys/inttypes.h present]),)

AC_CHECK_LIB(socket,socket,MPD_LIBS="$MPD_LIBS -lsocket",)
AC_CHECK_LIB(nsl,gethostbyname,MPD_LIBS="$MPD_LIBS -lnsl",)

AC_CHECK_LIB(m,exp,MPD_LIBS="$MPD_LIBS -lm",)
AC_CHECK_FUNCS(setenv)

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.4 gthread-2.0],,
		  [AC_MSG_ERROR([glib-2.4 is required])])

dnl doesn't work for systems that don't have CODESET like OpenBSD
dnl AC_CHECK_HEADER(langinfo.h,[enable_langinfo=yes;AC_DEFINE(HAVE_LANGINFO,1,[Define if nl_langinfo.h is present])],enable_langinfo=no)
AM_LANGINFO_CODESET
AC_CHECK_HEADER(locale.h,[enable_locale=yes;AC_DEFINE(HAVE_LOCALE,1,[Define if locale.h is present])],enable_locale=no)

ACX_PTHREAD([CC="$PTHREAD_CC"
             LIBS="$PTHREAD_LIBS $LIBS"
             CFLAGS="$PTHREAD_CFLAGS $CFLAGS"])

if test x$enable_tcp = xno; then
	# if we don't support TCP, we don't need IPv6 either
	enable_ipv6=no
fi

if test x$enable_ipv6 = xyes; then
	AC_MSG_CHECKING(for ipv6)
	AC_EGREP_CPP([AP_maGiC_VALUE],
	[
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#ifdef PF_INET6
#ifdef AF_INET6
AP_maGiC_VALUE
#endif
#endif
	],
	AC_DEFINE(HAVE_IPV6, 1, [Define if IPv6 support present])
	AC_MSG_RESULT([yes]),
	AC_MSG_RESULT([no])
)
fi

if test x$enable_tcp = xyes; then
	AC_DEFINE(HAVE_TCP, 1, [Define if TCP socket support is enabled])
fi

if test x$enable_un = xyes; then
	AC_DEFINE(HAVE_UN, 1, [Define if unix domain socket support is enabled])
	STRUCT_UCRED
fi

enable_osx=no
case $host in
	*-darwin*)
                AC_DEFINE(HAVE_OSX, 1, [Define for compiling OS X support])
		MPD_LIBS="$MPD_LIBS -framework AudioUnit -framework CoreServices"
		enable_osx=yes ;;
esac

AM_CONDITIONAL(HAVE_OSX, test x$enable_osx = xyes)

if test x$enable_curl = xyes; then
	PKG_CHECK_MODULES(CURL, [libcurl],
		AC_DEFINE(HAVE_CURL, 1, [Define when libcurl is used for HTTP streaming]),
		enable_curl=no)
fi
AM_CONDITIONAL(HAVE_CURL, test x$enable_curl = xyes)

if test x$enable_shout_ogg = xyes || test x$enable_shout_mp3 = xyes; then
	enable_shout=yes
	PKG_CHECK_MODULES([SHOUT], [shout],
		AC_DEFINE(HAVE_SHOUT, 1, [Define to enable libshout support]),
		enable_shout=no)
fi

if test x$enable_shout_ogg = xyes; then
	if test x$enable_oggvorbis = xno; then
		AC_MSG_WARN([disabling ogg shout streaming support because vorbis is not enabled])
		enable_shout_ogg=no
	fi
	if test x$use_tremor = xyes; then
		AC_MSG_WARN([disabling ogg shout streaming support because tremor does not support vorbis encoding])
		enable_shout_ogg=no
	fi
	if test x$enable_shout = xno; then
		AC_MSG_WARN([disabling ogg shout streaming support because libshout is not found])
		enable_shout_ogg=no
	fi
	if test x$enable_shout_ogg = xyes; then
		PKG_CHECK_MODULES(VORBISENC, [vorbisenc],
			AC_DEFINE(HAVE_SHOUT_OGG, 1, [Define to enable ogg streaming support]),
			enable_shout_ogg=no)
	fi
fi

if test x$enable_shout_mp3 = xyes; then
	if test x$enable_lame = xno; then
		AC_MSG_WARN([disabling mp3 shout streaming support because lame is not enabled])
		enable_shout_mp3=no
	fi
	if test x$enable_shout = xno; then
		AC_MSG_WARN([disabling mp3 shout streaming support because libshout is not found])
		enable_shout_mp3=no
	fi
	if test x$enable_shout_mp3 = xyes; then
		AC_DEFINE(HAVE_SHOUT_MP3, 1, [Define to enable mp3 streaming support])
	fi
fi

AM_CONDITIONAL(HAVE_SHOUT, test x$enable_shout = xyes)
AM_CONDITIONAL(HAVE_SHOUT_OGG, test x$enable_shout_ogg = xyes)
AM_CONDITIONAL(HAVE_SHOUT_MP3, test x$enable_shout_mp3 = xyes)

if test x$enable_ao = xyes; then
	PKG_CHECK_MODULES(AO, [ao],
		AC_DEFINE(HAVE_AO, 1, [Define to play with ao]),
		enable_ao=no)
fi

AM_CONDITIONAL(HAVE_AO, test x$enable_ao = xyes)

if test x$enable_oss = xyes; then
        AC_CHECK_HEADER(sys/soundcard.h,[enable_oss=yes;AC_DEFINE(HAVE_OSS,1,[Define to enable OSS])],[AC_MSG_WARN(Soundcard headers not found -- disabling OSS support);enable_oss=no])
fi

AM_CONDITIONAL(HAVE_OSS, test x$enable_oss = xyes)

PKG_PROG_PKG_CONFIG

if test x$enable_pulse = xyes; then
	PKG_CHECK_MODULES([PULSE], [libpulse-simple],
	                  [enable_pulse=yes;AC_DEFINE([HAVE_PULSE], 1, [Define to enable PulseAudio support])] MPD_LIBS="$MPD_LIBS $PULSE_LIBS" MPD_CFLAGS="$MPD_CFLAGS $PULSE_CFLAGS",
	                  [enable_pulse=no;AC_MSG_WARN([PulseAudio not found -- disabling])])
fi

AM_CONDITIONAL(HAVE_PULSE, test x$enable_pulse = xyes)

if test x$enable_lsr = xyes; then
	PKG_CHECK_MODULES([SAMPLERATE], [samplerate >= 0.0.15],
	                  [enable_lsr=yes;AC_DEFINE([HAVE_LIBSAMPLERATE], 1, [Define to enable libsamplerate])] MPD_LIBS="$MPD_LIBS $SAMPLERATE_LIBS" MPD_CFLAGS="$MPD_CFLAGS $SAMPLERATE_CFLAGS",
	                  [enable_lsr=no;AC_MSG_WARN([libsamplerate not found -- disabling])])
fi

if test x$enable_lsr = xyes; then
	PKG_CHECK_MODULES([SAMPLERATE_013], [samplerate >= 0.1.3],,
		[AC_DEFINE([HAVE_LIBSAMPLERATE_NOINT], 1, [libsamplerate doesn't provide src_int_to_float_array() (<0.1.3)])])
fi

AM_CONDITIONAL(HAVE_LIBSAMPLERATE, test x$enable_lsr = xyes)

if test x$enable_fifo = xyes; then
	AC_CHECK_FUNC([mkfifo],
	              [enable_fifo=yes;AC_DEFINE([HAVE_FIFO], 1, [Define to enable support for writing audio to a FIFO])],
	              [enable_fifo=no;AC_MSG_WARN([mkfifo not found -- disabling support for writing audio to a FIFO])])
fi

AM_CONDITIONAL(HAVE_FIFO, test x$enable_fifo = xyes)

if test x$enable_mvp = xyes; then
   AC_DEFINE(HAVE_MVP,1,[Define to enable Hauppauge Media MVP support])
fi

AM_CONDITIONAL(HAVE_MVP, test x$enable_mvp = xyes)

if test x$enable_alsa = xyes; then
	PKG_CHECK_MODULES(ALSA, [alsa >= 0.9.0],
		AC_DEFINE(HAVE_ALSA, 1, [Define to enable ALSA support]),
		enable_alsa=no)
fi

AM_CONDITIONAL(HAVE_ALSA, test x$enable_alsa = xyes)

if test x$enable_jack = xyes; then
	PKG_CHECK_MODULES([JACK], [jack >= 0.4],
	                  [enable_jack=yes;AC_DEFINE([HAVE_JACK], 1, [Define to enable JACK support])] MPD_LIBS="$MPD_LIBS $JACK_LIBS" MPD_CFLAGS="$MPD_CFLAGS $JACK_CFLAGS",
	                  [enable_jack=no;AC_MSG_WARN([JACK not found -- disabling])])
fi

AM_CONDITIONAL(HAVE_JACK, test x$enable_jack = xyes)

if test x$enable_id3 = xyes; then
	PKG_CHECK_MODULES([ID3TAG], [id3tag],
		AC_DEFINE(HAVE_ID3TAG, 1, [Define to use id3tag]),
		enable_id3=no)
fi

AM_CONDITIONAL(HAVE_ID3TAG, test x$enable_id3 = xyes)

if test x$enable_mp3 = xyes; then
	PKG_CHECK_MODULES([MAD], [mad],
		AC_DEFINE(HAVE_MAD, 1, [Define to use libmad]),
		enable_mp3=no)
fi

AM_CONDITIONAL(HAVE_MAD, test x$enable_mp3 = xyes)

if test x$enable_lame = xyes; then
	AM_PATH_LAME([MPD_LIBS="$MPD_LIBS $LAME_LIBS" MPD_CFLAGS="$MPD_CFLAGS $LAME_CFLAGS"],
	             [enable_lame=no;AC_MSG_WARN(You need lame -- disabling lame support)])
fi


if test x$enable_mpc = xyes; then
  	if test "x$mpcdec_libraries" != "x" ; then
  		MPCDEC_LIBS="-L$mpcdec_libraries"
  	elif test "x$mpcdec_prefix" != "x" ; then
		MPCDEC_LIBS="-L$mpcdec_prefix/lib"
  	fi

 	MPCDEC_LIBS="$MPCDEC_LIBS -lmpcdec"

  	if test "x$mpcdec_includes" != "x" ; then
  		MPCDEC_CFLAGS="-I$mpcdec_includes"
  	elif test "x$mpcdec_prefix" != "x" ; then
    		MPCDEC_CFLAGS="-I$mpcdec_prefix/include"
  	fi

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $MPD_CFLAGS $MPCDEC_CFLAGS -I."
	LIBS="$LIBS $MPD_LIBS $MPCDEC_LIBS"
	CPPFLAGS=$CFLAGS
	AC_CHECK_HEADER(mpcdec/mpcdec.h,,enable_mpc=no)
	if test x$enable_mpc = xyes; then
		AC_CHECK_LIB(mpcdec,main,[MPD_LIBS="$MPD_LIBS $MPCDEC_LIBS";MPD_CFLAGS="$MPD_CFLAGS $MPCDEC_CFLAGS";],enable_mpc=no)
	fi
	if test x$enable_mpc = xyes; then
		AC_DEFINE(HAVE_MPCDEC,1,[Define to use libmpcdec for MPC decoding])
	else
		AC_MSG_WARN([mpcdec lib needed for MPC support -- disabling MPC support])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
fi

AM_CONDITIONAL(HAVE_MPCDEC, test x$enable_mpc = xyes)

if test x$enable_wavpack = xyes; then
	PKG_CHECK_MODULES([WAVPACK], [wavpack],
	                  [enable_wavpack=yes;AC_DEFINE([HAVE_WAVPACK], 1, [Define to enable WavPack support])] MPD_LIBS="$MPD_LIBS $WAVPACK_LIBS" MPD_CFLAGS="$MPD_CFLAGS $WAVPACK_CFLAGS",
	                  [enable_wavpack=no;AC_MSG_WARN([WavPack not found -- disabling])])
fi

AM_CONDITIONAL(HAVE_WAVPACK, test x$enable_wavpack = xyes)

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
	CFLAGS="$CFLAGS $MPD_CFLAGS $FAAD_CFLAGS -I."
	LIBS="$LIBS $MPD_LIBS $FAAD_LIBS"
	CPPFLAGS=$CFLAGS
	AC_CHECK_HEADER(faad.h,,enable_aac=no)
	if test x$enable_aac = xyes; then
		AC_CHECK_DECL(FAAD2_VERSION,,enable_aac=no,[#include <faad.h>])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_DECL(faacDecInit2,,enable_aac=no,[#include <faad.h>])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_LIB(faad,faacDecInit2,[MPD_LIBS="$MPD_LIBS $FAAD_LIBS";MPD_CFLAGS="$MPD_CFLAGS $FAAD_CFLAGS"],enable_aac=no)
		if test x$enable_aac = xno; then
			enable_aac=yes
			AC_CHECK_LIB(faad,NeAACDecInit2,[MPD_LIBS="$MPD_LIBS $FAAD_LIBS";MPD_CFLAGS="$MPD_CFLAGS $FAAD_CFLAGS"],enable_aac=no)
		fi
	fi
	if test x$enable_aac = xyes; then
  		AC_MSG_CHECKING(that FAAD2 uses buffer and bufferlen)
		AC_COMPILE_IFELSE([
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
],[AC_MSG_RESULT(yes);AC_DEFINE(HAVE_FAAD_BUFLEN_FUNCS,1,[Define if FAAD2 uses buflen in function calls])],[AC_MSG_RESULT(no);
		AC_MSG_CHECKING(that FAAD2 can even be used)
		AC_COMPILE_IFELSE([
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
],AC_MSG_RESULT(yes),[AC_MSG_RESULT(no);enable_aac=no])
		])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_TYPES(mp4AudioSpecificConfig,,,[#include <faad.h>])
		AC_CHECK_MEMBERS([faacDecConfiguration.downMatrix,faacDecConfiguration.dontUpSampleImplicitSBR,faacDecFrameInfo.samplerate],,,[#include <faad.h>])
		AC_DEFINE(HAVE_FAAD,1,[Define to use FAAD2 for AAC decoding])
	else
		AC_MSG_WARN([faad2 lib needed for MP4/AAC support -- disabling MP4/AAC support])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
fi

AM_CONDITIONAL(HAVE_FAAD, test x$enable_aac = xyes)

if test x$enable_aac = xyes; then
	enable_mp4=yes
	MP4FF_LIBS="-lmp4ff"

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $FAAD_CFLAGS"
	LIBS="$LIBS $MP4FF_LIBS"
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
fi

AM_CONDITIONAL(HAVE_MP4, test x$enable_mp4 = xyes)

if test x$use_tremor = xyes; then
	if test "x$tremor_libraries" != "x" ; then
		TREMOR_LIBS="-L$tremor_libraries"
	elif test "x$tremor_prefix" != "x" ; then
		TREMOR_LIBS="-L$tremor_prefix/lib"
	fi
	TREMOR_LIBS="$TREMOR_LIBS -lvorbisidec"
	if test "x$tremor_includes" != "x" ; then
		TREMOR_CFLAGS="-I$tremor_includes"
	elif test "x$tremor_prefix" != "x" ; then
		TREMOR_CFLAGS="-I$tremor_prefix/include"
	fi
	ac_save_CFLAGS="$CFLAGS"
	ac_save_LIBS="$LIBS"
	CFLAGS="$CFLAGS $TREMOR_CFLAGS"
	LIBS="$LIBS $TREMOR_LIBS"
	AC_CHECK_LIB(vorbisidec,ov_read,enable_oggvorbis=yes,enable_oggvorbis=no;AC_MSG_WARN([vorbisidec lib needed for ogg support with tremor -- disabling ogg support]))
	if test x$enable_oggvorbis = xno; then
		CFLAGS="$ac_save_CFLAGS"
		LIBS="$ac_save_LIBS"
	fi
elif test x$enable_oggvorbis = xyes; then
	PKG_CHECK_MODULES(OGGVORBIS, [ogg vorbis vorbisfile],
		AC_DEFINE(HAVE_OGGVORBIS, 1, [Define for Ogg Vorbis support]),
		enable_oggvorbis=no)
fi

AM_CONDITIONAL(HAVE_OGGVORBIS, test x$enable_oggvorbis = xyes)

if test x$use_tremor = xyes; then
	AC_DEFINE(HAVE_TREMOR,1,[Define to use tremor (libvorbisidec) for ogg support])
	if test x$enable_oggflac = xyes; then
		AC_MSG_WARN([disabling OggFLAC support because it is incompatible with tremor])
		enable_oggflac=no
	fi
fi

if test x$enable_flac = xyes; then
	PKG_CHECK_MODULES(FLAC, [flac >= 1.1],
		AC_DEFINE(HAVE_FLAC, 1, [Define for FLAC support]),
		enable_flac=no)

	oldcflags="$CFLAGS"
	oldlibs="$LIBS"
	CFLAGS="$CFLAGS $FLAC_CFLAGS"
	LIBS="$LIBS $FLAC_LIBS"
	if test x$enable_flac = xyes && test x$enable_oggflac = xyes; then
		AC_CHECK_DECL(FLAC_API_SUPPORTS_OGG_FLAC,
		              [enable_oggflac=flac], [],
			      [#include <FLAC/export.h>])
	fi
	CFLAGS="$oldcflags"
	LIBS="$oldlibs"
fi

AM_CONDITIONAL(HAVE_FLAC, test x$enable_flac = xyes)

if test x$enable_oggflac = xyes; then
	oldmpdcflags="$MPD_CFLAGS"
	oldmpdlibs="$MPD_LIBS"
	AM_PATH_LIBOGGFLAC(MPD_LIBS="$MPD_LIBS $LIBOGGFLAC_LIBS" MPD_CFLAGS="$MPD_CFLAGS $LIBOGGFLAC_CFLAGS",enable_oggflac=no)
fi

if test x$enable_oggflac = xyes; then
	AC_DEFINE(HAVE_OGGFLAC,1,[Define for OggFLAC support])
fi

AM_CONDITIONAL(HAVE_OGGFLAC, test x$enable_oggflac = xyes)

AM_CONDITIONAL(HAVE_FLAC_COMMON, test x$enable_flac = xyes || test x$enable_oggflac = xyes)
AM_CONDITIONAL(HAVE_OGG_COMMON, test x$enable_oggvorbis = xyes || test x$enable_oggflac = xyes || test x$enable_flac = xyes)

if test x$enable_audiofile = xyes; then
	PKG_CHECK_MODULES(AUDIOFILE, [audiofile >= 0.1.7],
		AC_DEFINE(HAVE_AUDIOFILE, 1, [Define for audiofile support]),
		enable_audiofile=no)
fi

AM_CONDITIONAL(HAVE_AUDIOFILE, test x$enable_audiofile = xyes)

if test x$enable_mod = xyes; then
	AC_PATH_PROG(LIBMIKMOD_CONFIG, libmikmod-config)
	if test x$LIBMIKMOD_CONFIG != x ; then
		AC_SUBST(LIBMIKMOD_CFLAGS, `$LIBMIKMOD_CONFIG --cflags`)
		AC_SUBST(LIBMIKMOD_LIBS, `$LIBMIKMOD_CONFIG --libs`)
		AC_DEFINE(HAVE_MIKMOD, 1, [Define for mikmod support])
	else
		enable_mod=no
	fi
fi

AM_CONDITIONAL(HAVE_MIKMOD, test x$enable_mod = xyes)

if test x$enable_ffmpeg = xyes; then
	PKG_CHECK_MODULES(FFMPEG, [libavformat libavcodec libavutil],
		AC_DEFINE(HAVE_FFMPEG, 1, [Define for FFMPEG support]),
		enable_ffmpeg=no)
fi

if test x$enable_ffmpeg = xyes; then
	old_LIBS=$LIBS
	LIBS="$LIBS $FFMPEG_LIBS"
	AC_CHECK_LIB(avcodec, avcodec_decode_audio2,,
		enable_ffmpeg=no)
	LIBS=$old_LIBS
fi

if test x$enable_ffmpeg = xyes; then
	# prior to ffmpeg svn12865, you had to specify include files
	# without path prefix
	old_CPPCFLAGS=$CPPFLAGS
	CPPFLAGS="$CPPFLAGS $FFMPEG_CFLAGS"
	AC_CHECK_HEADER(libavcodec/avcodec.h,,
		AC_DEFINE(OLD_FFMPEG_INCLUDES, 1,
			[Define if avcodec.h instead of libavcodec/avcodec.h should be included]))
	CPPCFLAGS=$old_CPPFLAGS
fi

AM_CONDITIONAL(HAVE_FFMPEG, test x$enable_ffmpeg = xyes)

case $with_zeroconf in
no|avahi|bonjour)
	;;
*)
	with_zeroconf=auto
	;;
esac

if test x$with_zeroconf != xno; then
	if test x$with_zeroconf = xavahi || test x$with_zeroconf = xauto; then
		PKG_CHECK_MODULES([AVAHI], [avahi-client],
		                  [found_avahi=1;AC_DEFINE([HAVE_AVAHI], 1, [Define to enable Avahi Zeroconf support])]
		                  MPD_LIBS="$MPD_LIBS $AVAHI_LIBS" MPD_CFLAGS="$MPD_CFLAGS $AVAHI_CFLAGS",
		                  [found_avahi=0])
	fi

	if test x$found_avahi = x1; then
		with_zeroconf=avahi
	elif test x$with_zeroconf = xavahi; then
		AC_MSG_ERROR([Avahi support requested but not found])
	fi

	if test x$with_zeroconf = xbonjour || test x$with_zeroconf = xauto; then
		AC_CHECK_HEADER(dns_sd.h,
		                [found_bonjour=1;AC_DEFINE([HAVE_BONJOUR], 1, [Define to enable Bonjour Zeroconf support])],
		                [found_bonjour=0])
		AC_CHECK_LIB(dns_sd, DNSServiceRegister,
		             MPD_LIBS="$MPD_LIBS -ldns_sd")
	fi

	if test x$found_bonjour = x1; then
		with_zeroconf=bonjour
	elif test x$with_zeroconf = xbonjour; then
		AC_MSG_ERROR([Bonjour support requested but not found])
	fi

	if test x$with_zeroconf = xauto; then
		AC_MSG_WARN([No supported Zeroconf backend found, disabling Zeroconf])
		with_zeroconf=no
	else
		AC_DEFINE([HAVE_ZEROCONF], 1, [Define to enable Zeroconf support])
	fi
fi

AM_CONDITIONAL(HAVE_ZEROCONF, test x$with_zeroconf != xno)


dnl
dnl build options
dnl

if test "x$ENABLE_WERROR" = xyes; then
	MPD_CFLAGS="$MPD_CFLAGS -Werror -pedantic-errors"
fi

#if test "x$ENABLE_DEBUG" = xno; then
	# don't set NDEBUG for now, until MPD is stable
	#MPD_CFLAGS="$MPD_CFLAGS -DNDEBUG"
#fi

if test "x$ENABLE_GPROF" = xyes; then
	MPD_CFLAGS="$MPD_CFLAGS -pg"
fi


dnl
dnl CFLAGS
dnl

if test x$GCC = xyes
then
	MPD_CHECK_FLAG([-Wall])
	MPD_CHECK_FLAG([-Wextra])
	MPD_CHECK_FLAG([-Wno-deprecated-declarations])
	MPD_CHECK_FLAG([-Wmissing-prototypes])
	MPD_CHECK_FLAG([-Wdeclaration-after-statement])
	MPD_CHECK_FLAG([-Wshadow])
	MPD_CHECK_FLAG([-Wpointer-arith])
	MPD_CHECK_FLAG([-Wstrict-prototypes])
	MPD_CHECK_FLAG([-Wcast-qual])
	MPD_CHECK_FLAG([-Wwrite-strings])
	MPD_CHECK_FLAG([-pedantic])
fi


dnl
dnl generate files
dnl


AC_OUTPUT(doc/Makefile src/Makefile Makefile)


dnl
dnl pretty-print result
dnl

echo ""
echo "########### MPD CONFIGURATION ############"
echo ""

echo " Client Support:"
if test x$enable_tcp = xyes; then
	echo " TCP support ...................enabled"
else
	echo " TCP support ...................disabled"
fi

if test x$enable_ipv6 = xyes; then
	echo " IPv6 support ..................enabled"
else
	echo " IPv6 support ..................disabled"
fi

if test x$enable_un = xyes; then
	echo " Unix domain socket support ....enabled"
else
	echo " Unix domain socket support ....disabled"
fi

echo ""

if
	test x$enable_tcp = xno &&
	test x$enable_un = xno; then
	AC_MSG_ERROR([No client interfaces configured!])
fi

echo " Playback Support:"
if test x$enable_ao = xyes; then
	echo " libao support .................enabled"
else
	echo " libao support .................disabled"
fi

if test x$enable_oss = xyes; then
	echo " OSS support ...................enabled"
else
	echo " OSS support ...................disabled"
fi

if test x$enable_alsa = xyes; then
	echo " ALSA support ..................enabled"
else
	echo " ALSA support ..................disabled"
fi

if test x$enable_jack = xyes; then
	echo " JACK support ..................enabled"
else
	echo " JACK support ..................disabled"
fi

if test x$enable_osx = xyes; then
	echo " OS X support ..................enabled"
else
	echo " OS X support ..................disabled"
fi

if test x$enable_pulse = xyes; then
	echo " PulseAudio support ............enabled"
else
	echo " PulseAudio support ............disabled"
fi

if test x$enable_fifo = xyes; then
	echo " FIFO support ..................enabled"
else
	echo " FIFO support ..................disabled"
fi

if test x$enable_mvp = xyes; then
	echo " Media MVP support .............enabled"
else
	echo " Media MVP support .............disabled"
fi

if test x$enable_shout_ogg = xyes; then
	echo " Shout ogg streaming support ...enabled"
else
	echo " Shout ogg streaming support ...disabled"
fi

if test x$enable_shout_mp3 = xyes; then
	echo " Shout mp3 streaming support ...enabled"
else
	echo " Shout mp3 streaming support ...disabled"
fi

echo ""

if test x$enable_ao = xno &&
   test x$enable_oss = xno &&
   test x$enable_shout_ogg = xno &&
   test x$enable_shout_mp3 = xno &&
   test x$enable_alsa = xno &&
   test x$enable_osx = xno &&
   test x$enable_pulse = xno &&
   test x$enable_jack = xno &&
   test x$enable_fifo = xno &&
   test x$enable_mvp = xno; then
	AC_MSG_ERROR([No Audio Output types configured!])
fi

echo " File Format Support:"

if test x$enable_id3 = xyes; then
	echo " ID3 tag support ...............enabled"
else
	echo " ID3 tag support ...............disabled"
fi

if test x$enable_mp3 = xyes; then
	echo " mp3 support ...................enabled"
else
	echo " mp3 support ...................disabled"
fi

if test x$enable_lame = xyes; then
        echo " lame support ..................enabled"
else
        echo " lame support ..................disabled"
fi

if test x$enable_oggvorbis = xyes; then
	echo " Ogg Vorbis support ............enabled"
	if test x$use_tremor = xyes; then
		echo "   using tremor.................yes"
	else
		echo "   using tremor.................no"
	fi
else
	echo " Ogg Vorbis support ............disabled"
fi

if test x$enable_flac = xyes; then
	echo " FLAC support ..................enabled"
else
	echo " FLAC support ..................disabled"
fi

case $enable_oggflac in
yes)
	echo " OggFLAC support ...............enabled"
	;;
flac)
	echo " OggFLAC support ...............enabled(FLAC 1.1.3)"
	;;
*)
	echo " OggFLAC support ...............disabled"
	;;
esac

if test x$enable_audiofile = xyes; then
	echo " Wave file support .............enabled"
else
	echo " Wave file support .............disabled"
fi

if test x$enable_aac = xyes; then
	echo " AAC support ...................enabled"
else
	echo " AAC support ...................disabled"
fi

if test x$enable_mp4 = xyes; then
	echo " MP4 support ...................enabled"
else
	echo " MP4 support ...................disabled"
fi

if test x$enable_mpc = xyes; then
	echo " Musepack (MPC) support ........enabled"
else
	echo " Musepack (MPC) support ........disabled"
fi

if test x$enable_wavpack = xyes; then
	echo " WavPack support ...............enabled"
else
	echo " WavPack support ...............disabled"
fi

if test x$enable_mod = xyes; then
	echo " MOD support ...................enabled"
else
	echo " MOD support ...................disabled"
fi

if test x$enable_ffmpeg = xyes; then
	echo " FFMPEG support ................enabled"
else
	echo " FFMPEG support ................disabled"
fi

if
	test x$enable_mp3 = xno &&
	test x$enable_oggvorbis = xno &&
	test x$enable_flac = xno && 
	test x$enable_oggflac = xno &&
	test x$enable_audiofile = xno && 
	test x$enable_aac = xno &&
	test x$enable_mpc = xno &&
	test x$enable_wavpack = xno &&
	test x$enable_ffmpeg = xno &&
	test x$enable_mod = xno; then
	AC_MSG_ERROR([No input plugins supported!])
fi

echo ""
echo " Other features:"

if test x$enable_lsr = xyes; then
        echo " libsamplerate support .........enabled"
else
	echo " libsamplerate support .........disabled"
fi
                

if test x$with_zeroconf != xno; then
	echo " Zeroconf support ..............$with_zeroconf"
else
	echo " Zeroconf support ..............disabled"
fi

echo ""
echo "##########################################"
echo ""
echo "You are now ready to compile MPD"
echo "Type \"make\" to compile MPD"
