AC_PREREQ(2.60)
AC_INIT(mpd, 0.16~git, musicpd-dev-team@lists.sourceforge.net)
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([foreign 1.10 dist-bzip2 subdir-objects])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AC_DEFINE(PROTOCOL_VERSION, "0.16.0", [The MPD protocol version])


dnl
dnl programs
dnl

AC_PROG_CC_C99
AC_PROG_CXX

HAVE_CXX=yes
if test x$CXX = xg++; then
	# CXX=g++ probably means that autoconf hasn't found any C++
	# compiler; to be sure, we check again
	AC_PATH_PROG(CXX, $CXX, no)
	if test x$CXX = xno; then
		# no, we don't have C++ - the following hack is
		# required because automake insists on using $(CXX)
		# for linking the MPD binary
		AC_MSG_NOTICE([Disabling C++ support])
		CXX="$CC"
		HAVE_CXX=no
	fi
fi

AC_PROG_INSTALL
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG


dnl
dnl declare variables
dnl

AC_SUBST(AM_CFLAGS,"")

AC_SUBST(MPD_LIBS)
AC_SUBST(MPD_CFLAGS)
MPD_LIBS=""
MPD_CFLAGS=""


dnl
dnl OS specific defaults
dnl

AC_CANONICAL_HOST

case "$host_os" in
mingw32* | windows*)
	MPD_LIBS="$MPD_LIBS -lws2_32"
	;;
esac

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
dnl libc features
dnl

AC_SYS_LARGEFILE
if test x$enable_largefile != xno; then
	AC_DEFINE([ENABLE_LARGEFILE], 1, [Define if large file support is enabled])
fi

AC_CHECK_FUNCS(daemon fork syslog)
if test $ac_cv_func_syslog = no; then
	# syslog is not in the default libraries.  See if it's in some other.
	for lib in bsd socket inet; do
		AC_CHECK_LIB($lib, syslog,
			[AC_DEFINE(HAVE_SYSLOG)
			LIBS="$LIBS -l$lib"; break])
	done
fi

AC_CHECK_LIB(socket,socket,MPD_LIBS="$MPD_LIBS -lsocket",)
AC_CHECK_LIB(nsl,gethostbyname,MPD_LIBS="$MPD_LIBS -lnsl",)

AC_CHECK_FUNCS(pipe2 accept4)

AC_CHECK_LIB(m,exp,MPD_LIBS="$MPD_LIBS -lm",)

AC_CHECK_HEADERS(locale.h)
AC_CHECK_HEADERS(valgrind/memcheck.h)

AC_CHECK_FUNCS(inotify_init inotify_init1)
AC_ARG_ENABLE(inotify,
	AS_HELP_STRING([--disable-inotify],
		[disable support Inotify automatic database update (default: enabled) ]),,
	[enable_inotify=yes])

if test x$ac_cv_func_inotify_init = xno; then
	enable_inotify=no
fi

if test x$enable_inotify = xyes; then
	AC_DEFINE([ENABLE_INOTIFY], 1, [Define to enable inotify support])
fi
AM_CONDITIONAL(ENABLE_INOTIFY, test x$enable_inotify = xyes)

dnl
dnl mandatory libraries
dnl

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.12 gthread-2.0],,
		[AC_MSG_ERROR([GLib 2.12 is required])])


dnl
dnl protocol options
dnl

AC_ARG_ENABLE(tcp,
	AS_HELP_STRING([--disable-tcp],
		[disable support for clients connecting via TCP (default: enable)]),,
	[enable_tcp=yes])

AC_ARG_ENABLE(ipv6,
	AS_HELP_STRING([--disable-ipv6],
		[disable IPv6 support (default: enable)]),,
	[enable_ipv6=yes])

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

AC_ARG_ENABLE(un,
	AS_HELP_STRING([--disable-un],
		[disable support for clients connecting via unix domain sockets (default: enable)]),,
	[enable_un=yes])

if test x$enable_un = xyes; then
	AC_DEFINE(HAVE_UN, 1, [Define if unix domain socket support is enabled])
	STRUCT_UCRED
fi


dnl ##
dnl misc libraries
dnl ##

AC_ARG_ENABLE(cue,
	AS_HELP_STRING([--enable-cue],
		[enable support for libcue support]),,
	enable_cue=auto)

MPD_AUTO_PKG(cue, CUE, [libcue],
	[libcue parsing library], [libcue not found])
if test x$enable_cue = xyes; then
	AC_DEFINE([HAVE_CUE], 1,
		[Define to enable libcue support])
fi

AM_CONDITIONAL(HAVE_CUE, test x$enable_cue = xyes)

dnl ##
dnl Avahi / Zeroconf
dnl ##
AC_ARG_WITH(zeroconf,
	AS_HELP_STRING([--with-zeroconf=@<:@auto|avahi|bonjour|no@:>@],
		[enable zeroconf backend (default=auto)]),,
	with_zeroconf="auto")

case $with_zeroconf in
no|avahi|bonjour)
	;;
*)
	with_zeroconf=auto
	;;
esac

