AC_PREREQ(2.60)

AC_INIT(mpd, 0.17.4, musicpd-dev-team@lists.sourceforge.net)

VERSION_MAJOR=0
VERSION_MINOR=17
VERSION_REVISION=0
VERSION_EXTRA=0

AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([foreign 1.11 dist-bzip2 dist-xz subdir-objects])
AM_SILENT_RULES
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AC_DEFINE(PROTOCOL_VERSION, "0.17.0", [The MPD protocol version])


dnl ---------------------------------------------------------------------------
dnl Programs
dnl ---------------------------------------------------------------------------
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_RANLIB

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
AM_CONDITIONAL(HAVE_CXX, test x$HAVE_CXX = xyes)

AC_PROG_INSTALL
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
	    AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
	    [], [with_systemdsystemunitdir=no])
if test "x$with_systemdsystemunitdir" = xyes; then
	AC_MSG_CHECKING(for systemd)
	with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
	if test -z "$with_systemdsystemunitdir"; then
		AC_MSG_ERROR([Failed to detect systemd])
	fi
	AC_MSG_RESULT([$with_systemdsystemunitdir])
fi
if test "x$with_systemdsystemunitdir" != xno; then
	AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

dnl ---------------------------------------------------------------------------
dnl Declare Variables
dnl ---------------------------------------------------------------------------
AC_SUBST(AM_CPPFLAGS,"")
AC_SUBST(AM_CFLAGS,"")
AC_SUBST(AM_CXXFLAGS,"")

## Used for the windows resource file
AC_SUBST(VERSION_MAJOR)
AC_SUBST(VERSION_MINOR)
AC_SUBST(VERSION_REVISION)
AC_SUBST(VERSION_EXTRA)

dnl ---------------------------------------------------------------------------
dnl OS Specific Defaults
dnl ---------------------------------------------------------------------------
AC_CANONICAL_HOST

case "$host_os" in
mingw32* | windows*)
	AC_CONFIG_FILES([
		src/win/mpd_win32_rc.rc
	])
	AC_CHECK_TOOL(WINDRES, windres)
	AM_CPPFLAGS="$AM_CPPFLAGS -DWINVER=0x0501"
	LIBS="$LIBS -lws2_32"
	HAVE_WINDOWS=1
	;;
esac
AM_CONDITIONAL([HAVE_WINDOWS], [test x$HAVE_WINDOWS = x1])

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
			CPPFLAGS="$CPPFLAGS -I$d"
			break
		fi
	done
fi

dnl ---------------------------------------------------------------------------
dnl Header/Library Checks
dnl ---------------------------------------------------------------------------
AC_CHECK_FUNCS(daemon fork)

AC_SEARCH_LIBS([syslog], [bsd socket inet],
	[AC_DEFINE(HAVE_SYSLOG, 1, [Define if syslog() is available])])

AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([gethostbyname], [nsl])

AC_CHECK_FUNCS(pipe2 accept4)

AC_SEARCH_LIBS([exp], [m],,
	[AC_MSG_ERROR([exp() not found])])

AC_CHECK_HEADERS(locale.h)
AC_CHECK_HEADERS(valgrind/memcheck.h)

dnl ---------------------------------------------------------------------------
dnl Allow tools to be specifically built
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(alsa,
	AS_HELP_STRING([--enable-alsa], [enable ALSA support]),,
	[enable_alsa=auto])

AC_ARG_ENABLE(roar,
	AS_HELP_STRING([--enable-roar],
		[enable support for RoarAudio]),,
	[enable_roar=auto])

AC_ARG_ENABLE(ao,
	AS_HELP_STRING([--enable-ao],
		[enable support for libao]),,
	enable_ao=auto)

AC_ARG_ENABLE(audiofile,
	AS_HELP_STRING([--enable-audiofile],
		[enable audiofile support (WAV and others)]),,
	enable_audiofile=auto)

AC_ARG_ENABLE(bzip2,
	AS_HELP_STRING([--enable-bzip2],
		[enable bzip2 archive support (default: disabled)]),,
	enable_bzip2=no)

AC_ARG_ENABLE(cdio-paranoia,
	AS_HELP_STRING([--enable-cdio-paranoia],
		[enable support for audio CD support]),,
	enable_cdio_paranoia=auto)

AC_ARG_ENABLE(curl,
	AS_HELP_STRING([--enable-curl],
		[enable support for libcurl HTTP streaming (default: auto)]),,
	[enable_curl=auto])

AC_ARG_ENABLE(soup,
	AS_HELP_STRING([--enable-soup],
		[enable support for libsoup HTTP streaming (default: auto)]),,
	[enable_soup=auto])

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug],
		[enable debugging (default: disabled)]),,
	enable_debug=no)

AC_ARG_ENABLE(documentation,
	AS_HELP_STRING([--enable-documentation],
		[build documentation (default: disable)]),,
	[enable_documentation=no])

AC_ARG_ENABLE(ffado,
	AS_HELP_STRING([--enable-ffado], [enable libffado (FireWire) support]),,
	[enable_ffado=no])

AC_ARG_ENABLE(ffmpeg,
	AS_HELP_STRING([--enable-ffmpeg],
		[enable FFMPEG support]),,
	enable_ffmpeg=auto)

AC_ARG_ENABLE(fifo,
	AS_HELP_STRING([--disable-fifo],
		[disable support for writing audio to a FIFO (default: enable)]),,
	enable_fifo=yes)

AC_ARG_ENABLE(flac,
	AS_HELP_STRING([--enable-flac],
		[enable FLAC decoder]),,
	enable_flac=auto)

AC_ARG_ENABLE(fluidsynth,
	AS_HELP_STRING([--enable-fluidsynth],
		[enable MIDI support via fluidsynth (default: auto)]),,
	enable_fluidsynth=auto)

