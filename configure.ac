dnl AC_INIT(src/main.c)
dnl AM_INIT_AUTOMAKE(mpd, 0.12.0)

AC_PREREQ(2.58)
AC_INIT(mpd, 0.12.0, warren.dukes@gmail.com)
AM_INIT_AUTOMAKE($PACKAGE_NAME, $PACKAGE_VERSION)

dnl MAD wants this stuff
AC_SUBST(CCAS)
AC_SUBST(CCASFLAGS)

AC_SUBST(MAD_SUBDIR)
AC_SUBST(MAD_LIB)
AC_SUBST(ID3_SUBDIR)
AC_SUBST(ID3_LIB)
AC_SUBST(MP4FF_LIB)
AC_SUBST(MP4FF_SUBDIR)
AC_SUBST(MPD_LIBS)
AC_SUBST(MPD_CFLAGS)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

AM_CONFIG_HEADER(config.h)

MPD_CFLAGS=""
case "$CC" in
*gcc*)
	MPD_CFLAGS="-Wall"
	;;
esac
MPD_LIBS=""

AC_ARG_ENABLE(ao,[  --enable-ao    enable support for libao (default: disable)],[enable_ao=$enableval],[enable_ao=no])
AC_ARG_ENABLE(shout,[  --disable-shout    disable support for streaming through shout (default: enable)],[enable_shout=$enableval],[enable_shout=yes])
AC_ARG_ENABLE(iconv,[  --disable-iconv     disable iconv support (default: enable)],[enable_iconv=$enableval],[enable_iconv=yes])
AC_ARG_ENABLE(ipv6,[  --disable-ipv6     disable IPv6 support (default: enable)],[enable_ipv6=$enableval],[enable_ipv6=yes])
AC_ARG_ENABLE(sun,[  --disable-sun     disable sun support (default: enable)],[enable_sun=$enableval],[enable_sun=yes])
AC_ARG_ENABLE(oss,[  --disable-oss     disable OSS support (default: enable)],[enable_oss=$enableval],[enable_oss=yes])
AC_ARG_ENABLE(alsa,[  --disable-alsa     disable ALSA support (default: enable)],[enable_alsa=$enableval],[enable_alsa=yes])
AC_ARG_ENABLE(pulse,[  --disable-pulse    disable support for the PulseAudio sound server (default: enable)],[enable_pulse=$enableval],[enable_pulse=yes])
AC_ARG_ENABLE(mvp,[  --enable-mvp    enable support for Hauppauge Media MVP (default: disable)],[enable_mvp=$enableval],[enable_mvp=no])
AC_ARG_ENABLE(oggvorbis,[  --disable-oggvorbis     disable Ogg Vorbis support (default: enable)],[enable_oggvorbis=$enableval],enable_oggvorbis=yes)
AC_ARG_ENABLE(oggflac,[  --disable-oggflac     disable OggFLAC support (default: enable)],[enable_oggflac=$enableval],enable_oggflac=yes)
AC_ARG_ENABLE(flac,[  --disable-flac    disable flac support (default: enable)],[enable_flac=$enableval],[enable_flac=yes])
AC_ARG_ENABLE(mp3,[  --disable-mp3     disable mp3 support (default: enable)],[enable_mp3=$enableval],[enable_mp3=yes])
AC_ARG_ENABLE(aac,[  --disable-aac     disable AAC support (default: enable)],[enable_aac=$enableval],[enable_aac=yes])
AC_ARG_ENABLE(audiofile,[  --disable-audiofile disable audiofile support, disables wave support (default: enable)],[enable_audiofile=$enableval],[enable_audiofile=yes])
AC_ARG_ENABLE(mod,[  --enable-mod     enable MOD support (default: disable],[enable_mod=$enableval],[enable_mod=yes])
AC_ARG_ENABLE(mpc,[  --disable-mpc     disable musepack (MPC) support (default: enable)],[enable_mpc=$enableval],[enable_mpc=yes])
AC_ARG_ENABLE(id3,[  --disable-id3     disable id3 support (default: enable)],[enable_id3=$enableval],[enable_id3=yes])

AC_ARG_WITH(tremor,[[  --with-tremor[=PFX]   Use Tremor(vorbisidec) integer Ogg-Vorbis decoder (with optional prefix)]], use_tremor=yes; test x$withval != xyes && tremor_prefix="$withval",)
AC_ARG_WITH(tremor-libraries,[  --with-tremor-libraries=DIR   Directory where Tremor library is installed (optional)], tremor_libraries="$withval", tremor_libraries="")
AC_ARG_WITH(tremor-includes,[  --with-tremor-includes=DIR   Directory where Tremor header files are installed (optional)], tremor_includes="$withval", tremor_includes="")

AC_ARG_WITH(iconv,[  --with-iconv=PFX   Prefix where iconv is installed (optional)], iconv_prefix="$withval", iconv_prefix="")
AC_ARG_WITH(iconv-libraries,[  --with-iconv-libraries=DIR   Directory where libiconv library is installed (optional)], iconv_libraries="$withval", iconv_libraries="")
AC_ARG_WITH(iconv-includes,[  --with-iconv-includes=DIR   Directory where libiconv header files are installed (optional)], iconv_includes="$withval", iconv_includes="")

AC_ARG_WITH(id3tag,[  --with-id3tag=PFX   Prefix where libid3tag is installed (optional)], id3tag_prefix="$withval", id3tag_prefix="")
AC_ARG_WITH(id3tag-libraries,[  --with-id3tag-libraries=DIR   Directory where libid3tag library is installed (optional)], id3tag_libraries="$withval", id3tag_libraries="")
AC_ARG_WITH(id3tag-includes,[  --with-id3tag-includes=DIR   Directory where libid3tag header files are installed (optional)], id3tag_includes="$withval", id3tag_includes="")

AC_ARG_WITH(mad,[  --with-mad=PFX   Prefix where libmad is installed (optional)], mad_prefix="$withval", mad_prefix="")
AC_ARG_WITH(mad-libraries,[  --with-mad-libraries=DIR   Directory where libmad library is installed (optional)], mad_libraries="$withval", mad_libraries="")
AC_ARG_WITH(mad-includes,[  --with-mad-includes=DIR   Directory where mad header files are installed (optional)], mad_includes="$withval", mad_includes="")

AC_ARG_WITH(faad,[  --with-faad=PFX   Prefix where faad2 is installed], faad_prefix="$withval", faad_prefix="")
AC_ARG_WITH(faad-libraries,[  --with-faad-libraries=DIR   Directory where faad2 library is installed (optional)], faad_libraries="$withval", faad_libraries="")
AC_ARG_WITH(faad-includes,[  --with-faad-includes=DIR   Directory where faad2 header files are installed (optional)], faad_includes="$withval", faad_includes="")

AC_C_BIGENDIAN

AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

AC_CHECK_HEADER(sys/inttypes.h,AC_DEFINE(HAVE_SYS_INTTYPES_H,1,[Define if sys/types.h present]),)

AC_CHECK_LIB(socket,socket,MPD_LIBS="$MPD_LIBS -lsocket",)
AC_CHECK_LIB(nsl,gethostbyname,MPD_LIBS="$MPD_LIBS -lnsl",)

AC_CHECK_LIB(m,exp,MPD_LIBS="$MPD_LIBS -lm",)
AC_CHECK_FUNCS(setenv)


dnl doesn't work for systems that don't have CODESET like OpenBSD
dnl AC_CHECK_HEADER(langinfo.h,[enable_langinfo=yes;AC_DEFINE(HAVE_LANGINFO,1,[Define if nl_langinfo.h is present])],enable_langinfo=no)
AM_LANGINFO_CODESET
AC_CHECK_HEADER(locale.h,[enable_locale=yes;AC_DEFINE(HAVE_LOCALE,1,[Define if locale.h is present])],enable_locale=no)

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

enable_osx=no
case $host in
	*-darwin*)
                AC_DEFINE(HAVE_OSX, 1, [Define for compiling OS X support])
		MPD_LIBS="$MPD_LIBS -framework AudioUnit -framework CoreServices"
		enable_osx=yes ;;