if test x$with_zeroconf != xno; then
	if test x$with_zeroconf = xavahi || test x$with_zeroconf = xauto; then
		PKG_CHECK_MODULES([AVAHI], [avahi-client avahi-glib],
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
AM_CONDITIONAL(HAVE_AVAHI, test x$with_zeroconf = xavahi)
AM_CONDITIONAL(HAVE_BONJOUR, test x$with_zeroconf = xbonjour)

AC_ARG_ENABLE(sqlite,
	AS_HELP_STRING([--enable-sqlite],
		[enable support for the SQLite database]),,
	[enable_sqlite=auto])

MPD_AUTO_PKG(sqlite, SQLITE, [sqlite3],
	[SQLite database support], [sqlite not found])
if test x$enable_sqlite = xyes; then
	AC_DEFINE([ENABLE_SQLITE], 1, [Define to enable sqlite database support])
fi

AM_CONDITIONAL(ENABLE_SQLITE, test x$enable_sqlite = xyes)



dnl
dnl input plugins
dnl

AC_ARG_ENABLE(curl,
	AS_HELP_STRING([--enable-curl],
		[enable support for libcurl HTTP streaming (default: auto)]),,
	[enable_curl=auto])

MPD_AUTO_PKG(curl, CURL, [libcurl],
	[libcurl HTTP streaming], [libcurl not found])
if test x$enable_curl = xyes; then
	AC_DEFINE(ENABLE_CURL, 1, [Define when libcurl is used for HTTP streaming])
fi
AM_CONDITIONAL(ENABLE_CURL, test x$enable_curl = xyes)

AC_ARG_ENABLE(lastfm,
	AS_HELP_STRING([--enable-lastfm],
		[enable support for last.fm radio (default: disable)]),,
	[enable_lastfm=no])

if test x$enable_lastfm = xyes; then
	if test x$enable_curl != xyes; then
		AC_MSG_ERROR([Cannot enable last.fm radio without curl])
	fi

	AC_DEFINE(ENABLE_LASTFM, 1, [Define when last.fm radio is enabled])
fi
AM_CONDITIONAL(ENABLE_LASTFM, test x$enable_lastfm = xyes)

AC_ARG_ENABLE(mms,
	AS_HELP_STRING([--enable-mms],
		[enable the MMS protocol with libmms]),,
	[enable_mms=auto])

MPD_AUTO_PKG(mms, MMS, [libmms >= 0.4],
	[libmms mms:// protocol support], [libmms not found])
if test x$enable_mms = xyes; then
	AC_DEFINE(ENABLE_MMS, 1,
		[Define when libmms is used for the MMS protocol])
fi
AM_CONDITIONAL(ENABLE_MMS, test x$enable_mms = xyes)


dnl
dnl archive plugins
dnl

dnl bzip2
AC_ARG_ENABLE(bzip2,
	AS_HELP_STRING([--enable-bzip2],
		[enable bzip2 archive support (default: disabled)]),,
	enable_bzip2=no)

if test x$enable_bzip2 = xyes; then
	AC_CHECK_LIB(bz2, BZ2_bzDecompressInit,
		[MPD_LIBS="$MPD_LIBS -lbz2"],
		[AC_MSG_ERROR([libbz2 not found])])
fi

AM_CONDITIONAL(HAVE_BZ2, test x$enable_bzip2 = xyes)
if test x$enable_bzip2 = xyes; then
	AC_DEFINE(HAVE_BZ2, 1, [Define to have bz2 archive support])
fi

dnl zip
AC_ARG_ENABLE(zip,
	AS_HELP_STRING([--enable-zip],
		[enable zip archive support (default: disabled)]),,
	enable_zip=no)

MPD_AUTO_PKG(zip, ZZIP, [zziplib >= 0.13],
	[libzzip archive library], [libzzip not found])

AM_CONDITIONAL(HAVE_ZIP, test x$enable_zip = xyes)
if test x$enable_zip = xyes; then
	AC_DEFINE(HAVE_ZIP, 1, [Define to have zip archive support])
fi

dnl iso9660
AC_ARG_ENABLE(iso9660,
	AS_HELP_STRING([--enable-iso9660],
		[enable iso9660 archive support (default: disabled)]),,
	enable_iso9660=no)

MPD_AUTO_PKG(iso9660, ISO9660, [libiso9660],
	[libiso9660 archive library], [libiso9660 not found])

AM_CONDITIONAL(HAVE_ISO, test x$enable_iso9660 = xyes)
if test x$enable_iso9660 = xyes; then
	AC_DEFINE(HAVE_ISO, 1, [Define to have iso archive support])
fi

dnl archive API
if
	test x$enable_bzip2 = xyes ||
	test x$enable_zip = xyes ||
	test x$enable_iso9660 = xyes; then
		enable_archive=yes
		AC_DEFINE(ENABLE_ARCHIVE, 1, [The archive API is available])
else
	enable_archive=no
fi

AM_CONDITIONAL(ENABLE_ARCHIVE, test x$enable_archive = xyes)


dnl
dnl metadata
dnl

AC_ARG_ENABLE(id3,
	AS_HELP_STRING([--disable-id3],
		[disable id3 support (default: enable)]),,
	enable_id3=yes)


dnl
dnl decoder plugins
dnl

AC_ARG_ENABLE(audiofile,
	AS_HELP_STRING([--enable-audiofile],
		[enable audiofile support (WAV and others)]),,
	enable_audiofile=auto)

MPD_AUTO_PKG(audiofile, AUDIOFILE, [audiofile >= 0.1.7],
	[audiofile decoder plugin], [libaudiofile not found])
AM_CONDITIONAL(HAVE_AUDIOFILE, test x$enable_audiofile = xyes)
if test x$enable_audiofile = xyes; then
	AC_DEFINE(HAVE_AUDIOFILE, 1, [Define for audiofile support])
fi

AC_ARG_ENABLE(ffmpeg,
	AS_HELP_STRING([--enable-ffmpeg],
		[enable FFMPEG support]),,
	enable_ffmpeg=auto)

AC_ARG_ENABLE(flac,
	AS_HELP_STRING([--disable-flac],
		[disable flac support (default: enable)]),,
	enable_flac=yes)

dnl ###
dnl MAD mp3 decoder
dnl ###

AC_ARG_ENABLE(mad,
	AS_HELP_STRING([--enable-mad],
		[enable libmad mp3 decoder plugin]),,
	enable_mad=auto)

MPD_AUTO_PKG(mad, MAD, [mad],
	[libmad MP3 decoder plugin], [libmad not found])
if test x$enable_mad = xyes; then
	AC_DEFINE(HAVE_MAD, 1, [Define to use libmad])
fi
AM_CONDITIONAL(HAVE_MAD, test x$enable_mad = xyes)

AC_ARG_ENABLE(mpg123,
	AS_HELP_STRING([--enable-mpg123],
		[enable libmpg123 decoder plugin]),,
	enable_mpg123=auto)

MPD_AUTO_PKG(mpg123, MPG123, [libmpg123],
	[libmpg123 decoder plugin], [libmpg123 not found])
if test x$enable_mpg123 = xyes; then
	AC_DEFINE(HAVE_MPG123, 1, [Define to use libmpg123])
fi
AM_CONDITIONAL(HAVE_MPG123, test x$enable_mpg123 = xyes)

AC_ARG_ENABLE(mikmod,
	AS_HELP_STRING([--enable-mikmod],
		[enable the mikmod decoder (default: disable)]),,
	enable_mikmod=no)

if test x$enable_mikmod = xyes; then
	AC_PATH_PROG(LIBMIKMOD_CONFIG, libmikmod-config)
	if test x$LIBMIKMOD_CONFIG != x ; then
		AC_SUBST(LIBMIKMOD_CFLAGS, `$LIBMIKMOD_CONFIG --cflags`)
		AC_SUBST(LIBMIKMOD_LIBS, `$LIBMIKMOD_CONFIG --libs`)
		AC_DEFINE(ENABLE_MIKMOD_DECODER, 1, [Define for mikmod support])
	else
		enable_mikmod=no
	fi
fi

AM_CONDITIONAL(ENABLE_MIKMOD_DECODER, test x$enable_mikmod = xyes)

AC_ARG_ENABLE(modplug,
	AS_HELP_STRING([--enable-modplug],
		[enable modplug decoder plugin]),,
	enable_modplug=auto)

found_modplug=$HAVE_CXX
MPD_AUTO_PRE(modplug, [modplug decoder plugin], [No C++ compiler found])

MPD_AUTO_PKG(modplug, MODPLUG, [libmodplug],
	[modplug decoder plugin], [libmodplug not found])
AM_CONDITIONAL(HAVE_MODPLUG, test x$enable_modplug = xyes)
if test x$enable_modplug = xyes; then
	AC_DEFINE(HAVE_MODPLUG, 1, [Define for modplug support])
fi

AC_ARG_ENABLE(mpc,
	AS_HELP_STRING([--disable-mpc],
		[disable musepack (MPC) support (default: enable)]),,
	enable_mpc=yes)

AC_ARG_ENABLE(oggflac,
	AS_HELP_STRING([--disable-oggflac],
		[disable OggFLAC support (default: enable)]),,
	enable_oggflac=yes)

AC_ARG_ENABLE(vorbis,
	AS_HELP_STRING([--disable-vorbis],
		[disable Ogg Vorbis support (default: enable)]),,
	enable_vorbis=yes)

AC_ARG_ENABLE(sndfile,
	AS_HELP_STRING([--enable-sndfile],
		[enable sndfile support]),,
	enable_sndfile=auto)

if test x$enable_sndfile = xauto && test x$enable_modplug = xyes; then
	dnl If modplug is enabled, enable sndfile only if explicitly
	dnl requested - modplug's modplug/sndfile.h is known to
	dnl conflict with libsndfile's sndfile.h.
	AC_MSG_NOTICE([disabling libsndfile auto-detection, because the modplug decoder is enabled])
	enable_sndfile=no
fi

MPD_AUTO_PKG(sndfile, SNDFILE, [sndfile],
	[libsndfile decoder plugin], [libsndfile not found])
AM_CONDITIONAL(ENABLE_SNDFILE, test x$enable_sndfile = xyes)
if test x$enable_sndfile = xyes; then
	AC_DEFINE(ENABLE_SNDFILE, 1, [Define to enable the sndfile decoder plugin])
fi


dnl ###
dnl Ogg Tremor
dnl ###
AC_ARG_WITH(tremor,
	AS_HELP_STRING([--with-tremor=PFX],
		[use Tremor (vorbisidec) integer Ogg Vorbis decoder (with optional prefix)]),,
	with_tremor=no)

if test x$with_tremor = xyes || test x$with_tremor = xno; then
	use_tremor="$with_tremor"
else
	tremor_prefix="$with_tremor"
	use_tremor=yes
fi

AC_ARG_WITH(tremor-libraries,
	AS_HELP_STRING([--with-tremor-libraries=DIR],
		[directory where Tremor library is installed (optional)]),,
	tremor_libraries="")

AC_ARG_WITH(tremor-includes,
	AS_HELP_STRING([--with-tremor-includes=DIR],
		[directory where Tremor header files are installed (optional)]),,
	tremor_includes="")

AC_ARG_ENABLE(sidplay,
	AS_HELP_STRING([--enable-sidplay],
		[enable C64 SID support via libsidplay2]),,
	enable_sidplay=auto)

found_sidplay=$HAVE_CXX
MPD_AUTO_PRE(sidplay, [sidplay decoder plugin], [No C++ compiler found])

if test x$enable_sidplay != xno; then
	# we're not using pkg-config here
	# because libsidplay2's .pc file requires libtool
	AC_HAVE_LIBRARY(sidplay2, [found_sidplay=yes], [found_sidplay=no])
	MPD_AUTO_PRE(sidplay, [sidplay decoder plugin],
		[libsidplay2 not found])
fi

if test x$enable_sidplay != xno; then
	# can't use AC_HAVE_LIBRARY here, because the dash in the
	# library name triggers an autoconf bug
	AC_CHECK_LIB(resid-builder, main,
		[found_sidplay=yes], [found_sidplay=no])

	if test x$found_sidplay = xyes; then
		AC_HAVE_LIBRARY(sidutils,, [found_sidplay=no])
	fi

	MPD_AUTO_RESULT(sidplay, [sidplay decoder plugin],
		[libresid-builder or libsidutils not found])
fi

if test x$enable_sidplay = xyes; then
	AC_SUBST(SIDPLAY_LIBS,"-lsidplay2 -lresid-builder -lsidutils")
	AC_SUBST(SIDPLAY_CFLAGS,)

	AC_DEFINE(ENABLE_SIDPLAY, 1, [Define for libsidplay2 support])
fi

AM_CONDITIONAL(ENABLE_SIDPLAY, test x$enable_sidplay = xyes)

AC_ARG_ENABLE(fluidsynth,
	AS_HELP_STRING([--enable-fluidsynth],
		[enable MIDI support via fluidsynth (default: disable)]),,
	enable_fluidsynth=no)

AC_ARG_ENABLE(wildmidi,
	AS_HELP_STRING([--enable-wildmidi],
		[enable MIDI support via wildmidi (default: disable)]),,
	enable_wildmidi=no)

AC_ARG_ENABLE(wavpack,
	AS_HELP_STRING([--enable-wavpack],
		[enable WavPack support]),,
	enable_wavpack=auto)

MPD_AUTO_PKG(wavpack, WAVPACK, [wavpack],
	[WavPack decoder plugin], [libwavpack not found])
AM_CONDITIONAL(HAVE_WAVPACK, test x$enable_wavpack = xyes)
if test x$enable_wavpack = xyes; then
	AC_DEFINE([HAVE_WAVPACK], 1, [Define to enable WavPack support])
fi


dnl
dnl converters
dnl

AC_ARG_ENABLE(lsr,
	AS_HELP_STRING([--enable-lsr],
		[enable libsamplerate support]),,
	enable_lsr=auto)

MPD_AUTO_PKG(lsr, SAMPLERATE, [samplerate >= 0.0.15],
	[libsamplerate resampling], [libsamplerate not found])
if test x$enable_lsr = xyes; then
	AC_DEFINE([HAVE_LIBSAMPLERATE], 1,
		[Define to enable libsamplerate])
fi

if test x$enable_lsr = xyes; then
	PKG_CHECK_MODULES([SAMPLERATE_013],
		[samplerate >= 0.1.3],,
		[AC_DEFINE([HAVE_LIBSAMPLERATE_NOINT], 1,
		[libsamplerate doesn't provide src_int_to_float_array() (<0.1.3)])])
fi

AM_CONDITIONAL(HAVE_LIBSAMPLERATE, test x$enable_lsr = xyes)


dnl
dnl encoder plugins
dnl

AC_ARG_ENABLE(vorbis-encoder,
	AS_HELP_STRING([--enable-vorbis-encoder],
		[enable the Ogg Vorbis encoder]),,
	[enable_vorbis_encoder=auto])

AC_ARG_ENABLE(lame-encoder,
	AS_HELP_STRING([--enable-lame-encoder],
		[enable the LAME mp3 encoder]),,
	enable_lame_encoder=auto)

AC_ARG_ENABLE(twolame-encoder,
	AS_HELP_STRING([--enable-twolame-encoder],
		[enable the TwoLAME mp2 encoder]),,
	enable_twolame_encoder=auto)

AC_ARG_ENABLE(wave-encoder,
	AS_HELP_STRING([--enable-wave-encoder],
		[enable the PCM wave encoder]),,
	enable_wave_encoder=yes)


dnl
dnl audio output plugins
dnl

AC_ARG_ENABLE(alsa,
	AS_HELP_STRING([--enable-alsa], [enable ALSA support]),,
	enable_alsa=auto)

AC_ARG_ENABLE(ao,
	AS_HELP_STRING([--enable-ao],
		[enable support for libao]),,
	enable_ao=auto)

MPD_AUTO_PKG(ao, AO, [ao],
	[libao output plugin], [libao not found])
if test x$enable_ao = xyes; then
	AC_DEFINE(HAVE_AO, 1, [Define to play with ao])
fi

AM_CONDITIONAL(HAVE_AO, test x$enable_ao = xyes)

AC_ARG_ENABLE(fifo,
	AS_HELP_STRING([--disable-fifo],
		[disable support for writing audio to a FIFO (default: enable)]),,
	enable_fifo=yes)

AC_ARG_ENABLE(pipe-output,
	AS_HELP_STRING([--enable-pipe-output],
		[enable support for writing audio to a pipe (default: disable)]),,
	enable_pipe_output=no)

if test x$enable_pipe_output = xyes; then
	AC_DEFINE([ENABLE_PIPE_OUTPUT], 1,
		[Define to enable support for writing audio to a pipe])
fi
AM_CONDITIONAL(ENABLE_PIPE_OUTPUT, test x$enable_pipe_output = xyes)

AC_ARG_ENABLE(jack,
	AS_HELP_STRING([--enable-jack],
		[enable jack support]),,
	enable_jack=auto)

MPD_AUTO_PKG(jack, JACK, [jack >= 0.100],
	[JACK output plugin], [libjack not found])
if test x$enable_jack = xyes; then
	AC_DEFINE([HAVE_JACK], 1, [Define to enable JACK support])
fi

if test x$enable_jack = xyes; then
	# check whether jack_set_info_function() is available
	old_LIBS=$LIBS
	LIBS="$LIBS $JACK_LIBS"

	AC_CHECK_FUNCS(jack_set_info_function)

	LIBS=$old_LIBS
fi

AM_CONDITIONAL(HAVE_JACK, test x$enable_jack = xyes)

AC_ARG_ENABLE(mvp,
	AS_HELP_STRING([--enable-mvp],
		[enable support for Hauppauge Media MVP (default: disable)]),,
	enable_mvp=no)

AC_ARG_ENABLE(oss,
	AS_HELP_STRING([--disable-oss],
		[disable OSS support (default: enable)]),,
	enable_oss=yes)

AC_ARG_ENABLE(openal,
	AS_HELP_STRING([--enable-openal],
		[enable OpenAL support (default: disable)]),,
	enable_openal=no)

AC_ARG_ENABLE(pulse,
	AS_HELP_STRING([--enable-pulse],
		[enable support for the PulseAudio sound server]),,
	enable_pulse=auto)

MPD_AUTO_PKG(pulse, PULSE, [libpulse],
	[PulseAudio output plugin], [libpulse not found])
if test x$enable_pulse = xyes; then
	AC_DEFINE([HAVE_PULSE], 1,
		[Define to enable PulseAudio support])
fi

AM_CONDITIONAL(HAVE_PULSE, test x$enable_pulse = xyes)

AC_ARG_ENABLE(recorder-output,
	AS_HELP_STRING([--enable-recorder-output],
		[enables the recorder file output plugin (default: disable)]),,
	[enable_recorder_output=auto])

AC_ARG_ENABLE(httpd-output,
	AS_HELP_STRING([--enable-httpd-output],
		[enables the HTTP server output]),,
	[enable_httpd_output=auto])

enable_osx=no
case "$host_os" in
	darwin*)
		AC_DEFINE(HAVE_OSX, 1, [Define for compiling OS X support])
		MPD_LIBS="$MPD_LIBS -framework AudioUnit -framework CoreServices"
		enable_osx=yes ;;
esac

AM_CONDITIONAL(HAVE_OSX, test x$enable_osx = xyes)

AC_ARG_ENABLE(shout,
	AS_HELP_STRING([--enable-shout],
		[enables the shoutcast streaming output]),,
	[enable_shout=auto])

enable_shout2="$enable_shout"
MPD_AUTO_PKG(shout, SHOUT, [shout],
	[shout output plugin], [libshout not found])
if test x$enable_shout = xyes && test x$enable_shout2 = xauto; then
	enable_shout=auto
fi

case "$host_os" in
	solaris*)
		AC_DEFINE(ENABLE_SOLARIS_OUTPUT, 1, [Define to enable Solaris /dev/audio support])
		enable_solaris_output=yes
		;;

	*)
		enable_solaris_output=no
		;;
esac

AM_CONDITIONAL(ENABLE_SOLARIS_OUTPUT, test x$enable_solaris_output = xyes)

if test x$enable_oss = xyes; then
	AC_CHECK_HEADER(sys/soundcard.h,
		[enable_oss=yes;AC_DEFINE(HAVE_OSS,1,[Define to enable OSS])],
		[AC_MSG_WARN(Soundcard headers not found -- disabling OSS support);
			enable_oss=no])
fi

AM_CONDITIONAL(HAVE_OSS, test x$enable_oss = xyes)

if test x$enable_openal = xyes; then
	if test x$enable_osx = xyes; then
		AC_CHECK_HEADERS([OpenAL/al.h OpenAL/alc.h], [], [enable_openal=no])
		if test x$enable_openal = xyes; then
			OPENAL_LIBS="-framework OpenAL"
			AC_DEFINE(HAVE_OPENAL, 1, [Define for OpenAL support])
		else
			AC_MSG_WARN(OpenAL headers not found -- disabling OpenAL support)
		fi
	else
		PKG_CHECK_MODULES([OPENAL], [openal],
			AC_DEFINE(HAVE_OPENAL, 1, [Define for OpenAL support]),
			enable_openal=no)
	fi
fi

if test x$enable_openal = xyes; then
	MPD_CFLAGS="$MPD_CFLAGS $OPENAL_CFLAGS"
	MPD_LIBS="$MPD_LIBS $OPENAL_LIBS"
fi

AM_CONDITIONAL(HAVE_OPENAL, test x$enable_openal = xyes)

if test x$enable_fifo = xyes; then
	AC_CHECK_FUNC([mkfifo],
		[enable_fifo=yes;AC_DEFINE([HAVE_FIFO], 1,
			[Define to enable support for writing audio to a FIFO])],
		[enable_fifo=no;AC_MSG_WARN([mkfifo not found -- disabling support for writing audio to a FIFO])])
fi

AM_CONDITIONAL(HAVE_FIFO, test x$enable_fifo = xyes)

if test x$enable_mvp = xyes; then
   AC_DEFINE(HAVE_MVP,1,[Define to enable Hauppauge Media MVP support])
fi

AM_CONDITIONAL(HAVE_MVP, test x$enable_mvp = xyes)

MPD_AUTO_PKG(alsa, ALSA, [alsa >= 0.9.0],
	[ALSA output plugin], [libasound not found])

if test x$enable_alsa = xyes; then
	AC_DEFINE(HAVE_ALSA, 1, [Define to enable ALSA support])
fi

AM_CONDITIONAL(HAVE_ALSA, test x$enable_alsa = xyes)

if test x$enable_id3 = xyes; then
	PKG_CHECK_MODULES([ID3TAG], [id3tag],,
		AC_CHECK_LIB(id3tag, id3_file_open,
			[ID3TAG_LIBS="-lid3tag -lz" ID3TAG_CFLAGS=""],
			enable_id3=no))
fi

if test x$enable_id3 = xyes; then
	AC_DEFINE(HAVE_ID3TAG, 1, [Define to use id3tag])
fi

AM_CONDITIONAL(HAVE_ID3TAG, test x$enable_id3 = xyes)

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
	AC_CHECK_HEADER(mpc/mpcdec.h,
		old_mpcdec=no,
		[AC_CHECK_HEADER(mpcdec/mpcdec.h,
			old_mpcdec=yes,
			enable_mpc=no)])
	if test x$enable_mpc = xyes; then
		AC_CHECK_LIB(mpcdec,main,
			[MPD_LIBS="$MPD_LIBS $MPCDEC_LIBS";
			MPD_CFLAGS="$MPD_CFLAGS $MPCDEC_CFLAGS";],
			enable_mpc=no)
	fi
	if test x$enable_mpc = xyes; then
		AC_DEFINE(HAVE_MPCDEC,1,
			[Define to use libmpcdec for MPC decoding])
		if test x$old_mpcdec = xyes; then
			AC_DEFINE(MPC_IS_OLD_API, 1,
				[Define if an old pre-SV8 libmpcdec is used])
		fi
	else
		AC_MSG_WARN([mpcdec lib needed for MPC support -- disabling MPC support])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
fi

AM_CONDITIONAL(HAVE_MPCDEC, test x$enable_mpc = xyes)

AM_PATH_FAAD()

AM_CONDITIONAL(HAVE_FAAD, test x$enable_aac = xyes)

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
	AC_CHECK_LIB(vorbisidec,ov_read,enable_vorbis=yes,enable_vorbis=no;
		AC_MSG_WARN([vorbisidec lib needed for ogg support with tremor -- disabling ogg support]))
	CFLAGS="$ac_save_CFLAGS"
	LIBS="$ac_save_LIBS"
	if test x$enable_vorbis = xyes; then
		AC_DEFINE(ENABLE_VORBIS_DECODER, 1, [Define for Ogg Vorbis support])
	fi
elif test x$enable_vorbis = xyes; then
	PKG_CHECK_MODULES(VORBIS, [ogg vorbis vorbisfile],
		AC_DEFINE(ENABLE_VORBIS_DECODER, 1, [Define for Ogg Vorbis support]),
		enable_vorbis=no)
fi

AM_CONDITIONAL(ENABLE_VORBIS_DECODER, test x$enable_vorbis = xyes)

if test x$use_tremor = xyes; then
	AC_DEFINE(HAVE_TREMOR,1,
		[Define to use tremor (libvorbisidec) for ogg support])
	if test x$enable_oggflac = xyes; then
		AC_MSG_WARN([disabling OggFLAC support because it is incompatible with tremor])
		enable_oggflac=no
	fi
fi

AC_SUBST(TREMOR_CFLAGS)
AC_SUBST(TREMOR_LIBS)

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

enable_flac_encoder=$enable_flac

if test x$enable_oggflac = xyes; then
	oldmpdcflags="$MPD_CFLAGS"
	oldmpdlibs="$MPD_LIBS"
	AM_PATH_LIBOGGFLAC(MPD_LIBS="$MPD_LIBS $LIBOGGFLAC_LIBS"
				MPD_CFLAGS="$MPD_CFLAGS $LIBOGGFLAC_CFLAGS",
			   enable_oggflac=no)
fi

if test x$enable_oggflac = xyes; then
	AC_DEFINE(HAVE_OGGFLAC,1,[Define for OggFLAC support])
fi

AM_CONDITIONAL(HAVE_OGGFLAC, test x$enable_oggflac = xyes)

AM_CONDITIONAL(HAVE_FLAC_COMMON,
	  test x$enable_flac = xyes || test x$enable_oggflac = xyes)
AM_CONDITIONAL(HAVE_OGG_COMMON,
	  test x$enable_vorbis = xyes || test x$enable_oggflac = xyes || test x$enable_flac = xyes)

MPD_AUTO_PKG(ffmpeg, FFMPEG, [libavformat libavcodec libavutil],
	[ffmpeg decoder library], [libavformat+libavcodec+libavutil not found])

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

if test x$enable_ffmpeg = xyes; then
	AC_DEFINE(HAVE_FFMPEG, 1, [Define for FFMPEG support])
fi

AM_CONDITIONAL(HAVE_FFMPEG, test x$enable_ffmpeg = xyes)

if test x$enable_fluidsynth = xyes; then
	PKG_CHECK_MODULES(FLUIDSYNTH, [fluidsynth],
		AC_DEFINE(ENABLE_FLUIDSYNTH, 1, [Define for fluidsynth support]),
		enable_fluidsynth=no)
fi

AM_CONDITIONAL(ENABLE_FLUIDSYNTH, test x$enable_fluidsynth = xyes)

if test x$enable_wildmidi = xyes; then
	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS

	AC_CHECK_LIB(WildMidi, WildMidi_Init,,
		AC_MSG_ERROR([libwildmidi not found]))

	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags

	AC_SUBST(WILDMIDI_LIBS,-lWildMidi)
	AC_SUBST(WILDMIDI_CFLAGS,)

	AC_DEFINE(ENABLE_WILDMIDI, 1, [Define for wildmidi support])
fi

AM_CONDITIONAL(ENABLE_WILDMIDI, test x$enable_wildmidi = xyes)


dnl
dnl Encoder API and shout/httpd output plugin
dnl

if test x$enable_shout = xyes || \
	test x$enable_recorder_output = xyes || \
	test x$enable_httpd_output = xyes; then
	# at least one output using encoders is explicitly enabled
	need_encoder=yes
elif test x$enable_shout = xauto || \
	test x$enable_recorder_output = xauto || \
	test x$enable_httpd_output = xauto; then
	need_encoder=auto
else
	# all outputs using encoders are disabled
	need_encoder=no

	# don't bother to check for encoder plugins
	enable_vorbis_encoder=no
	enable_lame_encoder=no
	enable_twolame_encoder=no
	enable_wave_encoder=no
	enable_flac_encoder=no
fi

MPD_AUTO_PKG(vorbis_encoder, VORBISENC, [vorbisenc],
	[Ogg Vorbis encoder], [libvorbisenc not found])

if test x$enable_lame_encoder != xno; then
	AM_PATH_LAME([found_lame_encoder=yes], [found_lame_encoder=no])
	MPD_AUTO_RESULT(lame_encoder, [LAME encoder plugin],
		[LAME not found])
fi

AC_SUBST(LAME_CFLAGS)
AC_SUBST(LAME_LIBS)

MPD_AUTO_PKG(twolame_encoder, TWOLAME, [twolame],
	[TwoLAME encoder], [libtwolame not found])

if test x$enable_vorbis_encoder != xno ||
	test x$enable_lame_encoder != xno ||
	test x$enable_twolame_encoder != xno ||
	test x$enable_flac_encoder != xno ||
	test x$enable_wave_encoder != xno; then
	# at least one encoder plugin is enabled
	enable_encoder=yes
else
	# no encoder plugin is enabled: disable the whole encoder API
	enable_encoder=no

	if test x$need_encoder = xyes; then
		AC_MSG_ERROR([No encoder plugin found])
	fi
fi


if test x$enable_shout = xauto; then
	# handle shout auto-detection: disable if no encoder is
	# available
	if test x$enable_encoder = xyes; then
		enable_shout=yes
	else
		AC_MSG_WARN([No encoder plugin -- disabling the shout output plugin])
		enable_shout=no
	fi
fi

if test x$enable_recorder_output = xauto; then
	# handle recorder auto-detection: disable if no encoder is
	# available
	if test x$enable_encoder = xyes; then
		enable_recorder_output=yes
	else
		AC_MSG_WARN([No encoder plugin -- disabling the recorder output plugin])
		enable_recorder_output=no
	fi
fi

if test x$enable_httpd_output = xauto; then
	# handle HTTPD auto-detection: disable if no encoder is
	# available
	if test x$enable_encoder = xyes; then
		enable_httpd_output=yes
	else
		AC_MSG_WARN([No encoder plugin -- disabling the HTTP output plugin])
		enable_httpd_output=no
	fi
fi

AM_CONDITIONAL(HAVE_SHOUT, test x$enable_shout = xyes)
if test x$enable_shout = xyes; then
	AC_DEFINE(HAVE_SHOUT, 1, [Define to enable the shoutcast output])
fi

AM_CONDITIONAL(ENABLE_RECORDER_OUTPUT, test x$enable_recorder_output = xyes)
if test x$enable_recorder_output = xyes; then
	AC_DEFINE(ENABLE_RECORDER_OUTPUT, 1, [Define to enable the recorder output])
fi

AM_CONDITIONAL(ENABLE_HTTPD_OUTPUT, test x$enable_httpd_output = xyes)
if test x$enable_httpd_output = xyes; then
	AC_DEFINE(ENABLE_HTTPD_OUTPUT, 1, [Define to enable the HTTP server output])
fi

AM_CONDITIONAL(ENABLE_ENCODER, test x$enable_encoder = xyes)
if test x$enable_encoder = xyes; then
	AC_DEFINE(ENABLE_ENCODER, 1,
		[Define to enable the encoder plugins])
fi

AM_CONDITIONAL(ENABLE_VORBIS_ENCODER, test x$enable_vorbis_encoder = xyes)
if test x$enable_vorbis_encoder = xyes; then
	AC_DEFINE(ENABLE_VORBIS_ENCODER, 1,
		[Define to enable the vorbis encoder plugin])
fi

AM_CONDITIONAL(ENABLE_LAME_ENCODER, test x$enable_lame_encoder = xyes)
if test x$enable_lame_encoder = xyes; then
	AC_DEFINE(ENABLE_LAME_ENCODER, 1,
		[Define to enable the lame encoder plugin])
fi

AM_CONDITIONAL(ENABLE_TWOLAME_ENCODER, test x$enable_twolame_encoder = xyes)
if test x$enable_twolame_encoder = xyes; then
	AC_DEFINE(ENABLE_TWOLAME_ENCODER, 1,
		[Define to enable the TwoLAME encoder plugin])
fi

AM_CONDITIONAL(ENABLE_WAVE_ENCODER, test x$enable_wave_encoder = xyes)
if test x$enable_wave_encoder = xyes; then
	AC_DEFINE(ENABLE_WAVE_ENCODER, 1,
		[Define to enable the PCM wave encoder plugin])
fi

AM_CONDITIONAL(ENABLE_FLAC_ENCODER, test x$enable_flac_encoder = xyes)
if test x$enable_flac_encoder = xyes; then
	AC_DEFINE(ENABLE_FLAC_ENCODER, 1,
		[Define to enable the FLAC encoder plugin])
fi

dnl
dnl Documentation
dnl

AC_ARG_ENABLE(documentation,
	AS_HELP_STRING([--enable-documentation],
		[build documentation (default: disable)]),,
	[enable_documentation=no])

if test x$enable_documentation = xyes; then
	AC_PATH_PROG(XMLTO, xmlto)
	AC_SUBST(XMLTO)
	AM_CONDITIONAL(HAVE_XMLTO, test x$XMLTO != x)

	AC_PATH_PROG(DOXYGEN, doxygen)
	if test x$DOXYGEN = x; then
		AC_MSG_ERROR([doxygen not found])
	fi

	AC_SUBST(DOXYGEN)
else
	AM_CONDITIONAL(HAVE_XMLTO, false)
fi

AM_CONDITIONAL(ENABLE_DOCUMENTATION, test x$enable_documentation = xyes)


dnl
dnl build options
dnl

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],
		[treat warnings as errors (default: disabled)]),,
	enable_werror=no)

if test "x$enable_werror" = xyes; then
	AM_CFLAGS="$AM_CFLAGS -Werror -pedantic-errors"
fi

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug],
		[enable debugging (default: disabled)]),,
	enable_debug=no)

