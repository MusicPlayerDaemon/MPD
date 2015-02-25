AC_PREREQ(2.60)

AC_INIT(mpd, 0.20, musicpd-dev-team@lists.sourceforge.net)

VERSION_MAJOR=0
VERSION_MINOR=20
VERSION_REVISION=0
VERSION_EXTRA=0

AC_CONFIG_SRCDIR([src/Main.cxx])
AC_CONFIG_AUX_DIR(build)
AM_INIT_AUTOMAKE([foreign 1.11 dist-xz subdir-objects])
AM_SILENT_RULES
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AC_DEFINE(PROTOCOL_VERSION, "0.20.0", [The MPD protocol version])

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
		win32/res/mpd.rc
	])
	AC_CHECK_TOOL(WINDRES, windres)
	AM_CPPFLAGS="$AM_CPPFLAGS -DWIN32_LEAN_AND_MEAN"
	AM_CPPFLAGS="$AM_CPPFLAGS -DWINVER=0x0600 -D_WIN32_WINNT=0x0600"
	AM_CPPFLAGS="$AM_CPPFLAGS -DSTRICT"
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

AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
AM_CFLAGS="$AM_CFLAGS $PTHREAD_CFLAGS"
AM_CXXFLAGS="$AM_CXXFLAGS $PTHREAD_CFLAGS"

MPD_WITH_LIBRARY([PTHREAD],
	[AC_CHECK_FUNCS([pthread_setname_np])])

AC_SEARCH_LIBS([clock_gettime], [rt])

AC_ARG_ENABLE(syslog,
	AS_HELP_STRING([--enable-syslog],
		[enable syslog support (default: auto)]),,
	enable_syslog=auto)
MPD_AUTO(syslog, [syslog support], [syslog() not available],
	[AC_SEARCH_LIBS([syslog], [bsd socket inet],
		[found_syslog=yes],
		[found_syslog=no])])
if test x$enable_syslog = xyes; then
	AC_DEFINE(HAVE_SYSLOG, 1, [Define if syslog() is available])
fi

AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([gethostbyname], [nsl])

if test x$host_is_linux = xyes; then
	AC_CHECK_FUNCS(pipe2 accept4 linkat)
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
MPD_DEFINE_CONDITIONAL(enable_database, ENABLE_DATABASE,
	[the music database])
if test x$enable_database = xyes; then
	database_auto=auto
else
	database_auto=no
fi

default_enable_daemon=yes
if test x$host_is_android = xyes || test x$host_is_android = xyes; then
	default_enable_daemon=no
fi
AC_ARG_ENABLE(daemon,
	AS_HELP_STRING([--enable-daemon],
		[enable daemonization (default: enabled)]),,
	enable_daemon=$default_enable_daemon)
MPD_DEFINE_CONDITIONAL(enable_daemon, ENABLE_DAEMON, [Enable daemonization?])

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

AC_ARG_ENABLE(fifo,
	AS_HELP_STRING([--disable-fifo],
		[disable support for writing audio to a FIFO (default: enable)]),,
	enable_fifo=yes)

AC_ARG_ENABLE(httpd-output,
	AS_HELP_STRING([--enable-httpd-output],
		[enables the HTTP server output]),,
	[enable_httpd_output=auto])

AC_ARG_ENABLE(inotify,
	AS_HELP_STRING([--disable-inotify],
		[disable support Inotify automatic database update (default: enabled) ]),,
	[enable_inotify=yes])

AC_ARG_ENABLE(ipv6,
	AS_HELP_STRING([--disable-ipv6],
		[disable IPv6 support (default: enable)]),,
	[enable_ipv6=yes])

AC_SYS_LARGEFILE

AC_ARG_ENABLE(soundcloud,
	AS_HELP_STRING([--enable-soundcloud],
		[enable support for soundcloud.com]),,
	[enable_soundcloud=auto])

AC_ARG_ENABLE([libwrap],
	AS_HELP_STRING([--enable-libwrap], [use libwrap]),,
	[enable_libwrap=auto])