AC_ARG_ENABLE(gme,
	AS_HELP_STRING([--enable-gme],
		[enable Blargg's game music emulator plugin]),,
	enable_gme=auto)

AC_ARG_ENABLE(httpd-output,
	AS_HELP_STRING([--enable-httpd-output],
		[enables the HTTP server output]),,
	[enable_httpd_output=auto])

AC_ARG_ENABLE(id3,
	AS_HELP_STRING([--enable-id3],
		[enable id3 support]),,
	enable_id3=auto)

AC_ARG_ENABLE(inotify,
	AS_HELP_STRING([--disable-inotify],
		[disable support Inotify automatic database update (default: enabled) ]),,
	[enable_inotify=yes])

AC_ARG_ENABLE(ipv6,
	AS_HELP_STRING([--disable-ipv6],
		[disable IPv6 support (default: enable)]),,
	[enable_ipv6=yes])

AC_ARG_ENABLE(iso9660,
	AS_HELP_STRING([--enable-iso9660],
		[enable iso9660 archive support (default: disabled)]),,
	enable_iso9660=no)

AC_ARG_ENABLE(jack,
	AS_HELP_STRING([--enable-jack],
		[enable jack support]),,
	enable_jack=auto)

AC_SYS_LARGEFILE

AC_ARG_ENABLE(lastfm,
	AS_HELP_STRING([--enable-lastfm],
		[enable support for last.fm radio (default: disable)]),,
	[enable_lastfm=no])

AC_ARG_ENABLE(despotify,
	AS_HELP_STRING([--enable-despotify],
		[enable support for despotify (default: disable)]),,
	[enable_despotify=no])

AC_ARG_ENABLE(soundcloud,
	AS_HELP_STRING([--enable-soundcloud],
		[enable support for soundcloud.com]),,
	[enable_soundcloud=auto])

AC_ARG_ENABLE(lame-encoder,
	AS_HELP_STRING([--enable-lame-encoder],
		[enable the LAME mp3 encoder]),,
	enable_lame_encoder=auto)

AC_ARG_ENABLE([libwrap],
	AS_HELP_STRING([--enable-libwrap], [use libwrap]),,
	[enable_libwrap=auto])

AC_ARG_ENABLE(lsr,
	AS_HELP_STRING([--enable-lsr],
		[enable libsamplerate support]),,
	enable_lsr=auto)

AC_ARG_ENABLE(mad,
	AS_HELP_STRING([--enable-mad],
		[enable libmad mp3 decoder plugin]),,
	enable_mad=auto)

AC_ARG_ENABLE(mikmod,
	AS_HELP_STRING([--enable-mikmod],
		[enable the mikmod decoder (default: disable)]),,
	enable_mikmod=no)

AC_ARG_ENABLE(mms,
	AS_HELP_STRING([--enable-mms],
		[enable the MMS protocol with libmms]),,
	[enable_mms=auto])

AC_ARG_ENABLE(modplug,
	AS_HELP_STRING([--enable-modplug],
		[enable modplug decoder plugin]),,
	enable_modplug=auto)

AC_ARG_ENABLE(mpc,
	AS_HELP_STRING([--disable-mpc],
		[disable musepack (MPC) support (default: enable)]),,
	enable_mpc=yes)

AC_ARG_ENABLE(mpg123,
	AS_HELP_STRING([--enable-mpg123],
		[enable libmpg123 decoder plugin]),,
	enable_mpg123=auto)

AC_ARG_ENABLE(mvp,
	AS_HELP_STRING([--enable-mvp],
		[enable support for Hauppauge Media MVP (default: disable)]),,
	enable_mvp=no)

AC_ARG_ENABLE(openal,
	AS_HELP_STRING([--enable-openal],
		[enable OpenAL support (default: disable)]),,
	enable_openal=no)

AC_ARG_ENABLE(oss,
	AS_HELP_STRING([--disable-oss],
		[disable OSS support (default: enable)]),,
	enable_oss=yes)

AC_ARG_ENABLE(pipe-output,
	AS_HELP_STRING([--enable-pipe-output],
		[enable support for writing audio to a pipe (default: disable)]),,
	enable_pipe_output=no)

AC_ARG_ENABLE(pulse,
	AS_HELP_STRING([--enable-pulse],
		[enable support for the PulseAudio sound server]),,
	enable_pulse=auto)

AC_ARG_ENABLE(recorder-output,
	AS_HELP_STRING([--enable-recorder-output],
		[enables the recorder file output plugin (default: disable)]),,
	[enable_recorder_output=auto])

AC_ARG_ENABLE(sidplay,
	AS_HELP_STRING([--enable-sidplay],
		[enable C64 SID support via libsidplay2]),,
	enable_sidplay=auto)


AC_ARG_ENABLE(shout,
	AS_HELP_STRING([--enable-shout],
		[enables the shoutcast streaming output]),,
	[enable_shout=auto])

AC_ARG_ENABLE(sndfile,
	AS_HELP_STRING([--enable-sndfile],
		[enable sndfile support]),,
	enable_sndfile=auto)

AC_ARG_ENABLE(solaris_output,
	AS_HELP_STRING([--enable-solaris-output],
		[enables the Solaris /dev/audio output]),,
	[enable_solaris_output=auto])

AC_ARG_ENABLE(sqlite,
	AS_HELP_STRING([--enable-sqlite],
		[enable support for the SQLite database]),,
	[enable_sqlite=auto])

AC_ARG_ENABLE(systemd-daemon,
	AS_HELP_STRING([--enable-systemd-daemon],
		[use the systemd daemon library (default=auto)]),,
	[enable_systemd_daemon=auto])

AC_ARG_ENABLE(tcp,
	AS_HELP_STRING([--disable-tcp],
		[disable support for clients connecting via TCP (default: enable)]),,
	[enable_tcp=yes])

AC_ARG_ENABLE(test,
	AS_HELP_STRING([--enable-test],
		[build the test programs (default: disabled)]),,
	enable_test=no)

AC_ARG_WITH(tremor,
	AS_HELP_STRING([--with-tremor=PFX],
		[use Tremor (vorbisidec) integer Ogg Vorbis decoder (with optional prefix)]),,
	with_tremor=no)

AC_ARG_ENABLE(twolame-encoder,
	AS_HELP_STRING([--enable-twolame-encoder],
		[enable the TwoLAME mp2 encoder]),,
	enable_twolame_encoder=auto)

AC_ARG_ENABLE(un,
	AS_HELP_STRING([--disable-un],
		[disable support for clients connecting via unix domain sockets (default: enable)]),,
	[enable_un=yes])

AC_ARG_ENABLE(vorbis,
	AS_HELP_STRING([--enable-vorbis],
		[enable Ogg Vorbis decoder]),,
	enable_vorbis=auto)

AC_ARG_ENABLE(vorbis-encoder,
	AS_HELP_STRING([--enable-vorbis-encoder],
		[enable the Ogg Vorbis encoder]),,
	[enable_vorbis_encoder=auto])

AC_ARG_ENABLE(wave-encoder,
	AS_HELP_STRING([--enable-wave-encoder],
		[enable the PCM wave encoder]),,
	enable_wave_encoder=yes)

AC_ARG_ENABLE(wavpack,
	AS_HELP_STRING([--enable-wavpack],
		[enable WavPack support]),,
	enable_wavpack=auto)

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],
		[treat warnings as errors (default: disabled)]),,
	enable_werror=no)

AC_ARG_ENABLE(wildmidi,
	AS_HELP_STRING([--enable-wildmidi],
		[enable MIDI support via wildmidi (default: disable)]),,
	enable_wildmidi=no)