#if test "x$enable_debug" = xno; then
	# don't set NDEBUG for now, until MPD is stable
	#AM_CFLAGS="$AM_CFLAGS -DNDEBUG"
#fi

AC_ARG_ENABLE(gprof,
	AS_HELP_STRING([--enable-gprof],
		[enable profiling via gprof (default: disabled)]),,
	enable_gprof=no)

if test "x$enable_gprof" = xyes; then
	MPD_CFLAGS="$MPD_CFLAGS -pg"
	MPD_LIBS="$MPD_LIBS -pg"
fi

AC_ARG_ENABLE(test,
	AS_HELP_STRING([--enable-test],
		[build the test programs (default: disabled)]),,
	enable_test=no)

AM_CONDITIONAL(ENABLE_TEST, test "x$enable_test" = xyes)


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
dnl pretty-print result
dnl

echo ""
echo "########### MPD CONFIGURATION ############"
echo ""

echo " Client Support:"
if test x$enable_ipv6 = xyes; then
	echo " IPv6 support ..................enabled"
else
	echo " IPv6 support ..................disabled"
fi

if test x$enable_tcp = xyes; then
	echo " TCP support ...................enabled"
else
	echo " TCP support ...................disabled"
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
if test x$enable_alsa = xyes; then
	echo " ALSA support ..................enabled"
