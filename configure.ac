AC_PREREQ(2.60)

AC_INIT(mpd, 0.19.10, musicpd-dev-team@lists.sourceforge.net)

VERSION_MAJOR=0
VERSION_MINOR=19
VERSION_REVISION=10
VERSION_EXTRA=0

AC_CONFIG_SRCDIR([src/Main.cxx])
AC_CONFIG_AUX_DIR(build)
AM_INIT_AUTOMAKE([foreign 1.11 dist-xz subdir-objects])
AM_SILENT_RULES
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AC_DEFINE(PROTOCOL_VERSION, "0.19.0", [The MPD protocol version])

GIT_COMMIT=`GIT_DIR="$srcdir/.git" git describe --dirty --always 2>/dev/null`
if test x$GIT_COMMIT != x; then
	AC_DEFINE_UNQUOTED(GIT_COMMIT, ["$GIT_COMMIT"], [The current git commit])
fi

dnl ---------------------------------------------------------------------------
dnl Programs
dnl ---------------------------------------------------------------------------
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_RANLIB

AN_MAKEVAR([AR], [AC_PROG_AR])
AN_PROGRAM([ar], [AC_PROG_AR])
AC_DEFUN([AC_PROG_AR], [AC_CHECK_TOOL(AR, ar, :)])
AC_PROG_AR

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

host_is_unix=yes
host_is_linux=no
host_is_android=no
host_is_darwin=no
host_is_solaris=no
host_is_windows=no

linux_auto=no

case "$host_os" in
linux-android*)
	host_is_android=yes
	host_is_linux=yes
	linux_auto=auto
	AM_CPPFLAGS="$AM_CPPFLAGS -DANDROID"
	;;

linux*)
	host_is_linux=yes
	linux_auto=auto

	dnl allow using all glibc features
	CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
	;;

mingw32* | windows*)
	AC_CONFIG_FILES([
		src/win32/mpd_win32_rc.rc
	])
	AC_CHECK_TOOL(WINDRES, windres)
	AM_CPPFLAGS="$AM_CPPFLAGS -DWIN32_LEAN_AND_MEAN"
	AM_CPPFLAGS="$AM_CPPFLAGS -DWINVER=0x0600 -D_WIN32_WINNT=0x0600"
	LIBS="$LIBS -lws2_32"
	host_is_windows=yes
	host_is_unix=no
	;;

darwin*)
	host_is_darwin=yes
	;;

solaris*)
	host_is_solaris=yes
	;;
esac

AM_CONDITIONAL([ANDROID], [test x$host_is_android = xyes])
AM_CONDITIONAL([HAVE_WINDOWS], [test x$host_is_windows = xyes])

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
dnl Android
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([android-sdk],
	AS_HELP_STRING([--with-android-sdk=DIR],
		[Directory for Android SDK]),
		[], [with_android_sdk=no])

if test x$host_is_android = xyes; then
	if test x$with_android_sdk = xno; then
		AC_MSG_ERROR([Android build requires option --with-android-sdk=DIR])
	fi

	if ! test -x $with_android_sdk/tools/android; then
		AC_MSG_ERROR([Android SDK not found in $with_android_sdk])
	fi
fi

AC_SUBST(ANDROID_SDK, [$with_android_sdk])

dnl ---------------------------------------------------------------------------
dnl Language Checks
dnl ---------------------------------------------------------------------------

AC_CXX_COMPILE_STDCXX_0X
if test "$ax_cv_cxx_compile_cxx0x_native" != yes; then
	if test "$ax_cv_cxx_compile_cxx0x_gxx" = yes; then
	   	AM_CXXFLAGS="$AM_CXXFLAGS -std=gnu++0x"
	elif test "$ax_cv_cxx_compile_cxx0x_cxx" = yes; then
	   	AM_CXXFLAGS="$AM_CXXFLAGS -std=c++0x"
	fi
fi

dnl ---------------------------------------------------------------------------
dnl Header/Library Checks
dnl ---------------------------------------------------------------------------

AC_SEARCH_LIBS([clock_gettime], [rt])

AC_SEARCH_LIBS([syslog], [bsd socket inet],
	[AC_DEFINE(HAVE_SYSLOG, 1, [Define if syslog() is available])])

AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([gethostbyname], [nsl])

if test x$host_is_linux = xyes; then
	AC_CHECK_FUNCS(pipe2 accept4)
fi

AC_CHECK_FUNCS(getpwnam_r getpwuid_r)
AC_CHECK_FUNCS(strndup)

if test x$host_is_linux = xyes; then
	MPD_OPTIONAL_FUNC(eventfd, eventfd, USE_EVENTFD)
	MPD_OPTIONAL_FUNC(signalfd, signalfd, USE_SIGNALFD)
fi

AC_SEARCH_LIBS([exp], [m],,
	[AC_MSG_ERROR([exp() not found])])

AC_CHECK_HEADERS(locale.h)
AC_CHECK_HEADERS(valgrind/memcheck.h)

AC_CHECK_HEADERS([sys/prctl.h], AC_CHECK_FUNCS([prctl]))

AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
AM_CFLAGS="$AM_CFLAGS $PTHREAD_CFLAGS"
AM_CXXFLAGS="$AM_CXXFLAGS $PTHREAD_CFLAGS"

AC_CHECK_LIB([pthread], [pthread_setname_np],
	[have_pthread_setname_np=yes],
	[have_pthread_setname_np=no])
if test x$have_pthread_setname_np = xyes; then
	AC_DEFINE(HAVE_PTHREAD_SETNAME_NP, 1, [Is pthread_setname_np() available?])
fi

dnl ---------------------------------------------------------------------------
dnl Event loop selection
dnl ---------------------------------------------------------------------------

MPD_OPTIONAL_FUNC_NODEF(poll, poll)

if test x$host_is_linux = xyes; then
	MPD_OPTIONAL_FUNC_NODEF(epoll, epoll_create1)
fi

AC_ARG_WITH(pollmethod,
	AS_HELP_STRING(
		[--with-pollmethod=@<:@epoll|poll|winselect|auto@:>@],
		[specify poll method for internal event loop (default=auto)]),,
	[with_pollmethod=auto])

if test "x$with_pollmethod" = xauto; then
	if test "x$enable_epoll" = xyes; then
		with_pollmethod=epoll
	elif test "x$enable_poll" = xyes; then
		with_pollmethod=poll
	elif test "x$host_is_windows" = xyes; then
		with_pollmethod=winselect
	else
		AC_MSG_ERROR([no poll method is available for your platform])
	fi
fi
case "$with_pollmethod" in
epoll)
	AC_DEFINE(USE_EPOLL, 1, [Define to poll sockets with epoll])
	;;
poll)
	AC_DEFINE(USE_POLL, 1, [Define to poll sockets with poll])
	;;
winselect)
	AC_DEFINE(USE_WINSELECT, 1,
		[Define to poll sockets with Windows select])
	;;
*)
	AC_MSG_ERROR([unknown pollmethod option: $with_pollmethod])
esac

dnl ---------------------------------------------------------------------------
dnl Allow tools to be specifically built
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(database,
	AS_HELP_STRING([--enable-database],
		[enable support for the music database]),,
	enable_database=yes)
AM_CONDITIONAL(ENABLE_DATABASE, test x$enable_database = xyes)
if test x$enable_database = xyes; then
	database_auto=auto
	AC_DEFINE(ENABLE_DATABASE, 1, [Define to enable the music database])
else
	database_auto=no
fi

AC_ARG_ENABLE(libmpdclient,
	AS_HELP_STRING([--enable-libmpdclient],
		[enable support for the MPD client]),,
	enable_libmpdclient=auto)
MPD_DEPENDS([enable_libmpdclient], [enable_database],
	[Cannot use --enable-libmpdclient with --disable-database])

AC_ARG_ENABLE(expat,
	AS_HELP_STRING([--enable-expat],
		[enable the expat XML parser]),,
	enable_expat=auto)

AC_ARG_ENABLE(upnp,
	AS_HELP_STRING([--enable-upnp],
		[enable UPnP client support (default: auto)]),,
	enable_upnp=auto)
MPD_DEPENDS([enable_upnp], [enable_database],
	[Cannot use --enable-upnp with --disable-database])

AC_ARG_ENABLE(adplug,
	AS_HELP_STRING([--enable-adplug],
		[enable the AdPlug decoder plugin (default: auto)]),,
	enable_adplug=auto)

AC_ARG_ENABLE(alsa,
	AS_HELP_STRING([--enable-alsa], [enable ALSA support]),,
	[enable_alsa=$linux_auto])

AC_ARG_ENABLE(roar,
	AS_HELP_STRING([--enable-roar],
		[enable support for RoarAudio]),,
	[enable_roar=auto])

AC_ARG_ENABLE(ao,
	AS_HELP_STRING([--enable-ao],
		[enable support for libao]),,
	enable_ao=auto)
MPD_DEPENDS([enable_ao], [enable_glib],
	[Cannot use --enable-ao with --disable-glib])

AC_ARG_ENABLE(audiofile,
	AS_HELP_STRING([--enable-audiofile],
		[enable audiofile support (WAV and others)]),,
	enable_audiofile=auto)

AC_ARG_ENABLE(zlib,
	AS_HELP_STRING([--enable-zlib],
		[enable zlib support (default: auto)]),,
	enable_zlib=auto)

AC_ARG_ENABLE(bzip2,
	AS_HELP_STRING([--enable-bzip2],
		[enable bzip2 archive support (default: auto)]),,
	enable_bzip2=auto)

AC_ARG_ENABLE(cdio-paranoia,
	AS_HELP_STRING([--enable-cdio-paranoia],
		[enable support for audio CD support]),,
	enable_cdio_paranoia=auto)
MPD_DEPENDS([enable_cdio_paranoia], [enable_glib],
	[Cannot use --enable-cdio-paranoia with --disable-glib])

AC_ARG_ENABLE(curl,
	AS_HELP_STRING([--enable-curl],
		[enable support for libcurl HTTP streaming (default: auto)]),,
	[enable_curl=auto])