esac

if test x$enable_oggvorbis = xno; then
	AC_MSG_WARN("disabling shout streaming support because vorbis encoder was not found")
	enable_shout=no
elif test x$use_tremor = xyes; then
	AC_MSG_WARN("disabling shout streaming support because tremor does not support vorbis encoding")
	enable_shout=no
fi

if test x$enable_ao = xyes; then
	XIPH_PATH_AO([AC_DEFINE(HAVE_AO, 1, [Define to play with ao]) MPD_LIBS="$MPD_LIBS $AO_LIBS" MPD_CFLAGS="$MPD_CFLAGS $AO_CFLAGS"], enable_ao=no)
fi

if test x$enable_shout = xyes; then
	XIPH_PATH_SHOUT([AC_DEFINE(HAVE_SHOUT, 1, [Define to enable libshout support]) MPD_LIBS="$MPD_LIBS $SHOUT_LIBS" MPD_CFLAGS="$MPD_CFLAGS $SHOUT_CFLAGS"], enable_shout=no)
fi

if test x$enable_sun = xyes; then
	AC_CHECK_HEADER(sys/audioio.h,[enable_sun=yes;AC_DEFINE(HAVE_SUN,1,[Define to enable SUN audio support])],[AC_MSG_WARN(Sun audioio headers not found -- disabling SUN support);enable_sun=no])