else
	echo " ALSA support ..................disabled"
fi

if test x$enable_fifo = xyes; then
	echo " FIFO support ..................enabled"
else
	echo " FIFO support ..................disabled"
fi

if test x$enable_recorder_output = xyes; then
	echo " File Recorder support .........enabled"
else
	echo " File Recorder support .........disabled"
fi

if test x$enable_httpd_output = xyes; then
	echo " HTTP daemon support ...........enabled"
else
	echo " HTTP daemon support ...........disabled"
fi

if test x$enable_jack = xyes; then
	echo " JACK support ..................enabled"
else
	echo " JACK support ..................disabled"
fi

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

if test x$enable_openal = xyes; then
	echo " OpenAL support ................enabled"
else
	echo " OpenAL support ................disabled"
fi

if test x$enable_osx = xyes; then
	echo " OS X support ..................enabled"
else
	echo " OS X support ..................disabled"
fi

if test x$enable_pipe_output = xyes; then
	echo " Pipeline output support .......enabled"
else
	echo " Pipeline output support .......disabled"
fi

if test x$enable_pulse = xyes; then
	echo " PulseAudio support ............enabled"
else
	echo " PulseAudio support ............disabled"
fi

if test x$enable_mvp = xyes; then
	echo " Media MVP support .............enabled"