AC_ARG_ENABLE(smbclient,
	AS_HELP_STRING([--enable-smbclient],
		[enable support for libsmbclient (default: auto)]),,
	[enable_smbclient=auto])

AC_ARG_ENABLE(nfs,
	AS_HELP_STRING([--enable-nfs],
		[enable support for libnfs (default: auto)]),,
	[enable_nfs=auto])

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug],
		[enable debugging (default: disabled)]),,
	enable_debug=no)

AC_ARG_ENABLE(documentation,
	AS_HELP_STRING([--enable-documentation],
		[build documentation (default: disable)]),,
	[enable_documentation=no])

AC_ARG_ENABLE(dsd,
	AS_HELP_STRING([--enable-dsd],
		[enable DSD decoder (default: enable)]),,
	[enable_dsd=yes])

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
MPD_DEPENDS([enable_gme], [enable_glib],
	[Cannot use --enable-gme with --disable-glib])

AC_ARG_ENABLE(httpd-output,
	AS_HELP_STRING([--enable-httpd-output],
		[enables the HTTP server output]),,
	[enable_httpd_output=auto])
MPD_DEPENDS([enable_httpd_output], [enable_glib],
	[Cannot use --enable-httpd-output with --disable-glib])

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
MPD_DEPENDS([enable_jack], [enable_glib],
	[Cannot use --enable-jack with --disable-glib])

AC_SYS_LARGEFILE

AC_ARG_ENABLE(soundcloud,
	AS_HELP_STRING([--enable-soundcloud],
		[enable support for soundcloud.com]),,
	[enable_soundcloud=auto])
MPD_DEPENDS([enable_soundcloud], [enable_glib],
	[Cannot use --enable-soundcloud with --disable-glib])

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

AC_ARG_ENABLE(soxr,
	AS_HELP_STRING([--enable-soxr],
		[enable the libsoxr resampler]),,
	enable_soxr=auto)

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
	AS_HELP_STRING([--enable-mpc],
		[disable musepack (MPC) support (default: auto)]),,
	enable_mpc=auto)

AC_ARG_ENABLE(mpg123,
	AS_HELP_STRING([--enable-mpg123],
		[enable libmpg123 decoder plugin]),,
	enable_mpg123=auto)

AC_ARG_ENABLE(openal,
	AS_HELP_STRING([--enable-openal],
		[enable OpenAL support (default: auto)]),,
	enable_openal=auto)

AC_ARG_ENABLE(opus,
	AS_HELP_STRING([--enable-opus],
		[enable Opus codec support (default: auto)]),,
	enable_opus=auto)

AC_ARG_ENABLE(oss,
	AS_HELP_STRING([--disable-oss],
		[disable OSS support (default: enable)]),,
	enable_oss=yes)

AC_ARG_ENABLE(osx,
	AS_HELP_STRING([--enable-osx],
		[enable the OS X output plugin - unsupported! (default: disable)]),,
	enable_osx=no)

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
MPD_DEPENDS([enable_sidplay], [enable_glib],
	[Cannot use --enable-sidplay with --disable-glib])

AC_ARG_ENABLE(shine-encoder,
	AS_HELP_STRING([--enable-shine-encoder],
		[enables shine encoder]),,
	[enable_shine_encoder=auto])

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
	[enable_solaris_output=$host_is_solaris])

AC_ARG_ENABLE(sqlite,
	AS_HELP_STRING([--enable-sqlite],
		[enable support for the SQLite database]),,
	[enable_sqlite=$database_auto])
MPD_DEPENDS([enable_sqlite], [enable_glib],
	[Cannot use --enable-sqlite with --disable-glib])

AC_ARG_ENABLE(systemd-daemon,
	AS_HELP_STRING([--enable-systemd-daemon],
		[use the systemd daemon library (default=auto)]),,
	[enable_systemd_daemon=$linux_auto])

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
	[enable_un=$host_is_unix])

AC_ARG_ENABLE(vorbis,
	AS_HELP_STRING([--enable-vorbis],
		[enable Ogg Vorbis decoder]),,
	enable_vorbis=auto)

AC_ARG_ENABLE(vorbis-encoder,
	AS_HELP_STRING([--enable-vorbis-encoder],
		[enable the Ogg Vorbis encoder]),,
	[enable_vorbis_encoder=auto])
MPD_DEPENDS([enable_vorbis_encoder], [enable_glib],
	[Cannot use --enable-vorbis-encoder with --disable-glib])

AC_ARG_ENABLE(wave-encoder,
	AS_HELP_STRING([--enable-wave-encoder],
		[enable the PCM wave encoder]),,
	enable_wave_encoder=yes)

AC_ARG_ENABLE(wavpack,
	AS_HELP_STRING([--enable-wavpack],
		[enable WavPack support]),,
	enable_wavpack=auto)
MPD_DEPENDS([enable_wavpack], [enable_glib],
	[Cannot use --enable-wavpack with --disable-glib])

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],
		[treat warnings as errors (default: disabled)]),,
	enable_werror=no)

AC_ARG_ENABLE(wildmidi,
	AS_HELP_STRING([--enable-wildmidi],
		[enable MIDI support via wildmidi (default: auto)]),,
	enable_wildmidi=auto)

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