AC_ARG_WITH(zeroconf,
	AS_HELP_STRING([--with-zeroconf=@<:@auto|avahi|bonjour|no@:>@],
		[enable zeroconf backend (default=auto)]),,
	with_zeroconf="auto")

AC_ARG_ENABLE(zzip,
	AS_HELP_STRING([--enable-zzip],
		[enable zip archive support (default: disabled)]),,
	enable_zzip=no)


AC_ARG_WITH(tremor-libraries,
	AS_HELP_STRING([--with-tremor-libraries=DIR],
		[directory where Tremor library is installed (optional)]),,
	tremor_libraries="")

AC_ARG_WITH(tremor-includes,
	AS_HELP_STRING([--with-tremor-includes=DIR],
		[directory where Tremor header files are installed (optional)]),,
	tremor_includes="")

dnl ---------------------------------------------------------------------------
dnl Mandatory Libraries
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.16 gthread-2.0],,
		[AC_MSG_ERROR([GLib 2.16 is required])])

if test x$GCC = xyes; then
	# suppress warnings in the GLib headers
	GLIB_CFLAGS=`echo $GLIB_CFLAGS |sed -e 's,-I/,-isystem /,g'`
fi

dnl ---------------------------------------------------------------------------
dnl Protocol Options
dnl ---------------------------------------------------------------------------

if test x$enable_tcp = xno; then
	# if we don't support TCP, we don't need IPv6 either
	enable_ipv6=no
fi

if test x$enable_ipv6 = xyes; then
	AC_MSG_CHECKING(for ipv6)
	AC_EGREP_CPP([AP_maGiC_VALUE],
	[
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
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

case "$host_os" in
mingw* | windows* | cygwin*)
	enable_un=no
	;;
esac

if test x$enable_un = xyes; then
	AC_DEFINE(HAVE_UN, 1, [Define if unix domain socket support is enabled])
	STRUCT_UCRED
	AC_CHECK_FUNCS(getpeereid)
fi

dnl --------------------------- Post Protocol Tests ---------------------------
if
	test x$enable_tcp = xno &&
	test x$enable_un = xno; then
	AC_MSG_ERROR([No client interfaces configured!])
fi

MPD_AUTO_PKG(systemd_daemon, SYSTEMD_DAEMON, libsystemd-daemon,
	[systemd activation], [libsystemd-daemon not found])
AM_CONDITIONAL(ENABLE_SYSTEMD_DAEMON, test x$enable_systemd_daemon = xyes)
if test x$enable_systemd_daemon = xyes; then
	AC_DEFINE([ENABLE_SYSTEMD_DAEMON], 1, [Define to use the systemd daemon library])
fi

dnl ---------------------------------------------------------------------------
dnl LIBC Features
dnl ---------------------------------------------------------------------------
if test x$enable_largefile != xno; then
	AC_DEFINE([ENABLE_LARGEFILE], 1, [Define if large file support is enabled])
fi

dnl ---------------------------------------------------------------------------
dnl Miscellaneous Libraries
dnl ---------------------------------------------------------------------------

dnl --------------------------------- inotify ---------------------------------
AC_CHECK_FUNCS(inotify_init inotify_init1)

if test x$ac_cv_func_inotify_init = xno; then
	enable_inotify=no
fi

if test x$enable_inotify = xyes; then
	AC_DEFINE([ENABLE_INOTIFY], 1, [Define to enable inotify support])
fi
AM_CONDITIONAL(ENABLE_INOTIFY, test x$enable_inotify = xyes)

dnl --------------------------------- libwrap ---------------------------------
if test x$enable_libwrap != xno; then
	AC_CHECK_LIBWRAP(found_libwrap=yes, found_libwrap=no)
	MPD_AUTO_RESULT(libwrap, libwrap, [libwrap not found])
fi

if test x$enable_libwrap = xyes; then
	AC_SUBST(LIBWRAP_CFLAGS)
	AC_SUBST(LIBWRAP_LDFLAGS)
	AC_DEFINE(HAVE_LIBWRAP, 1, [define to enable libwrap library])
fi

dnl ---------------------------------------------------------------------------
dnl Metadata Plugins
dnl ---------------------------------------------------------------------------

dnl -------------------------------- libid3tag --------------------------------
MPD_AUTO_PKG_LIB(id3, ID3TAG, id3tag, id3tag, id3_file_open, [-lid3tag -lz], [],
	[id3tag], [libid3tag not found])
if test x$enable_id3 = xyes; then
	AC_DEFINE(HAVE_ID3TAG, 1, [Define to use id3tag])
fi

AM_CONDITIONAL(HAVE_ID3TAG, test x$enable_id3 = xyes)

dnl ---------------------------------------------------------------------------
dnl Autodiscovery
dnl ---------------------------------------------------------------------------

dnl --------------------------------- zeroconf --------------------------------

case $with_zeroconf in
no|bonjour)
	enable_avahi=no
	;;

avahi)
	enable_avahi=yes
	;;

*)
	with_zeroconf=auto
	enable_avahi=auto
	;;
esac

MPD_AUTO_PKG(avahi, AVAHI, [avahi-client avahi-glib],
	[avahi client library], [avahi client+glib not found])
if test x$enable_avahi = xyes; then
	AC_DEFINE([HAVE_AVAHI], 1, [Define to enable Avahi Zeroconf support])
	with_zeroconf=avahi
fi

AM_CONDITIONAL(HAVE_AVAHI, test x$enable_avahi = xyes)

enable_bounjour=no
if test x$with_zeroconf != xno; then
	if test x$with_zeroconf = xbonjour || test x$with_zeroconf = xauto; then
		AC_CHECK_HEADER(dns_sd.h,
			[enable_bonjour=yes;AC_DEFINE([HAVE_BONJOUR], 1, [Define to enable Bonjour Zeroconf support])])
		AC_CHECK_LIB([dns_sd], [DNSServiceRegister])
	fi

	if test x$enable_bonjour = xyes; then
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
AM_CONDITIONAL(HAVE_BONJOUR, test x$with_zeroconf = xbonjour)

dnl ---------------------------------------------------------------------------
dnl Sticker Database
dnl ---------------------------------------------------------------------------

dnl ---------------------------------- sqlite ---------------------------------

MPD_AUTO_PKG(sqlite, SQLITE, [sqlite3],
	[SQLite database support], [sqlite not found])
if test x$enable_sqlite = xyes; then
	AC_DEFINE([ENABLE_SQLITE], 1, [Define to enable sqlite database support])
fi

AM_CONDITIONAL(ENABLE_SQLITE, test x$enable_sqlite = xyes)

dnl ---------------------------------------------------------------------------
dnl Converter Plugins
dnl ---------------------------------------------------------------------------

dnl ------------------------------ libsamplerate ------------------------------
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