else
	echo " Media MVP support .............disabled"
fi

if test x$enable_shout = xyes; then
	echo " SHOUTcast support .............enabled"
else
	echo " SHOUTcast support .............disabled"
fi

if test x$enable_solaris_output = xyes; then
	echo " Solaris /dev/audio support ....enabled"
else
	echo " Solaris /dev/audio support ....disabled"
fi

echo ""

if
	test x$enable_ao = xno &&
	test x$enable_oss = xno &&
	test x$enable_openal = xno &&
	test x$enable_shout = xno &&
	test x$enable_recorder_output = xno &&
	test x$enable_httpd_output = xno &&
	test x$enable_solaris_output = xno &&
	test x$enable_alsa = xno &&
	test x$enable_osx = xno &&
	test x$enable_pulse = xno &&
	test x$enable_jack = xno &&
	test x$enable_fifo = xno &&
	test x$enable_pipe_output = xno &&
	test x$enable_mvp = xno; then
		AC_MSG_ERROR([No Audio Output types configured!])
fi

if
	test x$enable_shout = xyes ||
	test x$enable_recorder = xyes ||
	test x$enable_httpd_output = xyes; then
		echo " Streaming Encoder Support:"
		if test x$enable_lame_encoder = xyes; then
			echo " LAME mp3 encoder ..............enabled"
		else
			echo " LAME mp3 encoder ..............disabled"
		fi

		if test x$enable_vorbis_encoder = xyes; then
			echo " Ogg Vorbis encoder ............enabled"
		else
			echo " Ogg Vorbis encoder ............disabled"
		fi

		if test x$enable_twolame_encoder = xyes; then
			echo " TwoLAME mp3 encoder ...........enabled"
		else
			echo " TwoLAME mp3 encoder ...........disabled"
		fi

		if test x$enable_flac_encoder = xyes; then
			echo " FLAC encoder ..................enabled"
		else
			echo " FLAC encoder ..................disabled"
		fi

		if test x$enable_wave_encoder = xyes; then
			echo " PCM wave encoder ..............enabled"
		else
			echo " PCM wave encoder ..............disabled"
		fi

		echo ""