no_exceptions=yes

AX_BOOST_BASE([1.46],, [AC_MSG_ERROR([Boost not found])])

dnl Don't disable exceptions on Boost older than 1.54, because
dnl Boost.Intrusive supports this compiler mode only since 1.54;
dnl see https://svn.boost.org/trac/boost/ticket/7849
CPPFLAGS_SAVED="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
export CPPFLAGS
AC_LANG_PUSH(C++)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
@%:@include <boost/version.hpp>
]], [[
#if BOOST_VERSION < 105400
#error detected Boost older than 1.54
#endif
]])],, [no_exceptions=no])
AC_LANG_POP([C++])
CPPFLAGS="$CPPFLAGS_SAVED"

AC_ARG_ENABLE(icu,
	AS_HELP_STRING([--enable-icu],
		[enable libicu for Unicode (default: enabled)]),,
	enable_icu=yes)

if test x$enable_icu = xyes; then
	PKG_CHECK_MODULES([ICU], [icu-i18n],,
		[AC_MSG_ERROR([libicu not found])])

	AC_DEFINE(HAVE_ICU, 1, [Define if libicu is used])
fi
AM_CONDITIONAL(HAVE_ICU, test x$enable_icu = xyes)

AC_ARG_ENABLE(glib,
	AS_HELP_STRING([--enable-glib],
		[enable GLib usage (default: enabled)]),,
	enable_glib=yes)

if test x$enable_glib = xyes; then
	PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.28 gthread-2.0],,
		[AC_MSG_ERROR([GLib 2.28 is required])])

	if test x$GCC = xyes; then
		# suppress warnings in the GLib headers
		GLIB_CFLAGS=`echo $GLIB_CFLAGS |sed -e 's,-I/,-isystem /,g'`
	fi

	AC_DEFINE(HAVE_GLIB, 1, [Define if GLib is used])
fi
AM_CONDITIONAL(HAVE_GLIB, test x$enable_glib = xyes)

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

dnl -------------------------------- libmpdclient --------------------------------
MPD_AUTO_PKG(libmpdclient, LIBMPDCLIENT, [libmpdclient >= 2.2],
	[MPD client library], [libmpdclient not found])
if test x$enable_libmpdclient = xyes; then
	AC_DEFINE(HAVE_LIBMPDCLIENT, 1, [Define to use libmpdclient])
fi

AM_CONDITIONAL(HAVE_LIBMPDCLIENT, test x$enable_libmpdclient = xyes)

dnl -------------------------------- expat --------------------------------
MPD_AUTO_PKG(expat, EXPAT, [expat],
	[expat XML parser], [expat not found])
if test x$enable_expat = xyes; then
	AC_DEFINE(HAVE_EXPAT, 1, [Define to use the expat XML parser])
fi

AM_CONDITIONAL(HAVE_EXPAT, test x$enable_expat = xyes)

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

	if test x$found_libwrap = xyes; then
		dnl  See if libwrap is compatible with C++; it is
		dnl  broken on many systems
		AC_MSG_CHECKING(whether libwrap is compatible with C++)
		AC_LANG_PUSH([C++])
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([
			#include <tcpd.h>
			bool CheckLibWrap(int fd, const char &progname) {
				struct request_info req;
				request_init(&req, RQ_FILE, fd, RQ_DAEMON, progname, 0);
				fromhost(&req);
				return hosts_access(&req);
			}
			])],
			AC_MSG_RESULT([yes]),
			[found_libwrap=no; AC_MSG_RESULT([no]);
			AC_MSG_WARN([Your version of libwrap is broken with C++])])
		AC_LANG_POP
	fi

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

MPD_AUTO_PKG(avahi, AVAHI, [avahi-client dbus-1],
	[avahi client library], [avahi-client not found])
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
MPD_AUTO_PKG(lsr, SAMPLERATE, [samplerate >= 0.1.3],
	[libsamplerate resampling], [libsamplerate not found])
if test x$enable_lsr = xyes; then
	AC_DEFINE([HAVE_LIBSAMPLERATE], 1,
		[Define to enable libsamplerate])
fi
AM_CONDITIONAL(HAVE_LIBSAMPLERATE, test x$enable_lsr = xyes)

dnl ------------------------------ libsoxr ------------------------------------
MPD_AUTO_PKG(soxr, SOXR, [soxr],
	[libsoxr resampler], [libsoxr not found])
if test x$enable_soxr = xyes; then
	AC_DEFINE([HAVE_SOXR], 1, [Define to enable libsoxr])
fi

AM_CONDITIONAL(HAVE_SOXR, test x$enable_soxr = xyes)

dnl ---------------------------------------------------------------------------
dnl Input Plugins
dnl ---------------------------------------------------------------------------

dnl ----------------------------------- CURL ----------------------------------
MPD_AUTO_PKG(curl, CURL, [libcurl >= 7.18],
	[libcurl HTTP streaming], [libcurl not found])
if test x$enable_curl = xyes; then
	AC_DEFINE(ENABLE_CURL, 1, [Define when libcurl is used for HTTP streaming])