dnl ---------------------------------------------------------------------------
dnl Input Plugins
dnl ---------------------------------------------------------------------------

dnl ----------------------------------- CURL ----------------------------------
MPD_AUTO_PKG(curl, CURL, [libcurl],
	[libcurl HTTP streaming], [libcurl not found])
if test x$enable_curl = xyes; then
	AC_DEFINE(ENABLE_CURL, 1, [Define when libcurl is used for HTTP streaming])
fi
AM_CONDITIONAL(ENABLE_CURL, test x$enable_curl = xyes)

dnl ----------------------------------- SOUP ----------------------------------
MPD_AUTO_PKG(soup, SOUP, [libsoup-2.4],
	[libsoup HTTP streaming], [libsoup not found])
if test x$enable_soup = xyes; then
	AC_DEFINE(ENABLE_SOUP, 1, [Define when libsoup is used for HTTP streaming])
fi
AM_CONDITIONAL(ENABLE_SOUP, test x$enable_soup = xyes)

dnl --------------------------------- Last.FM ---------------------------------
if test x$enable_lastfm = xyes; then
	if test x$enable_curl != xyes; then
		AC_MSG_ERROR([Cannot enable last.fm radio without curl])
	fi

	AC_DEFINE(ENABLE_LASTFM, 1, [Define when last.fm radio is enabled])
fi
AM_CONDITIONAL(ENABLE_LASTFM, test x$enable_lastfm = xyes)

dnl --------------------------------- Despotify ---------------------------------
MPD_AUTO_PKG(despotify, DESPOTIFY, [despotify],
	[Despotify support], [despotify not found])
if test x$enable_despotify = xyes; then
	AC_DEFINE(ENABLE_DESPOTIFY, 1, [Define when despotify is enabled])
fi
AM_CONDITIONAL(ENABLE_DESPOTIFY, test x$enable_despotify = xyes)

dnl --------------------------------- Soundcloud ------------------------------
if test x$enable_soundcloud != xno; then
	PKG_CHECK_MODULES([YAJL], [yajl >= 2.0],
		[found_soundcloud=yes],
		AC_CHECK_LIB([yajl], [yajl_alloc],
			[found_soundcloud=yes YAJL_CFLAGS=-DHAVE_YAJL1 YAJL_LIBS=-lyajl],
			[found_soundcloud=no]))
fi
MPD_AUTO_RESULT([soundcloud], [soundcloud.com support], [libyajl not found])
if test x$enable_soundcloud = xyes; then
	AC_DEFINE(ENABLE_SOUNDCLOUD, 1, [Define when soundcloud is enabled])
fi
AM_CONDITIONAL(ENABLE_SOUNDCLOUD, test x$enable_soundcloud = xyes)
AC_SUBST(YAJL_LIBS)

dnl ---------------------------------- cdio ---------------------------------
MPD_AUTO_PKG(cdio_paranoia, CDIO_PARANOIA, [libcdio_paranoia],
	[libcdio_paranoia audio CD library], [libcdio_paranoia not found])
if test x$enable_cdio_paranoia = xyes; then
	AC_DEFINE([ENABLE_CDIO_PARANOIA], 1,
		[Define to enable libcdio_paranoia support])
fi

AM_CONDITIONAL(ENABLE_CDIO_PARANOIA, test x$enable_cdio_paranoia = xyes)