fi

echo " File Format Support:"

if test x$enable_aac = xyes; then
	echo " AAC support ...................enabled"
else
	echo " AAC support ...................disabled"
fi

if test x$enable_sidplay = xyes; then
	echo " C64 SID support ...............enabled"
else
	echo " C64 SID support ...............disabled"
fi

if test x$enable_ffmpeg = xyes; then
	echo " FFMPEG support ................enabled"
else
	echo " FFMPEG support ................disabled"
fi

if test x$enable_flac = xyes; then
	echo " FLAC support ..................enabled"
else
	echo " FLAC support ..................disabled"
fi

if test x$enable_fluidsynth = xyes; then
	echo " fluidsynth MIDI support .......enabled"
else
	echo " fluidsynth MIDI support .......disabled"
fi

if test x$enable_mikmod = xyes; then
	echo " MikMod support ................enabled"
else
	echo " MikMod support ................disabled"
fi

if test x$enable_modplug = xyes; then
	echo " MODPLUG support ...............enabled"
else
	echo " MODPLUG support ...............disabled"
fi

if test x$enable_mad = xyes; then
	echo " MAD mp3 decoder support .......enabled"
else
	echo " MAD mp3 decoder support .......disabled"
fi

if test x$enable_mpg123 = xyes; then
	echo " libmpg123 decoder support .....enabled"