fi
AM_CONDITIONAL(ENABLE_CURL, test x$enable_curl = xyes)

dnl ----------------------------------- smbclient -----------------------------
MPD_AUTO_PKG_LIB(smbclient, SMBCLIENT, [smbclient >= 0.2],
	[smbclient], [smbc_init], [-lsmbclient], [],
	[smbclient input plugin], [libsmbclient not found])
if test x$enable_smbclient = xyes; then
	AC_DEFINE(ENABLE_SMBCLIENT, 1, [Define when libsmbclient is used])
fi
AM_CONDITIONAL(ENABLE_SMBCLIENT, test x$enable_smbclient = xyes)

dnl ----------------------------------- NFS -----------------------------
MPD_AUTO_PKG(nfs, NFS, [libnfs],
	[NFS input plugin], [libnfs not found])
if test x$enable_nfs = xyes; then
	AC_DEFINE(ENABLE_NFS, 1, [Define when libnfs is used])
fi
AM_CONDITIONAL(ENABLE_NFS, test x$enable_nfs = xyes)

dnl --------------------------------- Soundcloud ------------------------------
if test x$enable_soundcloud != xno; then
	PKG_CHECK_MODULES([YAJL], [yajl >= 2.0],
		[found_soundcloud=yes],
		AC_CHECK_LIB([yajl], [yajl_parse_complete],
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
	AC_CHECK_HEADERS(cdio/paranoia/paranoia.h)
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
dnl Neighbor Plugins
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(neighbor-plugins,
	AS_HELP_STRING([--enable-neighbor-plugins],
		[enable support for neighbor discovery (default: auto)]),,
	[enable_neighbor_plugins=auto])

if test x$enable_neighbor_plugins = xauto; then
	if test x$enable_smbclient = xyes; then
		enable_neighbor_plugins=yes
	fi
	if test x$enable_upnp = xyes; then
		enable_neighbor_plugins=yes
	fi
fi

if test x$enable_neighbor_plugins = xyes; then
	AC_DEFINE(ENABLE_NEIGHBOR_PLUGINS, 1,
		[Define to enable support for neighbor discovery])
fi
AM_CONDITIONAL(ENABLE_NEIGHBOR_PLUGINS, test x$enable_neighbor_plugins = xyes)

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

dnl ---------------------------------- zlib ---------------------------------

MPD_AUTO_PKG(zlib, ZLIB, [zlib],
	[zlib support], [zlib not found])

AM_CONDITIONAL(HAVE_ZLIB, test x$enable_zlib = xyes)
if test x$enable_zlib = xyes; then
	AC_DEFINE(HAVE_ZLIB, 1, [Define to enable zlib support])
fi

dnl ---------------------------------- libbz2 ---------------------------------

MPD_AUTO_LIB(bzip2, BZ2, bz2, BZ2_bzDecompressInit, [-lbz2], [],
	[bzip2], [libbz2 not found])

AM_CONDITIONAL(HAVE_BZ2, test x$enable_bzip2 = xyes)
if test x$enable_bzip2 = xyes; then
	AC_DEFINE(HAVE_BZ2, 1, [Define to have bz2 archive support])

	AC_PATH_PROG(BZIP2, bzip2, no)
else
	BZIP2="no"
fi

AM_CONDITIONAL(ENABLE_BZIP2_TEST, test x$BZIP2 != xno)

dnl ---------------------------------- libupnp ---------------------------------

if test x$enable_expat = xno; then
	if test x$enable_upnp = xauto; then
		AC_MSG_WARN([expat disabled -- disabling UPnP])
		enable_upnp=no
	elif test x$enable_upnp = xyes; then
		AC_MSG_ERROR([expat disabled -- required for UPnP])
	fi
fi

MPD_AUTO_PKG(upnp, UPNP, [libupnp],
	[UPnP client support], [libupnp not found])
if test x$enable_upnp = xyes; then
	AC_DEFINE(HAVE_LIBUPNP, 1, [Define when libupnp is used])
fi
AM_CONDITIONAL(HAVE_LIBUPNP, test x$enable_upnp = xyes)

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

dnl -------------------------------- libadplug --------------------------------
MPD_AUTO_PKG(adplug, ADPLUG, [adplug],
	[AdPlug decoder plugin], [libadplug not found])
if test x$enable_adplug = xyes; then
	AC_DEFINE(HAVE_ADPLUG, 1, [Define to use libadplug])
fi
AM_CONDITIONAL(HAVE_ADPLUG, test x$enable_adplug = xyes)

dnl -------------------------------- audiofile --------------------------------
MPD_AUTO_PKG(audiofile, AUDIOFILE, [audiofile >= 0.3],
	[audiofile decoder plugin], [libaudiofile not found])
AM_CONDITIONAL(HAVE_AUDIOFILE, test x$enable_audiofile = xyes)
if test x$enable_audiofile = xyes; then
	AC_DEFINE(HAVE_AUDIOFILE, 1, [Define for audiofile support])
fi

dnl ----------------------------------- DSD -----------------------------------

if test x$enable_dsd = xyes; then
	AC_DEFINE(ENABLE_DSD, 1, [Define for the DSD decoder])
fi

AM_CONDITIONAL(ENABLE_DSD, test x$enable_dsd = xyes)

dnl ----------------------------------- FAAD ----------------------------------
AM_PATH_FAAD()

AM_CONDITIONAL(HAVE_FAAD, test x$enable_aac = xyes)

dnl ---------------------------------- ffmpeg ---------------------------------
MPD_AUTO_PKG(ffmpeg, FFMPEG, [libavformat >= 53.17 libavcodec >= 53.25 libavutil >= 51.17],
	[ffmpeg decoder library], [libavformat+libavcodec+libavutil not found])

if test x$enable_ffmpeg = xyes; then
	AC_DEFINE(HAVE_FFMPEG, 1, [Define for FFMPEG support])
fi

AM_CONDITIONAL(HAVE_FFMPEG, test x$enable_ffmpeg = xyes)

dnl ----------------------------------- FLAC ----------------------------------

MPD_AUTO_PKG(flac, FLAC, [flac >= 1.2],
	[FLAC decoder], [libFLAC not found])

if test x$enable_flac = xyes; then
	AC_DEFINE(HAVE_FLAC, 1, [Define for FLAC support])
fi

AM_CONDITIONAL(HAVE_FLAC, test x$enable_flac = xyes)

enable_flac_encoder=$enable_flac

dnl -------------------------------- FluidSynth -------------------------------

MPD_AUTO_PKG(fluidsynth, FLUIDSYNTH, [fluidsynth >= 1.1],
	[fluidsynth decoder], [fluidsynth not found])

if test x$enable_fluidsynth = xyes; then
	AC_DEFINE(ENABLE_FLUIDSYNTH, 1, [Define for fluidsynth support])
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
MPD_AUTO_PKG(modplug, MODPLUG, [libmodplug],
	[modplug decoder plugin], [libmodplug not found])

if test x$enable_modplug = xyes; then
	AC_DEFINE(HAVE_MODPLUG, 1, [Define for modplug support])
fi
AM_CONDITIONAL(HAVE_MODPLUG, test x$enable_modplug = xyes)

dnl -------------------------------- libopus ----------------------------------
MPD_AUTO_PKG(opus, OPUS, [opus ogg],
	[opus decoder plugin], [libopus not found])
if test x$enable_opus = xyes; then
	AC_DEFINE(HAVE_OPUS, 1, [Define to use libopus])
fi
AM_CONDITIONAL(HAVE_OPUS, test x$enable_opus = xyes)

dnl -------------------------------- libsndfile -------------------------------
dnl See above test, which may disable this.
MPD_AUTO_PKG(sndfile, SNDFILE, [sndfile],
	[libsndfile decoder plugin], [libsndfile not found])

if test x$enable_sndfile = xyes; then
	AC_DEFINE(ENABLE_SNDFILE, 1, [Define to enable the sndfile decoder plugin])
fi
AM_CONDITIONAL(ENABLE_SNDFILE, test x$enable_sndfile = xyes)

dnl --------------------------------- musepack --------------------------------

MPD_AUTO_LIB(mpc, MPCDEC, mpcdec, mpc_demux_init, [-lmpcdec], [],
	[mpcdec], [libmpcdec not found])
if test x$enable_mpc = xyes; then
	AC_DEFINE(HAVE_MPCDEC, 1, [Define to use libmpcdec for MPC decoding])
fi
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

MPD_AUTO_PKG(vorbis, VORBIS, [vorbisfile vorbis ogg],
	[Ogg Vorbis decoder], [libvorbis not found])
if test x$enable_vorbis = xyes; then
	AC_DEFINE(ENABLE_VORBIS_DECODER, 1, [Define for Ogg Vorbis support])
fi

AM_CONDITIONAL(ENABLE_VORBIS_DECODER, test x$enable_vorbis = xyes || test x$enable_tremor = xyes)

dnl --------------------------------- sidplay ---------------------------------
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
		AC_CHECK_LIB([sidutils],[main],[:],[found_sidplay=no],[])
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
MPD_AUTO_LIB(wildmidi, WILDMIDI, WildMidi, WildMidi_Init, [-lWildMidi], [],
	[wildmidi], [libwildmidi not found])
if test x$enable_wildmidi = xyes; then
	AC_DEFINE(ENABLE_WILDMIDI, 1, [Define for wildmidi support])
fi

AM_CONDITIONAL(ENABLE_WILDMIDI, test x$enable_wildmidi = xyes)

dnl ------------------------ Post Decoder Plugins Tests -----------------------

AM_CONDITIONAL(HAVE_XIPH,
	test x$enable_vorbis = xyes || test x$enable_tremor = xyes || test x$enable_flac = xyes || test x$enable_opus = xyes)

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
	enable_shine_encoder=no
	enable_wave_encoder=no
	enable_flac_encoder=no
fi

dnl ------------------------------- FLAC Encoder ------------------------------
if test x$enable_flac_encoder = xyes; then
	AC_DEFINE(ENABLE_FLAC_ENCODER, 1,
		[Define to enable the FLAC encoder plugin])
fi
AM_CONDITIONAL(ENABLE_FLAC_ENCODER, test x$enable_flac_encoder = xyes)

dnl ------------------------------- Shine Encoder ------------------------------

MPD_AUTO_PKG(shine_encoder, SHINE, [shine >= 3.1],
	[shine encoder], [libshine not found])

if test x$enable_shine_encoder = xyes; then
	AC_DEFINE(ENABLE_SHINE_ENCODER, 1,
		[Define to enable the shine encoder plugin])
fi
AM_CONDITIONAL(ENABLE_SHINE_ENCODER, test x$enable_shine_encoder = xyes)

dnl ---------------------------- Ogg Vorbis Encoder ---------------------------
MPD_AUTO_PKG(vorbis_encoder, VORBISENC, [vorbisenc vorbis ogg],
	[Ogg Vorbis encoder], [libvorbisenc not found])

if test x$enable_vorbis_encoder = xyes; then
	AC_DEFINE(ENABLE_VORBIS_ENCODER, 1,
		[Define to enable the vorbis encoder plugin])
fi
AM_CONDITIONAL(ENABLE_VORBIS_ENCODER, test x$enable_vorbis_encoder = xyes)

dnl ------------------------------- LAME Encoder ------------------------------

MPD_AUTO_LIB(lame_encoder, LAME, mp3lame, lame_init, [-lmp3lame], [],
	[libmp3lame], [libmp3lame not found])
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
	test x$enable_opus != xno ||
	test x$enable_lame_encoder != xno ||
	test x$enable_twolame_encoder != xno ||
	test x$enable_flac_encoder != xno ||
	test x$enable_shine_encoder != xno ||
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
AM_CONDITIONAL(HAVE_OGG_ENCODER, test x$enable_vorbis_encoder = xyes || test x$enable_opus = xyes)

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

dnl ---------------------------------- OpenAL ---------------------------------
AC_SUBST(OPENAL_CFLAGS,"")
AC_SUBST(OPENAL_LIBS,"")

if test x$host_is_darwin = xyes; then
	if test x$enable_openal != xno; then
		AC_CHECK_HEADERS([OpenAL/al.h OpenAL/alc.h],
			[found_openal=yes], [found_openal=no])
	fi

	MPD_AUTO_RESULT(openal, [OpenAL output plugin], [OpenAL not found])

	if test x$enable_openal = xyes; then
		OPENAL_LIBS="-framework OpenAL"
	fi
else
	MPD_AUTO_PKG(openal, [OPENAL], [openal],
		[OpenAL output plugin], [OpenAL not found])
fi

if test x$enable_openal = xyes; then
	AC_DEFINE(HAVE_OPENAL, 1, [Define for OpenAL support])
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
if test x$enable_osx = xyes; then
	AC_DEFINE(HAVE_OSX, 1, [Define for compiling OS X support])
	LIBS="$LIBS -framework AudioUnit -framework CoreAudio -framework CoreServices"
fi

AM_CONDITIONAL(HAVE_OSX, test x$enable_osx = xyes)

dnl ------------------------------- Pipe Output -------------------------------
if test x$enable_pipe_output = xyes; then
	AC_DEFINE([ENABLE_PIPE_OUTPUT], 1,
		[Define to enable support for writing audio to a pipe])
fi
AM_CONDITIONAL(ENABLE_PIPE_OUTPUT, test x$enable_pipe_output = xyes)

dnl -------------------------------- PulseAudio -------------------------------
MPD_AUTO_PKG(pulse, PULSE, [libpulse >= 0.9.16],
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

if test x$enable_solaris_output = xyes; then
	AC_DEFINE(ENABLE_SOLARIS_OUTPUT, 1, [Define to enable Solaris /dev/audio support])
fi

AM_CONDITIONAL(ENABLE_SOLARIS_OUTPUT, test x$enable_solaris_output = xyes)

dnl --------------------------------- WinMM ---------------------------------

if test "x$host_is_windows" = xyes; then
	AC_DEFINE(ENABLE_WINMM_OUTPUT, 1, [Define to enable WinMM support])
	enable_winmm_output=yes
	LIBS="$LIBS -lwinmm"
else
	enable_winmm_output=no
fi

AM_CONDITIONAL(ENABLE_WINMM_OUTPUT, test x$enable_winmm_output = xyes)

dnl ---------------------------------------------------------------------------
dnl Documentation
dnl ---------------------------------------------------------------------------
if test x$enable_documentation = xyes; then
	AC_PATH_PROG(XMLTO, xmlto)
	if test x$XMLTO = x; then
		AC_MSG_ERROR([xmlto not found])
	fi

	AC_SUBST(XMLTO)

	AC_PATH_PROG(DOXYGEN, doxygen)
	if test x$DOXYGEN = x; then
		AC_MSG_ERROR([doxygen not found])
	fi

	AC_SUBST(DOXYGEN)
fi

AM_CONDITIONAL(ENABLE_DOCUMENTATION, test x$enable_documentation = xyes)

dnl ---------------------------------------------------------------------------
dnl test suite
dnl ---------------------------------------------------------------------------

if test "x$enable_test" = xyes; then
	PKG_CHECK_MODULES([CPPUNIT], [cppunit],,
		[AC_MSG_ERROR([cppunit not found])])
fi

AM_CONDITIONAL(ENABLE_TEST, test "x$enable_test" = xyes)

dnl ---------------------------------------------------------------------------
dnl CFLAGS
dnl ---------------------------------------------------------------------------

dnl ---------------------------- warnings as errors ---------------------------
if test "x$enable_werror" = xyes; then
	CFLAGS="$CFLAGS -Werror -pedantic-errors"
	CXXFLAGS="$CXXFLAGS -Werror"
fi

dnl ---------------------------- language features ----------------------------
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden])
AX_APPEND_COMPILE_FLAGS([-ffast-math])
AX_APPEND_COMPILE_FLAGS([-ftree-vectorize])

AC_LANG_PUSH([C++])
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden])
AX_APPEND_COMPILE_FLAGS([-fno-threadsafe-statics])
AX_APPEND_COMPILE_FLAGS([-fmerge-all-constants])