fi

if test x$enable_oss = xyes; then
	AC_CHECK_HEADER(sys/soundcard.h,[enable_oss=yes;AC_DEFINE(HAVE_OSS,1,[Define to enable OSS])],[AC_MSG_WARN(Soundcard headers not found -- disabling OSS support);enable_oss=no])
fi

if test x$enable_pulse = xyes; then
	PKG_CHECK_MODULES([PULSE], [libpulse-simple],
	                  [enable_pulse=yes;AC_DEFINE([HAVE_PULSE], 1, [Define to enable PulseAudio])] MPD_LIBS="$MPD_LIBS $PULSE_LIBS" MPD_CFLAGS="$MPD_CFLAGS $PULSE_CFLAGS",
	                  [enable_pulse=no;AC_MSG_WARN([PulseAudio not found -- disabling])])
fi

if test x$enable_mvp = xyes; then
   AC_DEFINE(HAVE_MVP,1,[Define to enable Hauppauge Media MVP support])
fi

if test x$enable_alsa = xyes; then
	AM_PATH_ALSA(0.9.0,[AC_DEFINE(HAVE_ALSA,1,[Define to enable ALSA support]) MPD_LIBS="$MPD_LIBS $ALSA_LIBS" MPD_CFLAGS="$MPD_CFLAGS $ALSA_CFLAGS"],enable_alsa=no)
fi

if test x$enable_iconv = xyes; then
  	if test "x$iconv_libraries" != "x" ; then
  		ICONV_LIBS="-L$iconv_libraries"
  	elif test "x$iconv_prefix" != "x" ; then
    		ICONV_LIBS="-L$iconv_prefix/lib"
  	elif test "x$prefix" != "xNONE"; then
    		ICONV_LIBS="-L$libdir"
  	fi

  	ICONV_LIBS="$ICONV_LIBS -liconv"

  	if test "x$iconv_includes" != "x" ; then
  		ICONV_CFLAGS="-I$iconv_includes"
  	elif test "x$iconv_prefix" != "x" ; then
    		ICONV_CFLAGS="-I$iconv_prefix/include"
  	elif test "x$prefix" != "xNONE"; then
    		ICONV_CFLAGS="-I$includedir"
  	fi

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $MPD_CFLAGS $ICONV_CFLAGS"
	LIBS="$LIBS $MPD_LIBS $ICONV_LIBS"
	CPPFLAGS=$CFLAGS
	AC_CHECK_HEADER(iconv.h,MPD_CFLAGS="$MPD_CFLAGS $ICONV_CFLAGS",enable_iconv=no)
	if test x$enable_iconv = xyes; then
		AC_CHECK_LIB(iconv,main,MPD_LIBS="$MPD_LIBS $ICONV_LIBS",)
		AC_DEFINE(HAVE_ICONV,1,[Define to use iconv])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