AC_ARG_ENABLE(mikmod,
	AS_HELP_STRING([--enable-mikmod],
		[enable the mikmod decoder (default: disable)]),,
	enable_mikmod=no)

AC_ARG_ENABLE(openal,
	AS_HELP_STRING([--enable-openal],
		[enable OpenAL support (default: auto)]),,
	enable_openal=auto)

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

AC_ARG_ENABLE(solaris_output,
	AS_HELP_STRING([--enable-solaris-output],
		[enables the Solaris /dev/audio output]),,
	[enable_solaris_output=$host_is_solaris])

AC_ARG_ENABLE(tcp,
	AS_HELP_STRING([--disable-tcp],
		[disable support for clients connecting via TCP (default: enable)]),,
	[enable_tcp=yes])

AC_ARG_ENABLE(test,
	AS_HELP_STRING([--enable-test],
		[build the test programs (default: disabled)]),,
	enable_test=no)

AC_ARG_ENABLE(un,
	AS_HELP_STRING([--disable-un],
		[disable support for clients connecting via unix domain sockets (default: enable)]),,
	[enable_un=$host_is_unix])

AC_ARG_ENABLE(vorbis,
	AS_HELP_STRING([--enable-vorbis],
		[enable Ogg Vorbis decoder]),,
	enable_vorbis=auto)

AC_ARG_ENABLE(wave-encoder,
	AS_HELP_STRING([--enable-wave-encoder],
		[enable the PCM wave encoder]),,
	enable_wave_encoder=yes)

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],
		[treat warnings as errors (default: disabled)]),,
	enable_werror=no)

AC_ARG_WITH(zeroconf,
	AS_HELP_STRING([--with-zeroconf=@<:@auto|avahi|bonjour|no@:>@],
		[enable zeroconf backend (default=auto)]),,
	with_zeroconf="auto")

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
fi

MPD_DEFINE_CONDITIONAL(enable_icu, HAVE_ICU, [libicu])

AC_ARG_ENABLE(glib,
	AS_HELP_STRING([--enable-glib],
		[enable GLib (default: auto)]),,
	enable_glib=auto)

if test x$enable_glib != xno; then
	PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.32],
		[found_glib=yes],
		[found_glib=no])

	MPD_AUTO_RESULT([glib], [Glib], [GLib not found])
fi

if test x$enable_glib = xyes; then
	if test x$GCC = xyes; then
		# suppress warnings in the GLib headers
		GLIB_CFLAGS=`echo $GLIB_CFLAGS |sed -e 's,-I/,-isystem /,g'`
	fi
fi

MPD_DEFINE_CONDITIONAL(enable_glib, HAVE_GLIB, [GLib])

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

MPD_ENABLE_AUTO_PKG(systemd_daemon, SYSTEMD_DAEMON, libsystemd-daemon,
	[systemd socket activation], [libsystemd-daemon not found],
	[$linux_auto])

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
MPD_ENABLE_AUTO_PKG_DEPENDS(libmpdclient, LIBMPDCLIENT,
	[libmpdclient >= 2.2],
	[MPD client library], [libmpdclient not found], [],
	[enable_database], [Cannot use --enable-libmpdclient with --disable-database])

dnl -------------------------------- expat --------------------------------
MPD_ENABLE_AUTO_PKG(expat, EXPAT, [expat],
	[expat XML parser], [expat not found])

dnl --------------------------------- inotify ---------------------------------
AC_CHECK_FUNCS(inotify_init inotify_init1)

if test x$ac_cv_func_inotify_init = xno; then
	enable_inotify=no
fi

MPD_DEFINE_CONDITIONAL(enable_inotify, ENABLE_INOTIFY, [inotify support])

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
MPD_ENABLE_AUTO_PKG_LIB(id3, ID3TAG,
	id3tag, id3tag, id3_file_open, [-lid3tag -lz], [],
	[ID3 support using libid3tag], [libid3tag not found])

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
	with_zeroconf=avahi