if test x$no_exceptions = xyes; then
	AX_APPEND_COMPILE_FLAGS([-fno-exceptions])
	AX_APPEND_COMPILE_FLAGS([-fno-rtti])
fi

AX_APPEND_COMPILE_FLAGS([-ffast-math])
AX_APPEND_COMPILE_FLAGS([-ftree-vectorize])
AC_LANG_POP

dnl ---------------------------------- debug ----------------------------------
if test "x$enable_debug" = xno; then
	AM_CPPFLAGS="$AM_CPPFLAGS -DNDEBUG"

	AX_APPEND_COMPILE_FLAGS([-ffunction-sections])
	AX_APPEND_COMPILE_FLAGS([-fdata-sections])

	AC_LANG_PUSH([C++])
	AX_APPEND_COMPILE_FLAGS([-ffunction-sections])
	AX_APPEND_COMPILE_FLAGS([-fdata-sections])
	AC_LANG_POP

	AX_APPEND_LINK_FLAGS([-Wl,--gc-sections])
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

	AC_LANG_PUSH([C++])
	AX_APPEND_COMPILE_FLAGS([-Wall])
	AX_APPEND_COMPILE_FLAGS([-Wextra])
	AX_APPEND_COMPILE_FLAGS([-Wmissing-declarations])
	AX_APPEND_COMPILE_FLAGS([-Wshadow])
	AX_APPEND_COMPILE_FLAGS([-Wpointer-arith])
	AX_APPEND_COMPILE_FLAGS([-Wcast-qual])
	AX_APPEND_COMPILE_FLAGS([-Wwrite-strings])
	AX_APPEND_COMPILE_FLAGS([-Wsign-compare])
	AC_LANG_POP
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