dnl ---------------------------------- libmms ---------------------------------
MPD_AUTO_PKG(mms, MMS, [libmms >= 0.4],
	[libmms mms:// protocol support], [libmms not found])
if test x$enable_mms = xyes; then
	AC_DEFINE(ENABLE_MMS, 1,
		[Define when libmms is used for the MMS protocol])
fi
AM_CONDITIONAL(ENABLE_MMS, test x$enable_mms = xyes)

dnl ---------------------------------------------------------------------------
dnl Archive Plugins
dnl ---------------------------------------------------------------------------

dnl --------------------------------- iso9660 ---------------------------------
MPD_AUTO_PKG(iso9660, ISO9660, [libiso9660],
	[libiso9660 archive library], [libiso9660 not found])

AM_CONDITIONAL(HAVE_ISO9660, test x$enable_iso9660 = xyes)
if test x$enable_iso9660 = xyes; then
	AC_DEFINE(HAVE_ISO9660, 1, [Define to have ISO9660 archive support])

	AC_PATH_PROG(MKISOFS, mkisofs, no)
else
	MKISOFS="no"
fi

AM_CONDITIONAL(ENABLE_ISO9660_TEST, test x$MKISOFS != xno)

dnl ---------------------------------- libbz2 ---------------------------------
if test x$enable_bzip2 = xyes; then
	AC_CHECK_LIB(bz2, BZ2_bzDecompressInit,
		[BZ2_LIBS="-lbz2"],
		[AC_MSG_ERROR([libbz2 not found])])
fi
AC_SUBST(BZ2_LIBS)

AM_CONDITIONAL(HAVE_BZ2, test x$enable_bzip2 = xyes)
if test x$enable_bzip2 = xyes; then
	AC_DEFINE(HAVE_BZ2, 1, [Define to have bz2 archive support])

	AC_PATH_PROG(BZIP2, bzip2, no)
else
	BZIP2="no"
fi

AM_CONDITIONAL(ENABLE_BZIP2_TEST, test x$BZIP2 != xno)

dnl --------------------------------- libzzip ---------------------------------
MPD_AUTO_PKG(zzip, ZZIP, [zziplib >= 0.13],
	[libzzip archive library], [libzzip not found])

AM_CONDITIONAL(HAVE_ZZIP, test x$enable_zzip = xyes)
if test x$enable_zzip = xyes; then
	AC_DEFINE(HAVE_ZZIP, 1, [Define to have zip archive support])

	AC_PATH_PROG(ZIP, zip, no)
else
	ZIP="no"
fi

AM_CONDITIONAL(ENABLE_ZZIP_TEST, test x$ZIP != xno)

dnl ------------------------------- Archive API -------------------------------
if
	test x$enable_bzip2 = xyes ||
	test x$enable_zzip = xyes ||
	test x$enable_iso9660 = xyes; then
		enable_archive=yes
		AC_DEFINE(ENABLE_ARCHIVE, 1, [The archive API is available])
else
	enable_archive=no
fi

AM_CONDITIONAL(ENABLE_ARCHIVE, test x$enable_archive = xyes)

dnl ---------------------------------------------------------------------------
dnl Decoder Plugins
dnl ---------------------------------------------------------------------------

dnl -------------------------------- audiofile --------------------------------
MPD_AUTO_PKG(audiofile, AUDIOFILE, [audiofile >= 0.1.7],
	[audiofile decoder plugin], [libaudiofile not found])
AM_CONDITIONAL(HAVE_AUDIOFILE, test x$enable_audiofile = xyes)
if test x$enable_audiofile = xyes; then
	AC_DEFINE(HAVE_AUDIOFILE, 1, [Define for audiofile support])
fi

dnl ----------------------------------- FAAD ----------------------------------
AM_PATH_FAAD()

AM_CONDITIONAL(HAVE_FAAD, test x$enable_aac = xyes)
AM_CONDITIONAL(HAVE_MP4, test x$enable_mp4 = xyes)

dnl ---------------------------------- ffmpeg ---------------------------------
MPD_AUTO_PKG(ffmpeg, FFMPEG, [libavformat >= 52.31 libavcodec >= 52.20 libavutil >= 49.15],
	[ffmpeg decoder library], [libavformat+libavcodec+libavutil not found])

if test x$enable_ffmpeg = xyes; then
	AC_DEFINE(HAVE_FFMPEG, 1, [Define for FFMPEG support])
fi

AM_CONDITIONAL(HAVE_FFMPEG, test x$enable_ffmpeg = xyes)

dnl ----------------------------------- FLAC ----------------------------------

MPD_AUTO_PKG(flac, FLAC, [flac >= 1.1],
	[FLAC decoder], [libFLAC not found])

if test x$enable_flac = xyes; then
	AC_DEFINE(HAVE_FLAC, 1, [Define for FLAC support])
fi

AM_CONDITIONAL(HAVE_FLAC, test x$enable_flac = xyes)

enable_flac_encoder=$enable_flac

dnl -------------------------------- FluidSynth -------------------------------
if test x$enable_fluidsynth = xyes; then
	PKG_CHECK_MODULES(FLUIDSYNTH, [fluidsynth >= 1.1],
		AC_DEFINE(ENABLE_FLUIDSYNTH, 1, [Define for fluidsynth support]),
		enable_fluidsynth=no)
fi

AM_CONDITIONAL(ENABLE_FLUIDSYNTH, test x$enable_fluidsynth = xyes)

dnl ---------------------------------- libgme ---------------------------------
MPD_AUTO_PKG_LIB(gme, GME, [libgme], gme, gme_open_file, [-lgme -lstdc++], [],
	[gme decoder plugin], [libgme not found])
AM_CONDITIONAL(HAVE_GME, test x$enable_gme = xyes)
if test x$enable_gme = xyes; then
	AC_DEFINE(HAVE_GME, 1, [Define for gme support])
fi

dnl ---------------------------------- libmad ---------------------------------
MPD_AUTO_PKG_LIB(mad, MAD, [mad],
	mad, mad_stream_init, [-lmad], [],
	[libmad MP3 decoder plugin], [libmad not found])
if test x$enable_mad = xyes; then
	AC_DEFINE(HAVE_MAD, 1, [Define to use libmad])
fi
AM_CONDITIONAL(HAVE_MAD, test x$enable_mad = xyes)

enable_shout2="$enable_shout"
MPD_AUTO_PKG(shout, SHOUT, [shout],
	[shout output plugin], [libshout not found])
if test x$enable_shout = xyes && test x$enable_shout2 = xauto; then
	enable_shout=auto
fi

dnl -------------------------------- libmpg123 --------------------------------
MPD_AUTO_PKG(mpg123, MPG123, [libmpg123],
	[libmpg123 decoder plugin], [libmpg123 not found])
if test x$enable_mpg123 = xyes; then
	AC_DEFINE(HAVE_MPG123, 1, [Define to use libmpg123])
fi
AM_CONDITIONAL(HAVE_MPG123, test x$enable_mpg123 = xyes)

dnl -------------------------------- libmikmod --------------------------------
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

dnl -------------------------------- libmodplug -------------------------------
found_modplug=$HAVE_CXX
MPD_AUTO_PRE(modplug, [modplug decoder plugin], [No C++ compiler found])

MPD_AUTO_PKG(modplug, MODPLUG, [libmodplug],
	[modplug decoder plugin], [libmodplug not found])

if test x$enable_modplug = xyes; then
	AC_DEFINE(HAVE_MODPLUG, 1, [Define for modplug support])
fi
AM_CONDITIONAL(HAVE_MODPLUG, test x$enable_modplug = xyes)

dnl -------------------------------- libsndfile -------------------------------
dnl See above test, which may disable this.
MPD_AUTO_PKG(sndfile, SNDFILE, [sndfile],
	[libsndfile decoder plugin], [libsndfile not found])

if test x$enable_sndfile = xyes; then
	AC_DEFINE(ENABLE_SNDFILE, 1, [Define to enable the sndfile decoder plugin])
fi
AM_CONDITIONAL(ENABLE_SNDFILE, test x$enable_sndfile = xyes)

dnl --------------------------------- musepack --------------------------------
if test x$enable_mpc = xyes; then
	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	AC_CHECK_LIB(mpcdec,main,
			MPCDEC_LIBS="$MPCDEC_LIBS -lmpcdec",
			enable_mpc=no)
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags

	if test x$enable_mpc = xyes; then
		AC_CHECK_HEADER([mpc/mpcdec.h],
			[AC_DEFINE(HAVE_MPCDEC,1,
				[Define to use libmpcdec for MPC decoding])],
			[AC_CHECK_HEADER(mpcdec/mpcdec.h,
				[AC_DEFINE(MPC_IS_OLD_API, 1,
				[Define if an old pre-SV8 libmpcdec is used])]
				)]
		)	
	else
		AC_MSG_WARN([mpcdec lib needed for MPC support -- disabling MPC support])
	fi
fi

AC_SUBST(MPCDEC_LIBS)
AC_SUBST(MPCDEC_CFLAGS)
AM_CONDITIONAL(HAVE_MPCDEC, test x$enable_mpc = xyes)

dnl -------------------------------- Ogg Tremor -------------------------------
if test x$with_tremor = xyes || test x$with_tremor = xno; then
	enable_tremor="$with_tremor"
else
	tremor_prefix="$with_tremor"
	enable_tremor=yes
fi

if test x$enable_tremor = xyes; then
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
	AC_CHECK_LIB(vorbisidec,ov_read,,enable_tremor=no;
		AC_MSG_WARN([vorbisidec lib needed for ogg support with tremor -- disabling ogg support]))
	CFLAGS="$ac_save_CFLAGS"
	LIBS="$ac_save_LIBS"
fi

if test x$enable_tremor = xyes; then
	AC_DEFINE(HAVE_TREMOR,1,
		[Define to use tremor (libvorbisidec) for ogg support])
	AC_DEFINE(ENABLE_VORBIS_DECODER, 1, [Define for Ogg Vorbis support])
else
	TREMOR_CFLAGS=
	TREMOR_LIBS=
fi

AC_SUBST(TREMOR_CFLAGS)
AC_SUBST(TREMOR_LIBS)

dnl -------------------------------- Ogg Vorbis -------------------------------

if test x$enable_tremor = xyes; then
	if test x$enable_vorbis = xyes; then
		AC_MSG_WARN(["OggTremor detected, could not enable Vorbis."])
	fi
	enable_vorbis=no

	if test x$enable_vorbis_encoder = xauto; then
		AC_MSG_WARN([OggTremor detected, disabling the Vorbis encoder plugin.])
		enable_vorbis_encoder=no
	fi
fi

MPD_AUTO_PKG(vorbis, VORBIS, [vorbis vorbisfile ogg],
	[Ogg Vorbis decoder], [libvorbis not found])
if test x$enable_vorbis = xyes; then
	AC_DEFINE(ENABLE_VORBIS_DECODER, 1, [Define for Ogg Vorbis support])
fi

AM_CONDITIONAL(ENABLE_VORBIS_DECODER, test x$enable_vorbis = xyes || test x$enable_tremor = xyes)

dnl --------------------------------- sidplay ---------------------------------
found_sidplay=$HAVE_CXX
MPD_AUTO_PRE(sidplay, [sidplay decoder plugin], [No C++ compiler found])

if test x$enable_sidplay != xno; then
	# we're not using pkg-config here
	# because libsidplay2's .pc file requires libtool
	AC_CHECK_LIB([sidplay2],[main],[found_sidplay=yes],[found_sidplay=no],[])

	MPD_AUTO_PRE(sidplay, [sidplay decoder plugin],
		[libsidplay2 not found])
fi

if test x$enable_sidplay != xno; then
	AC_CHECK_LIB([resid-builder], [main],
		[found_sidplay=yes], [found_sidplay=no])

	if test x$found_sidplay = xyes; then
		AC_CHECK_LIB([sidutils],[main],[],[found_sidplay=no],[])
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

dnl --------------------------------- wavpack ---------------------------------
MPD_AUTO_PKG(wavpack, WAVPACK, [wavpack],
	[WavPack decoder plugin], [libwavpack not found])
AM_CONDITIONAL(HAVE_WAVPACK, test x$enable_wavpack = xyes)
if test x$enable_wavpack = xyes; then
	AC_DEFINE([HAVE_WAVPACK], 1, [Define to enable WavPack support])
fi

dnl --------------------------------- WildMidi --------------------------------
if test x$enable_wildmidi = xyes; then
	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS

	AC_CHECK_LIB(WildMidi, WildMidi_Init,,
		AC_MSG_ERROR([libwildmidi not found]))

	AC_CHECK_LIB(WildMidi, WildMidi_SampledSeek,
		[AC_DEFINE(HAVE_WILDMIDI_SAMPLED_SEEK, 1,
			[Defined if WildMidi_SampledSeek() is available (libwildmidi <= 0.2.2)])])

	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags

	AC_SUBST(WILDMIDI_LIBS,-lWildMidi)
	AC_SUBST(WILDMIDI_CFLAGS,)

	AC_DEFINE(ENABLE_WILDMIDI, 1, [Define for wildmidi support])
fi

AM_CONDITIONAL(ENABLE_WILDMIDI, test x$enable_wildmidi = xyes)

dnl ------------------------ Post Decoder Plugins Tests -----------------------

if
	test x$enable_aac = xno &&
	test x$enable_audiofile = xno &&
	test x$enable_ffmpeg = xno &&
	test x$enable_flac = xno &&
	test x$enable_fluidsynth = xno &&
	test x$enable_mad = xno &&
	test x$enable_mikmod = xno; then
	test x$enable_modplug = xno &&
	test x$enable_mp4 = xno &&
	test x$enable_mpc = xno &&
	test x$enable_mpg123 = xno &&
	test x$enable_sidplay = xno &&
	test x$enable_tremor = xno &&
	test x$enable_vorbis = xno &&
	test x$enable_wavpack = xno &&
	test x$enable_wildmidi = xno &&

		AC_MSG_ERROR([No input plugins supported!])
fi

AM_CONDITIONAL(HAVE_OGG_COMMON,
	test x$enable_vorbis = xyes || test x$enable_tremor = xyes || test x$enable_flac = xyes)

AM_CONDITIONAL(HAVE_FLAC_COMMON,
	  test x$enable_flac = xyes)

dnl ---------------------------------------------------------------------------
dnl Encoders for Streaming Audio Output Plugins
dnl ---------------------------------------------------------------------------

dnl ------------------------------- Encoder API -------------------------------
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

dnl ------------------------------- FLAC Encoder ------------------------------
if test x$enable_flac_encoder = xyes; then
	AC_DEFINE(ENABLE_FLAC_ENCODER, 1,
		[Define to enable the FLAC encoder plugin])
fi
AM_CONDITIONAL(ENABLE_FLAC_ENCODER, test x$enable_flac_encoder = xyes)

dnl ---------------------------- Ogg Vorbis Encoder ---------------------------
MPD_AUTO_PKG(vorbis_encoder, VORBISENC, [vorbisenc],
	[Ogg Vorbis encoder], [libvorbisenc not found])

if test x$enable_vorbis_encoder = xyes; then
	AC_DEFINE(ENABLE_VORBIS_ENCODER, 1,
		[Define to enable the vorbis encoder plugin])
fi
AM_CONDITIONAL(ENABLE_VORBIS_ENCODER, test x$enable_vorbis_encoder = xyes)

dnl ------------------------------- LAME Encoder ------------------------------
if test x$enable_lame_encoder != xno; then
	AC_CHECK_HEADERS(lame/lame.h,,
		[AC_CHECK_HEADERS(lame.h,, using_lame=no)])
	AC_CHECK_LIB(mp3lame, lame_init,, using_lame=no)
	if test x$using_lame != xno; then
		AC_DEFINE(HAVE_LAME, 1, [Define to 1 if you have lame 3.98 or greater.])
		LAME_LIBS="-lmp3lame -lm"
		enable_lame_encoder=yes
	fi

	if test "$enable_lame_encoder" = "yes" -a "$using_lame" = "no"; then
		AC_MSG_ERROR([LAME libraries and development support files not found.])
	fi
fi

AC_SUBST(LAME_LIBS)

if test x$enable_lame_encoder = xyes; then
	AC_DEFINE(ENABLE_LAME_ENCODER, 1,
		[Define to enable the lame encoder plugin])
fi
AM_CONDITIONAL(ENABLE_LAME_ENCODER, test x$enable_lame_encoder = xyes)

dnl ----------------------------- TwoLAME Encoder -----------------------------
MPD_AUTO_PKG(twolame_encoder, TWOLAME, [twolame],
	[TwoLAME encoder], [libtwolame not found])

if test x$enable_twolame_encoder = xyes; then
	AC_DEFINE(ENABLE_TWOLAME_ENCODER, 1,
		[Define to enable the TwoLAME encoder plugin])
fi
AM_CONDITIONAL(ENABLE_TWOLAME_ENCODER, test x$enable_twolame_encoder = xyes)

dnl ------------------------------- WAVE Encoder ------------------------------
AM_CONDITIONAL(ENABLE_WAVE_ENCODER, test x$enable_wave_encoder = xyes)
if test x$enable_wave_encoder = xyes; then
	AC_DEFINE(ENABLE_WAVE_ENCODER, 1,
		[Define to enable the PCM wave encoder plugin])
fi

dnl --------------------------- encoder plugins test --------------------------
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

if test x$enable_encoder = xyes; then
	AC_DEFINE(ENABLE_ENCODER, 1,
		[Define to enable the encoder plugins])
fi
AM_CONDITIONAL(ENABLE_ENCODER, test x$enable_encoder = xyes)

dnl ---------------------------------------------------------------------------
dnl Audio Output Plugins
dnl ---------------------------------------------------------------------------

dnl ----------------------------------- ALSA ----------------------------------
MPD_AUTO_PKG(alsa, ALSA, [alsa >= 0.9.0],
	[ALSA output plugin], [libasound not found])

if test x$enable_alsa = xyes; then
	AC_DEFINE(HAVE_ALSA, 1, [Define to enable ALSA support])
fi

AM_CONDITIONAL(HAVE_ALSA, test x$enable_alsa = xyes)

dnl ----------------------------------- ROAR ----------------------------------
MPD_AUTO_PKG(roar, ROAR, [libroar >= 0.4.0],
	[ROAR output plugin], [libroar not found])

if test x$enable_roar = xyes; then
	AC_DEFINE(HAVE_ROAR, 1, [Define to enable ROAR support])
fi

AM_CONDITIONAL(HAVE_ROAR, test x$enable_roar = xyes)

dnl ----------------------------------- FFADO ---------------------------------

MPD_AUTO_PKG(ffado, FFADO, [libffado],
	[libffado output plugin], [libffado not found])

if test x$enable_ffado = xyes; then
	AC_DEFINE(ENABLE_FFADO_OUTPUT, 1, [Define to enable the libffado output plugin])
fi

AM_CONDITIONAL(ENABLE_FFADO_OUTPUT, test x$enable_ffado = xyes)

dnl ----------------------------------- FIFO ----------------------------------
if test x$enable_fifo = xyes; then
	AC_CHECK_FUNC([mkfifo],
		[enable_fifo=yes;AC_DEFINE([HAVE_FIFO], 1,
			[Define to enable support for writing audio to a FIFO])],
		[enable_fifo=no;AC_MSG_WARN([mkfifo not found -- disabling support for writing audio to a FIFO])])
fi

AM_CONDITIONAL(HAVE_FIFO, test x$enable_fifo = xyes)

dnl ------------------------------- HTTPD Output ------------------------------
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

if test x$enable_httpd_output = xyes; then
	AC_DEFINE(ENABLE_HTTPD_OUTPUT, 1, [Define to enable the HTTP server output])
fi
AM_CONDITIONAL(ENABLE_HTTPD_OUTPUT, test x$enable_httpd_output = xyes)

dnl ----------------------------------- JACK ----------------------------------
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

dnl ---------------------------------- libao ----------------------------------
MPD_AUTO_PKG(ao, AO, [ao],
	[libao output plugin], [libao not found])
if test x$enable_ao = xyes; then
	AC_DEFINE(HAVE_AO, 1, [Define to play with ao])
fi

AM_CONDITIONAL(HAVE_AO, test x$enable_ao = xyes)

dnl ----------------------------------- MVP -----------------------------------
if test x$enable_mvp = xyes; then
   AC_DEFINE(HAVE_MVP,1,[Define to enable Hauppauge Media MVP support])
fi

AM_CONDITIONAL(HAVE_MVP, test x$enable_mvp = xyes)

dnl ---------------------------------- OpenAL ---------------------------------
AC_SUBST(OPENAL_CFLAGS,"")
AC_SUBST(OPENAL_LIBS,"")

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

AM_CONDITIONAL(HAVE_OPENAL, test x$enable_openal = xyes)

dnl ---------------------------- Open Sound System ----------------------------
if test x$enable_oss = xyes; then
	AC_CHECK_HEADER(sys/soundcard.h,
		[enable_oss=yes;AC_DEFINE(HAVE_OSS,1,[Define to enable OSS])],
		[AC_MSG_WARN(Soundcard headers not found -- disabling OSS support);
			enable_oss=no])
fi

AM_CONDITIONAL(HAVE_OSS, test x$enable_oss = xyes)

dnl ----------------------------------- OSX -----------------------------------
enable_osx=no
case "$host_os" in
	darwin*)
		AC_DEFINE(HAVE_OSX, 1, [Define for compiling OS X support])
		LIBS="$LIBS -framework AudioUnit -framework CoreAudio -framework CoreServices"
		enable_osx=yes ;;
esac

AM_CONDITIONAL(HAVE_OSX, test x$enable_osx = xyes)

dnl ------------------------------- Pipe Output -------------------------------
if test x$enable_pipe_output = xyes; then
	AC_DEFINE([ENABLE_PIPE_OUTPUT], 1,
		[Define to enable support for writing audio to a pipe])
fi
AM_CONDITIONAL(ENABLE_PIPE_OUTPUT, test x$enable_pipe_output = xyes)

dnl -------------------------------- PulseAudio -------------------------------
MPD_AUTO_PKG(pulse, PULSE, [libpulse],
	[PulseAudio output plugin], [libpulse not found])
if test x$enable_pulse = xyes; then
	AC_DEFINE([HAVE_PULSE], 1,
		[Define to enable PulseAudio support])
fi

AM_CONDITIONAL(HAVE_PULSE, test x$enable_pulse = xyes)

dnl --------------------------------- Recorder --------------------------------
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

if test x$enable_recorder_output = xyes; then
	AC_DEFINE(ENABLE_RECORDER_OUTPUT, 1, [Define to enable the recorder output])
fi
AM_CONDITIONAL(ENABLE_RECORDER_OUTPUT, test x$enable_recorder_output = xyes)

dnl -------------------------------- SHOUTcast --------------------------------
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

if test x$enable_shout = xyes; then
	AC_DEFINE(HAVE_SHOUT, 1, [Define to enable the shoutcast output])
fi
AM_CONDITIONAL(HAVE_SHOUT, test x$enable_shout = xyes)

dnl --------------------------------- Solaris ---------------------------------

if test x$enable_solaris_output = xauto; then
	case "$host_os" in
	solaris*)
		enable_solaris_output=yes
		;;

	*)
		enable_solaris_output=no
		;;
	esac