fi

ID3_SUBDIR=""

if test x$enable_id3 = xyes; then
	if test "x$id3tag_libraries" != "x" ; then
		ID3TAG_LIBS="-L$id3tag_libraries"
	elif test "x$id3tag_prefix" != "x" ; then
		ID3TAG_LIBS="-L$id3tag_prefix/lib"
	elif test "x$prefix" != "xNONE"; then
		ID3TAG_LIBS="-L$libdir"
	fi

	ID3TAG_LIBS="$ID3TAG_LIBS -lid3tag -lz"

	if test "x$id3tag_includes" != "x" ; then
		ID3TAG_CFLAGS="-I$id3tag_includes"
	elif test "x$id3tag_prefix" != "x" ; then
		ID3TAG_CFLAGS="-I$id3tag_prefix/include"
	elif test "x$prefix" != "xNONE"; then
		ID3TAG_CFLAGS="-I$includedir"
	fi

	ID3TAG_CFLAGS="$ID3TAG_CFLAGS"

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $MPD_CFLAGS $ID3TAG_CFLAGS"
	LIBS="$LIBS $MPD_LIBS $ID3TAG_LIBS"
	CPPFLAGS=$CFLAGS
	AC_CHECK_HEADERS(id3tag.h,use_libid3tag=yes,
		[use_libid3tag=no])
	if test x$use_libid3tag = xyes; then
		AC_CHECK_LIB(id3tag,id3_file_open,
			[ID3_LIB="$ID3TAG_LIBS";
			MPD_CFLAGS="$MPD_CFLAGS $ID3TAG_CFLAGS";
			use_libid3tag=yes],
			[use_libid3tag=no])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
	if $use_libid3tag = xyes; then
		AC_DEFINE(HAVE_ID3TAG,1,[Define to use id3tag])
	else
		enable_id3=no
	fi
fi

MAD_SUBDIR=""

if test x$enable_mp3 = xyes; then
	if test "x$mad_libraries" != "x" ; then
		MAD_LIBS="-L$mad_libraries"
	elif test "x$mad_prefix" != "x" ; then
		MAD_LIBS="-L$mad_prefix/lib"
	elif test "x$prefix" != "xNONE"; then
		MAD_LIBS="-L$libdir"
	fi

	MAD_LIBS="$MAD_LIBS -lmad"

	if test "x$mad_includes" != "x" ; then
		MAD_CFLAGS="-I$mad_includes"
	elif test "x$mad_prefix" != "x" ; then
		MAD_CFLAGS="-I$mad_prefix/include"
	elif test "x$prefix" != "xNONE"; then
		MAD_CFLAGS="-I$includedir"
	fi

	oldcflags=$CFLAGS
	oldlibs=$LIBS
	oldcppflags=$CPPFLAGS
	CFLAGS="$CFLAGS $MPD_CFLAGS $MAD_CFLAGS"
	LIBS="$LIBS $MPD_LIBS $MAD_LIBS"
	CPPFLAGS=$CFLAGS
	AC_CHECK_HEADERS(mad.h,use_libmad=yes,
		[use_libmad=no])
	if test x$use_libmad = xyes; then
		AC_CHECK_LIB(mad,mad_stream_init,[MAD_LIB="$MAD_LIBS";
			MPD_CFLAGS="$MPD_CFLAGS $MAD_CFLAGS";
			use_libmad=yes],[use_libmad=no])
	fi
	CFLAGS=$oldcflags
	LIBS=$oldlibs
	CPPFLAGS=$oldcppflags
	if test x$use_libmad = xyes; then
		AC_DEFINE(HAVE_MAD,1,[Define to use libmad])
	else
		enable_mp3=no
	fi
fi