fi

MPD_DEFINE_CONDITIONAL(enable_avahi, HAVE_AVAHI, [Avahi Zeroconf])

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

MPD_ENABLE_AUTO_PKG(sqlite, SQLITE, [sqlite3 >= 3.7.3],
	[SQLite database support], [sqlite not found],
	[$database_auto])

dnl ---------------------------------------------------------------------------
dnl Converter Plugins
dnl ---------------------------------------------------------------------------

dnl ------------------------------ libsamplerate ------------------------------
MPD_ENABLE_AUTO_PKG(lsr, LIBSAMPLERATE, [samplerate >= 0.1.3],
	[libsamplerate resampling], [libsamplerate not found])

dnl ------------------------------ libsoxr ------------------------------------
MPD_ENABLE_AUTO_PKG(soxr, SOXR, [soxr],
	[libsoxr resampler], [libsoxr not found])

dnl ---------------------------------------------------------------------------
dnl Input Plugins
dnl ---------------------------------------------------------------------------

dnl ----------------------------------- CURL ----------------------------------
MPD_ENABLE_AUTO_PKG(curl, CURL, [libcurl >= 7.18],
	[libcurl HTTP streaming], [libcurl not found])

dnl ----------------------------------- smbclient -----------------------------
MPD_ENABLE_AUTO_PKG_LIB(smbclient, SMBCLIENT, [smbclient >= 0.2],
	[smbclient], [smbc_init], [-lsmbclient], [],
	[smbclient input plugin], [libsmbclient not found])

dnl ----------------------------------- NFS -----------------------------
MPD_ENABLE_AUTO_PKG(nfs, NFS, [libnfs],
	[NFS input plugin], [libnfs not found])

dnl --------------------------------- Soundcloud ------------------------------
MPD_AUTO([soundcloud], [soundcloud.com support], [libyajl not found],
	[PKG_CHECK_MODULES([YAJL], [yajl >= 2.0],
		[found_soundcloud=yes],
		[found_soundcloud=no])])
MPD_DEFINE_CONDITIONAL(enable_soundcloud, ENABLE_SOUNDCLOUD,
	[soundcloud.com support])

dnl ---------------------------------- cdio ---------------------------------
MPD_ENABLE_AUTO_PKG(cdio_paranoia, CDIO_PARANOIA, [libcdio_paranoia],
	[libcdio_paranoia input plugin], [libcdio_paranoia not found])
if test x$enable_cdio_paranoia = xyes; then
	AC_CHECK_HEADERS(cdio/paranoia/paranoia.h)
fi