fi

if test x$enable_solaris_output = xyes; then
	AC_DEFINE(ENABLE_SOLARIS_OUTPUT, 1, [Define to enable Solaris /dev/audio support])
fi

AM_CONDITIONAL(ENABLE_SOLARIS_OUTPUT, test x$enable_solaris_output = xyes)

dnl --------------------------------- WinMM ---------------------------------

case "$host_os" in
	mingw32* | windows*)
		AC_DEFINE(ENABLE_WINMM_OUTPUT, 1, [Define to enable WinMM support])
		enable_winmm_output=yes
		LIBS="$LIBS -lwinmm"
		;;

	*)
		enable_winmm_output=no
		;;
esac

AM_CONDITIONAL(ENABLE_WINMM_OUTPUT, test x$enable_winmm_output = xyes)

dnl --------------------- Post Audio Output Plugins Tests ---------------------
if
	test x$enable_alsa = xno &&
	test x$enable_roar = xno &&
	test x$enable_ao = xno &&
	test x$enable_ffado = xno &&
	test x$enable_fifo = xno &&
	test x$enable_httpd_output = xno &&
	test x$enable_jack = xno &&
	test x$enable_mvp = xno; then
	test x$enable_openal = xno &&
	test x$enable_oss = xno &&
	test x$enable_osx = xno &&
	test x$enable_pipe_output = xno &&
	test x$enable_pulse = xno &&
	test x$enable_recorder_output = xno &&
	test x$enable_shout = xno &&
	test x$enable_solaris_output = xno &&
	test x$enable_winmm_output = xno &&

		AC_MSG_ERROR([No Audio Output types configured!])