if test x$enable_mpc = xyes; then
  	if test "x$mpcdec_libraries" != "x" ; then
  		MPCDEC_LIBS="-L$mpcdec_libraries"
  	elif test "x$mpcdec_prefix" != "x" ; then
    		MPCDEC_LIBS="-L$faad_prefix/lib"
  	elif test "x$prefix" != "xNONE"; then
    		MPCDEC_LIBS="-L$libdir"
  	fi

 	MPCDEC_LIBS="$MPCDEC_LIBS -lmpcdec"

  	if test "x$mpcdec_includes" != "x" ; then
  		MPCDEC_CFLAGS="-I$mpcdec_includes"
  	elif test "x$mpcdec_prefix" != "x" ; then
    		MPCDEC_CFLAGS="-I$mpcdec_prefix/include"
  	elif test "x$prefix" != "xNONE"; then
    		MPCDEC_CFLAGS="-I$includedir"
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

MP4FF_SUBDIR=""

if test x$enable_aac = xyes; then
  	if test "x$faad_libraries" != "x" ; then
  		FAAD_LIBS="-L$faad_libraries"
  	elif test "x$faad_prefix" != "x" ; then
    		FAAD_LIBS="-L$faad_prefix/lib"
  	elif test "x$prefix" != "xNONE"; then
    		FAAD_LIBS="-L$libdir"
  	fi

 	FAAD_LIBS="$FAAD_LIBS -lfaad"

  	if test "x$faad_includes" != "x" ; then
  		FAAD_CFLAGS="-I$faad_includes"
  	elif test "x$faad_prefix" != "x" ; then
    		FAAD_CFLAGS="-I$faad_prefix/include"
  	elif test "x$prefix" != "xNONE"; then
    		FAAD_CFLAGS="-I$includedir"
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
		AC_CHECK_LIB(faad,faacDecInit2,[MPD_LIBS="$MPD_LIBS $FAAD_LIBS";MPD_CFLAGS="$MPD_CFLAGS $FAAD_CFLAGS";MP4FF_SUBDIR="mp4ff";MP4FF_LIB="mp4ff/libmp4ff.la"],enable_aac=no)
		if test x$enable_aac = xno; then
			enable_aac=yes
			AC_CHECK_LIB(faad,NeAACDecInit2,[MPD_LIBS="$MPD_LIBS $FAAD_LIBS";MPD_CFLAGS="$MPD_CFLAGS $FAAD_CFLAGS";MP4FF_SUBDIR="mp4ff";MP4FF_LIB="mp4ff/libmp4ff.la"],enable_aac=no)
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

if test x$enable_oggvorbis = xyes && test x$use_tremor != xyes; then
	XIPH_PATH_OGG(MPD_LIBS="$MPD_LIBS $OGG_LIBS" MPD_CFLAGS="$MPD_CFLAGS $OGG_CFLAGS",enable_oggvorbis=no)
fi

if test x$use_tremor = xyes; then
     	if test "x$tremor_libraries" != "x" ; then
  		TREMOR_LIBS="-L$tremor_libraries"
  	elif test "x$tremor_prefix" != "x" ; then
    		TREMOR_LIBS="-L$tremor_prefix/lib"
  	elif test "x$prefix" != "xNONE"; then
    		TREMOR_LIBS="-L$libdir"
  	fi
 	TREMOR_LIBS="$TREMOR_LIBS -lvorbisidec"
  	if test "x$tremor_includes" != "x" ; then
  		TREMOR_CFLAGS="-I$tremor_includes"
  	elif test "x$tremor_prefix" != "x" ; then
    		TREMOR_CFLAGS="-I$tremor_prefix/include"
  	elif test "x$prefix" != "xNONE"; then
    		TREMOR_CFLAGS="-I$includedir"
  	fi
	ac_save_CFLAGS="$CFLAGS"
	ac_save_LIBS="$LIBS"
	CFLAGS="$CFLAGS $TREMOR_CFLAGS"
	LIBS="$LIBS $TREMOR_LIBS"
	AC_CHECK_LIB(vorbisidec,ov_read,,enable_oggvorbis=no;AC_MSG_WARN([vorbisidec lib needed for ogg support with tremor -- disabling ogg support]))
	CFLAGS="$ac_save_CFLAGS"
	LIBS="$ac_save_LIBS"