else
	echo " libmpg123 decoder support .....disabled"
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

if test x$enable_vorbis = xyes; then
	echo " Ogg Vorbis support ............enabled"
	if test x$use_tremor = xyes; then
		echo "   using tremor.................yes"
	else
		echo "   using tremor.................no"
	fi
else
	echo " Ogg Vorbis support ............disabled"
fi

if test x$enable_sndfile = xyes; then
	echo " libsndfile ....................enabled"
else
	echo " libsndfile ....................disabled"
fi

if test x$enable_audiofile = xyes; then
	echo " Wave file support .............enabled"
else
	echo " Wave file support .............disabled"
fi

if test x$enable_wavpack = xyes; then
	echo " WavPack support ...............enabled"
else
	echo " WavPack support ...............disabled"
fi

if test x$enable_wildmidi = xyes; then
	echo " wildmidi MIDI support .........enabled"
else
	echo " wildmidi MIDI support .........disabled"
fi



if
	test x$enable_mad = xno &&
	test x$enable_mpg123 = xno &&
	test x$enable_vorbis = xno &&
	test x$enable_flac = xno && 
	test x$enable_oggflac = xno &&
	test x$enable_audiofile = xno && 
	test x$enable_aac = xno &&
	test x$enable_mpc = xno &&
	test x$enable_wavpack = xno &&
	test x$enable_ffmpeg = xno &&
	test x$enable_modplug = xno &&
	test x$enable_sidplay = xno &&
	test x$enable_fluidsynth = xno &&
	test x$enable_wildmidi = xno &&
	test x$enable_mp4 = xno &&
	test x$enable_mikmod = xno; then
		AC_MSG_ERROR([No input plugins supported!])