fi

dnl ---------------------------------------------------------------------------
dnl Documentation
dnl ---------------------------------------------------------------------------
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

dnl ---------------------------------------------------------------------------
dnl test suite
dnl ---------------------------------------------------------------------------
AM_CONDITIONAL(ENABLE_TEST, test "x$enable_test" = xyes)

dnl ---------------------------------------------------------------------------
dnl CFLAGS
dnl ---------------------------------------------------------------------------

dnl ---------------------------------- debug ----------------------------------
if test "x$enable_debug" = xno; then
	AM_CPPFLAGS="$AM_CPPFLAGS -DNDEBUG"
fi

dnl ----------------------------------- GCC -----------------------------------
if test x$GCC = xyes
then
	AX_APPEND_COMPILE_FLAGS([-Wall])
	AX_APPEND_COMPILE_FLAGS([-Wextra])
	AX_APPEND_COMPILE_FLAGS([-Wmissing-prototypes])
	AX_APPEND_COMPILE_FLAGS([-Wshadow])
	AX_APPEND_COMPILE_FLAGS([-Wpointer-arith])
	AX_APPEND_COMPILE_FLAGS([-Wstrict-prototypes])
	AX_APPEND_COMPILE_FLAGS([-Wcast-qual])
	AX_APPEND_COMPILE_FLAGS([-Wwrite-strings])
	AX_APPEND_COMPILE_FLAGS([-pedantic])