MPD_ENABLE_AUTO_PKG(mms, MMS, [libmms >= 0.4],
	[libmms mms:// protocol support], [libmms not found])

dnl ---------------------------------------------------------------------------
dnl Playlist Plugins
dnl ---------------------------------------------------------------------------

MPD_ARG_ENABLE(cue, CUE, [CUE sheet parser], yes)

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

MPD_DEFINE_CONDITIONAL(enable_neighbor_plugins, ENABLE_NEIGHBOR_PLUGINS,
	[neighbor discovery])

dnl ---------------------------------------------------------------------------
dnl Archive Plugins
dnl ---------------------------------------------------------------------------

dnl --------------------------------- iso9660 ---------------------------------
MPD_ENABLE_AUTO_PKG(iso9660, ISO9660, [libiso9660],
	[libiso9660 archive plugin], [libiso9660 not found])

if test x$enable_iso9660 = xyes; then
	AC_PATH_PROG(MKISOFS, mkisofs, no)
else
	MKISOFS="no"
fi

AM_CONDITIONAL(ENABLE_ISO9660_TEST, test x$MKISOFS != xno)

dnl ---------------------------------- zlib ---------------------------------

MPD_ENABLE_AUTO_PKG(zlib, ZLIB, [zlib],
	[zlib support], [zlib not found])

dnl ---------------------------------- libbz2 ---------------------------------

MPD_ENABLE_AUTO_LIB(bzip2, BZ2, bz2, BZ2_bzDecompressInit, [-lbz2], [],
	[bzip2 archive plugin], [libbz2 not found])

if test x$enable_bzip2 = xyes; then
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

MPD_ENABLE_AUTO_PKG_DEPENDS(upnp, UPNP, [libupnp],
	[UPnP client support], [libupnp not found], [],
	[enable_database], [Cannot use --enable-upnp with --disable-database])

dnl --------------------------------- libzzip ---------------------------------
MPD_ENABLE_AUTO_PKG(zzip, ZZIP, [zziplib >= 0.13],
	[libzzip archive library], [libzzip not found],
	[no])

if test x$enable_zzip = xyes; then
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
else
	enable_archive=no
fi

MPD_DEFINE_CONDITIONAL(enable_archive, ENABLE_ARCHIVE, [the archive API])

dnl ---------------------------------------------------------------------------
dnl Decoder Plugins
dnl ---------------------------------------------------------------------------

dnl -------------------------------- libadplug --------------------------------
MPD_ENABLE_AUTO_PKG(adplug, ADPLUG, [adplug],
	[AdPlug decoder plugin], [libadplug not found])

dnl -------------------------------- audiofile --------------------------------
MPD_ENABLE_AUTO_PKG(audiofile, AUDIOFILE, [audiofile >= 0.3],
	[audiofile decoder plugin], [libaudiofile not found])

dnl ----------------------------------- DSD -----------------------------------

MPD_DEFINE_CONDITIONAL(enable_dsd, ENABLE_DSD, [DSD decoder])

dnl ----------------------------------- FAAD ----------------------------------
MPD_ENABLE_AUTO_LIB(aac, FAAD, faad, NeAACDecOpen, [-lfaad], [],
	[FAAD decoder plugin], [libfaad not found])

dnl ---------------------------------- ffmpeg ---------------------------------
MPD_ENABLE_AUTO_PKG(ffmpeg, FFMPEG,
	[libavformat >= 53.17 libavcodec >= 53.25 libavutil >= 51.17],
	[ffmpeg decoder library], [libavformat+libavcodec+libavutil not found])

dnl ----------------------------------- FLAC ----------------------------------

MPD_ENABLE_AUTO_PKG(flac, FLAC, [flac >= 1.2],
	[FLAC decoder], [libFLAC not found])

enable_flac_encoder=$enable_flac

dnl -------------------------------- FluidSynth -------------------------------

MPD_ENABLE_AUTO_PKG(fluidsynth, FLUIDSYNTH, [fluidsynth >= 1.1],
	[fluidsynth MIDI decoder plugin], [fluidsynth not found])

dnl ---------------------------------- libgme ---------------------------------

MPD_ENABLE_AUTO_PKG_LIB(gme, GME, [libgme],
	gme, gme_open_file, [-lgme -lstdc++], [],
	[Game Music Emulator decoder plugin], [libgme not found])

dnl ---------------------------------- libmad ---------------------------------
MPD_ENABLE_AUTO_PKG_LIB(mad, MAD, [mad],
	mad, mad_stream_init, [-lmad], [],
	[libmad MP3 decoder plugin], [libmad not found])

enable_shout2="$enable_shout"
MPD_AUTO_PKG(shout, SHOUT, [shout],
	[shout output plugin], [libshout not found])
if test x$enable_shout = xyes && test x$enable_shout2 = xauto; then
	enable_shout=auto
fi

dnl -------------------------------- libmpg123 --------------------------------
MPD_ENABLE_AUTO_PKG(mpg123, MPG123, [libmpg123],
	[libmpg123 decoder plugin], [libmpg123 not found])

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
MPD_ENABLE_AUTO_PKG(modplug, MODPLUG, [libmodplug],
	[modplug decoder plugin], [libmodplug not found])

dnl -------------------------------- libopus ----------------------------------
MPD_ENABLE_AUTO_PKG(opus, OPUS, [opus ogg],
	[opus decoder plugin], [libopus not found])

dnl -------------------------------- libsndfile -------------------------------
dnl See above test, which may disable this.
MPD_ENABLE_AUTO_PKG(sndfile, SNDFILE, [sndfile],
	[libsndfile decoder plugin], [libsndfile not found])

dnl --------------------------------- musepack --------------------------------

MPD_ENABLE_AUTO_LIB(mpc, MPCDEC, mpcdec, mpc_demux_init, [-lmpcdec], [],
	[Musepack decoder plugin], [libmpcdec not found])

dnl -------------------------------- Ogg Tremor -------------------------------

AC_ARG_WITH(tremor,
	AS_HELP_STRING([--with-tremor=PFX],
		[use Tremor (vorbisidec) integer Ogg Vorbis decoder (with optional prefix)]),,
	with_tremor=no)

AC_ARG_VAR([TREMOR_CFLAGS], [C compiler flags for Tremor])
AC_ARG_VAR([TREMOR_LIBS], [linker flags for Tremor])

if test x$with_tremor = xyes || test x$with_tremor = xno; then
	enable_tremor="$with_tremor"
	tremor_prefix=""
else
	tremor_prefix="$with_tremor"
	enable_tremor=yes
fi

if test x$enable_tremor = xyes; then
	if test x$TREMOR_CFLAGS = x && test x$tremor_prefix != x; then
		TREMOR_CFLAGS="-I$tremor_prefix/include"
	fi
	if test x$TREMOR_LIBS = x; then
		TREMOR_LIBS="-lvorbisidec"

		if test x$tremor_prefix != x; then
			TREMOR_LIBS="-L$tremor_prefix/lib $TREMOR_LIBS"
		fi
	fi

	MPD_WITH_LIBRARY([TREMOR],
		[AC_CHECK_FUNC([ov_read],,
			[AC_MSG_ERROR([libvorbisidec not found])])])

	AC_DEFINE(HAVE_TREMOR,1,
		[Define to use tremor (libvorbisidec) for ogg support])
	AC_DEFINE(ENABLE_VORBIS_DECODER, 1, [Define for Ogg Vorbis support])
else
	TREMOR_CFLAGS=
	TREMOR_LIBS=
fi

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
MPD_ENABLE_AUTO_PKG(wavpack, WAVPACK, [wavpack],
	[WavPack decoder plugin], [libwavpack not found])

dnl --------------------------------- WildMidi --------------------------------
MPD_ENABLE_AUTO_LIB(wildmidi, WILDMIDI, WildMidi, WildMidi_Init, [-lWildMidi], [],
	[WildMidi decoder plugin], [libwildmidi not found])

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
MPD_DEFINE_CONDITIONAL(enable_flac_encoder, ENABLE_FLAC_ENCODER,
	[FLAC encoder plugin])

dnl ------------------------------- Shine Encoder ------------------------------

MPD_ENABLE_AUTO_PKG(shine_encoder, SHINE, [shine >= 3.1],
	[shine encoder], [libshine not found])

dnl ---------------------------- Ogg Vorbis Encoder ---------------------------
MPD_ENABLE_AUTO_PKG(vorbis_encoder, VORBISENC, [vorbisenc vorbis ogg],
	[Ogg Vorbis encoder], [libvorbisenc not found])

dnl ------------------------------- LAME Encoder ------------------------------

MPD_ENABLE_AUTO_LIB(lame_encoder, LAME, mp3lame, lame_init, [-lmp3lame], [],
	[LAME encoder plugin], [libmp3lame not found])

dnl ----------------------------- TwoLAME Encoder -----------------------------
MPD_ENABLE_AUTO_PKG(twolame_encoder, TWOLAME, [twolame],
	[TwoLAME encoder plugin], [libtwolame not found])

dnl ------------------------------- WAVE Encoder ------------------------------
MPD_DEFINE_CONDITIONAL(enable_wave_encoder, ENABLE_WAVE_ENCODER,
	[PCM wave encoder plugin])

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

MPD_DEFINE_CONDITIONAL(enable_encoder, ENABLE_ENCODER,
		[the encoder plugins])

AM_CONDITIONAL(HAVE_OGG_ENCODER, test x$enable_vorbis_encoder = xyes || test x$enable_opus = xyes)

dnl ---------------------------------------------------------------------------
dnl Audio Output Plugins
dnl ---------------------------------------------------------------------------

dnl ----------------------------------- ALSA ----------------------------------
MPD_ENABLE_AUTO_PKG(alsa, ALSA, [alsa >= 0.9.0],
	[ALSA output plugin], [libasound not found],
	[$linux_auto])

dnl ----------------------------------- ROAR ----------------------------------
MPD_ENABLE_AUTO_PKG(roar, ROAR, [libroar >= 0.4.0],
	[RoarAudio output plugin], [libroar not found])

dnl ----------------------------------- FIFO ----------------------------------
if test x$enable_fifo = xyes; then
	AC_CHECK_FUNC([mkfifo],
		[enable_fifo=yes],
		[enable_fifo=no;AC_MSG_WARN([mkfifo not found -- disabling support for writing audio to a FIFO])])
fi

MPD_DEFINE_CONDITIONAL(enable_fifo, HAVE_FIFO,
	[support for writing audio to a FIFO])

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

MPD_DEFINE_CONDITIONAL(enable_httpd_output, ENABLE_HTTPD_OUTPUT,
	[the HTTP server output])

dnl ----------------------------------- JACK ----------------------------------
MPD_ENABLE_AUTO_PKG(jack, JACK, [jack >= 0.100],
	[JACK output plugin], [libjack not found])

if test x$enable_jack = xyes; then
	# check whether jack_set_info_function() is available
	old_LIBS=$LIBS
	LIBS="$LIBS $JACK_LIBS"

	AC_CHECK_FUNCS(jack_set_info_function)

	LIBS=$old_LIBS
fi

dnl ---------------------------------- libao ----------------------------------
MPD_ENABLE_AUTO_PKG(ao, AO, [ao],
	[libao output plugin], [libao not found])

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

MPD_DEFINE_CONDITIONAL(enable_openal, HAVE_OPENAL, [OpenAL support])

dnl ---------------------------- Open Sound System ----------------------------
if test x$enable_oss = xyes; then
	AC_CHECK_HEADER(sys/soundcard.h,
		[enable_oss=yes],
		[AC_MSG_WARN(Soundcard headers not found -- disabling OSS support);
			enable_oss=no])
fi

MPD_DEFINE_CONDITIONAL(enable_oss, HAVE_OSS, [Open Sound System])

dnl ----------------------------------- OSX -----------------------------------
if test x$enable_osx = xyes; then
	AC_DEFINE(HAVE_OSX, 1, [Define for compiling OS X support])
	LIBS="$LIBS -framework AudioUnit -framework CoreAudio -framework CoreServices"
fi

AM_CONDITIONAL(HAVE_OSX, test x$enable_osx = xyes)

dnl ------------------------------- Pipe Output -------------------------------
MPD_DEFINE_CONDITIONAL(enable_pipe_output, ENABLE_PIPE_OUTPUT,
		[support for writing audio to a pipe])

dnl -------------------------------- PulseAudio -------------------------------
MPD_ENABLE_AUTO_PKG(pulse, PULSE, [libpulse >= 0.9.16],
	[PulseAudio output plugin], [libpulse not found])

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

MPD_DEFINE_CONDITIONAL(enable_recorder_output, ENABLE_RECORDER_OUTPUT,
	[the recorder output])

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

MPD_DEFINE_CONDITIONAL(enable_shout, HAVE_SHOUT,
	[shoutcast output])

dnl --------------------------------- Solaris ---------------------------------

MPD_DEFINE_CONDITIONAL(enable_solaris_output, ENABLE_SOLARIS_OUTPUT,
	[Solaris /dev/audio support])

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