fi

if test x$enable_oggvorbis = xyes; then
	if test x$use_tremor = xyes; then
		VORBIS_LIBS="$TREMOR_LIBS"
		VORBIS_CFLAGS="$TREMOR_CFLAGS"
	else
		enable_vorbistest=no
		XIPH_PATH_VORBIS(,enable_oggvorbis=no)
	fi
	if test x$enable_oggvorbis = xyes; then

dnl
dnl Vorbis Test
dnl vorbistest in XIPH_PATH_OGG is broken on my debian system
dnl so here i used my own hacked up version till i get vorbis-dev gets
dnl upgraded in debian (hopefully when 1.0.1 comes out)
dnl
dnl This version also tests Tremor, which has no vorbistest of its own.
dnl
		
  		AC_MSG_CHECKING(that Vorbis is usable)

    		ac_save_CFLAGS="$CFLAGS"
    		ac_save_LIBS="$LIBS"
    		CFLAGS="$CFLAGS $VORBIS_CFLAGS $OGG_CFLAGS"
		if test x$use_tremor = xyes; then
		   CFLAGS="$CFLAGS -DTREMOR"
		fi
    		LIBS="$LIBS $VORBIS_LIBS $VORBISENC_LIBS $OGG_LIBS"

      		AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TREMOR
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#endif