fi

dnl ---------------------------- warnings as errors ---------------------------
if test "x$enable_werror" = xyes; then
	AM_CFLAGS="$AM_CFLAGS -Werror -pedantic-errors"
	AM_CXXFLAGS="$AM_CXXFLAGS -Werror"
fi

dnl ---------------------------------------------------------------------------
dnl Pretty-Print Results
dnl ---------------------------------------------------------------------------
echo ''
echo '########### MPD CONFIGURATION ############'

printf '\nArchive support:\n\t'
results(bzip2,[bzip2])
results(iso9660,[ISO9660])
results(zzip,[ZIP])

if test x$with_zeroconf != xno; then
	printf '\nAutodiscovery support:\n\t'
	results(avahi, [Avahi])
	results(bonjour, [Bonjour])
fi

printf '\nClient support:\n\t'
results(ipv6, "IPv6")
results(tcp, "TCP")
results(un,[UNIX Domain Sockets])

printf '\nFile format support:\n\t'
results(aac, [AAC])
results(sidplay, [C64 SID])
results(ffmpeg, [FFMPEG])
results(flac, [FLAC])
results(fluidsynth, [FluidSynth])
results(gme, [GME])
results(sndfile, [libsndfile])
printf '\n\t'
results(mikmod, [MikMod])
results(modplug, [MODPLUG])
results(mad, [MAD])
results(mpg123, [MPG123])
results(mp4, [MP4])
results(mpc, [Musepack])
printf '\n\t'
results(tremor, [OggTremor])
results(vorbis, [OggVorbis])
results(audiofile, [WAVE])
results(wavpack, [WavPack])
results(wildmidi, [WildMidi])

printf '\nOther features:\n\t'
results(lsr, [libsamplerate])
results(inotify, [inotify])
results(sqlite, [SQLite])

printf '\nMetadata support:\n\t'
results(id3,[ID3])

printf '\nPlayback support:\n\t'
results(alsa,ALSA)
results(ffado,FFADO)
results(fifo,FIFO)
results(recorder_output,[File Recorder])
results(httpd_output,[HTTP Daemon])
results(jack,[JACK])
printf '\n\t'
results(ao,[libao])
results(mvp, [Media MVP])
results(oss,[OSS])
results(openal,[OpenAL])
results(osx, [OS X])
results(pipe_output, [Pipeline])
printf '\n\t'
results(pulse, [PulseAudio])
results(roar,[ROAR])
results(shout, [SHOUTcast])
results(solaris_output, [Solaris])
results(winmm_output, [WinMM])

if
	test x$enable_shout = xyes ||
	test x$enable_recorder = xyes ||
	test x$enable_httpd_output = xyes; then
		printf '\nStreaming encoder support:\n\t'
		results(flac_encoder, [FLAC])
		results(lame_encoder, [LAME])
		results(vorbis_encoder, [Ogg Vorbis])
		results(twolame_encoder, [TwoLAME])
		results(wave_encoder, [WAVE])
fi

printf '\nStreaming support:\n\t'
results(cdio_paranoia, [CDIO_PARANOIA])
results(curl,[CURL])
results(despotify,[Despotify])
results(lastfm,[Last.FM])
results(soundcloud,[Soundcloud])
printf '\n\t'
results(mms,[MMS])
results(soup, [SOUP])

printf '\n\n##########################################\n\n'

echo 'Generating files needed for compilation'

dnl ---------------------------------------------------------------------------
dnl Generate files
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(doc/doxygen.conf)
AC_CONFIG_FILES(mpd.service)
AC_OUTPUT

echo 'MPD is ready for compilation, type "make" to begin.'