printf '\nStorage support:\n\t'
results(nfs, [NFS])
results(smbclient, [SMB])

printf '\nFile format support:\n\t'
results(aac, [AAC])
results(adplug, [AdPlug])
results(dsd, [DSD])
results(sidplay, [C64 SID])
results(ffmpeg, [FFMPEG])
results(flac, [FLAC])
results(fluidsynth, [FluidSynth])
results(gme, [GME])
printf '\n\t'
results(sndfile, [libsndfile])
results(mikmod, [MikMod])
results(modplug, [MODPLUG])
results(mad, [MAD])
results(mpg123, [MPG123])
results(mpc, [Musepack])
printf '\n\t'
results(opus, [Opus])
results(tremor, [OggTremor])
results(vorbis, [OggVorbis])
results(audiofile, [WAVE])
results(wavpack, [WavPack])
results(wildmidi, [WildMidi])

printf '\nOther features:\n\t'
results(lsr, [libsamplerate])
results(soxr, [libsoxr])
results(libmpdclient, [libmpdclient])
results(inotify, [inotify])
results(sqlite, [SQLite])

printf '\nMetadata support:\n\t'
results(id3,[ID3])

printf '\nPlayback support:\n\t'
results(alsa,ALSA)
results(fifo,FIFO)
results(recorder_output,[File Recorder])
results(httpd_output,[HTTP Daemon])
results(jack,[JACK])
printf '\n\t'
results(ao,[libao])
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
		results(shine_encoder, [Shine])
		results(vorbis_encoder, [Ogg Vorbis])
		results(opus, [Opus])
		results(twolame_encoder, [TwoLAME])
		results(wave_encoder, [WAVE])
fi

printf '\nStreaming support:\n\t'
results(cdio_paranoia, [CDIO_PARANOIA])
results(curl,[CURL])
results(smbclient,[SMBCLIENT])
results(soundcloud,[Soundcloud])
printf '\n\t'
results(mms,[MMS])

printf '\nEvent loop:\n\t'
printf $with_pollmethod

printf '\n\n##########################################\n\n'

echo 'Generating files needed for compilation'

dnl ---------------------------------------------------------------------------
dnl Generate files
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(doc/doxygen.conf)
AC_CONFIG_FILES(systemd/mpd.service)
AC_OUTPUT

echo 'MPD is ready for compilation, type "make" to begin.'