int main ()
{
    vorbis_block 	vb;
    vorbis_dsp_state	vd;
    vorbis_info		vi;

    vorbis_info_init (&vi);
#ifndef TREMOR
    vorbis_encode_init (&vi, 2, 44100, -1, 128, -1);
#endif
    /*vorbis_analysis_init (&vd, &vi);
    vorbis_block_init (&vd, &vb);*/
    /* this function was added in 1.0rc3, so this is what we're testing for */
    /*vorbis_bitrate_addblock (&vb);*/

    return 0;
}

], AC_MSG_RESULT(yes), [AC_MSG_RESULT(no);enable_oggvorbis=no],[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       		CFLAGS="$ac_save_CFLAGS"
       		LIBS="$ac_save_LIBS"
	fi
dnl 
dnl End of Vorbis Test
dnl
	if test x$enable_oggvorbis = xyes; then
		MPD_LIBS="$MPD_LIBS $VORBIS_LIBS $VORBISFILE_LIBS $VORBISENC_LIBS" 
		MPD_CFLAGS="$MPD_CFLAGS $VORBIS_CFLAGS $VORBISFILE_CFLAGS $VORBISENC_CFLAGS"
	fi
fi

if test x$enable_oggvorbis = xyes; then
	AC_DEFINE(HAVE_OGGVORBIS,1,[Define for Ogg Vorbis support])
fi

if test x$use_tremor = xyes; then
	AC_DEFINE(HAVE_TREMOR,1,[Define to use tremor (libvorbisidec) for ogg support])
fi	

if test x$enable_flac = xyes; then
	oldmpdcflags="$MPD_CFLAGS"
	oldmpdlibs="$MPD_LIBS"
	AM_PATH_LIBFLAC(MPD_LIBS="$MPD_LIBS $LIBFLAC_LIBS" MPD_CFLAGS="$MPD_CFLAGS $LIBFLAC_CFLAGS",enable_flac=no)
fi


if test x$enable_flac = xyes; then
	oldcflags="$CFLAGS"
	oldlibs="$LIBS"
	CFLAGS="$CFLAGS $MPD_CFLAGS"
	LIBS="$LIBS $MPD_LIBS"
	AC_CHECK_LIB(FLAC, FLAC__metadata_object_vorbiscomment_find_entry_from,
	          ,[enable_flac=no;AC_MSG_WARN(You need FLAC 1.1 -- disabling flac support)])
	if test x$enable_flac = xno; then
		MPD_CFLAGS="$oldmpdcflags"
		MPD_LIBS="$oldmpdlibs"
	fi
	CFLAGS="$oldcflags"
	LIBS="$oldlibs"
fi


if test x$enable_flac = xyes; then
	AC_DEFINE(HAVE_FLAC,1,[Define for FLAC support])
fi


if test x$enable_oggflac = xyes; then
	oldmpdcflags="$MPD_CFLAGS"
	oldmpdlibs="$MPD_LIBS"
	AM_PATH_LIBOGGFLAC(MPD_LIBS="$MPD_LIBS $LIBOGGFLAC_LIBS" MPD_CFLAGS="$MPD_CFLAGS $LIBOGGFLAC_CFLAGS",enable_oggflac=no)
fi

if test x$enable_oggflac = xyes; then
	AC_DEFINE(HAVE_OGGFLAC,1,[Define for OggFLAC support])
fi


if test x$enable_audiofile = xyes; then
	AM_PATH_AUDIOFILE(0.1.7, MPD_LIBS="$MPD_LIBS $AUDIOFILE_LIBS" MPD_CFLAGS="$MPD_CFLAGS $AUDIOFILE_CFLAGS",
		[enable_audiofile=no;AC_MSG_WARN(You need audiofile  -- disabling audiofile support)])
fi

if test x$enable_audiofile = xyes; then
	AC_DEFINE(HAVE_AUDIOFILE,1,[Define for audiofile support])
fi

if test x$enable_mod = xyes; then
	AM_PATH_LIBMIKMOD(3.1.7, MPD_CFLAGS="$MPD_CFLAGS $LIBMIKMOD_CFLAGS"
		MPD_LIBS="$MPD_LIBS $LIBMIKMOD_LIBS $LIBMIKMOD_LDADD", enable_mod=no)
	if test x$enable_mod = xyes; then
		AC_DEFINE(HAVE_MIKMOD, 1, [Define for mikmod support])
	fi
fi

AC_OUTPUT(src/mp4ff/Makefile doc/Makefile src/Makefile Makefile )

echo ""
echo "########### MPD CONFIGURATION ############"
echo ""

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

if test x$enable_sun = xyes; then
	echo " Sun support ...................enabled"
else
	echo " Sun support ...................disabled"
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

if test x$enable_mvp = xyes; then
	echo " Media MVP support .............enabled"
else
	echo " Media MVP support .............disabled"
fi

if test x$enable_shout = xyes; then
	echo " Shout streaming support .......enabled"
else
	echo " Shout streaming support .......disabled"
fi

echo ""

if test x$enable_ao = xno &&
   test x$enable_oss = xno &&
   test x$enable_shout = xno &&
   test x$enable_sun = xno &&
   test x$enable_alsa = xno &&
   test x$enable_osx = xno &&
   test x$enable_pulse = xno &&
   test x$enable_mvp = xno; then
	AC_MSG_ERROR("No Audio Output types configured!")
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

if test x$enable_oggflac = xyes; then
	echo " OggFLAC support ...............enabled"
else
	echo " OggFLAC support ...............disabled"
fi

if test x$enable_audiofile = xyes; then
	echo " Wave file support .............enabled"
else
	echo " Wave file support .............disabled"
fi

if test x$enable_aac = xyes; then
	echo " MP4/AAC support ...............enabled"
else
	echo " MP4/AAC support ...............disabled"
fi

if test x$enable_mpc = xyes; then
	echo " Musepack (MPC) support ........enabled"
else
	echo " Musepack (MPC) support ........disabled"
fi

if test x$enable_mod = xyes; then
	echo " MOD support ...................enabled"
else
	echo " MOD support ...................disabled"
fi

if
	test x$enable_mp3 = xno &&
	test x$enable_oggvorbis = xno &&
	test x$enable_flac = xno && 
	test x$enable_oggflac = xno &&
	test x$enable_audiofile = xno && 
	test x$enable_aac = xno &&
	test x$enable_mpc = xno &&
	test x$enable_mod = xno; then
	AC_MSG_ERROR("No input plugins supported!")
fi

echo ""
echo "##########################################"
echo ""
echo "You are now ready to compile MPD"
echo "Type \"make\" to compile MPD"