fi

echo ""
echo " Archive support:"

if test x$enable_bzip2 = xyes; then
	echo " BZ2 archives support ..........enabled"
else
	echo " BZ2 archives support ..........disabled"
fi

if test x$enable_iso9660 = xyes; then
	echo " ISO 9660 archives support .....enabled"
else
	echo " ISO 9660 archives support .....disabled"
fi

if test x$enable_zip = xyes; then
	echo " ZIP archives support ..........enabled"
else
	echo " ZIP archives support ..........disabled"
fi

echo ""
echo " Streaming support:"

if test x$enable_lastfm = xyes; then
        echo " last.fm radio support .........enabled"
else
        echo " last.fm radio support .........disabled"
fi

if test x$enable_curl = xyes; then
        echo " libcurl support (streaming) ...enabled"
else
        echo " libcurl support (streaming) ...disabled"
fi

if test x$enable_mms = xyes; then
        echo " libmms support ................enabled"
else
        echo " libmms support ................disabled"
fi

echo ""
echo " Other features:"

if test x$enable_id3 = xyes; then
	echo " ID3 tag support ...............enabled"
else
	echo " ID3 tag support ...............disabled"
fi

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

if test x$enable_cue = xyes; then
        echo " libcue support ................enabled"
else
        echo " libcue support ................disabled"
fi

if test x$enable_inotify = xyes; then
        echo " Inotify support (autoupdate) ..enabled"
else
        echo " Inotify support (autoupdate) ..disabled"
fi

echo ""
echo "##########################################"
echo ""

if test x$enable_sndfile = xyes && test x$enable_modplug = xyes; then
	AC_MSG_WARN([compilation may fail, because libmodplug conflicts with libsndfile])
	AC_MSG_WARN([libmodplug ships modplug/sndfile.h, which hides libsndfile's sndfile.h])
fi

echo "Generating needed files for compilation"
echo ""

dnl
dnl generate files
dnl

AC_OUTPUT(Makefile)

echo ""

echo "You are now ready to compile MPD"
echo "Type \"make\" to compile MPD"
