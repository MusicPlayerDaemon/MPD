dnl AC_INIT(src/main.c)
dnl AM_INIT_AUTOMAKE(mpd, 0.10.0)

AC_PREREQ(2.52)
AC_INIT(mpd, 0.10.1, shank@mercury.chem.pitt.edu)
AM_INIT_AUTOMAKE($PACKAGE_NAME, $PACKAGE_VERSION)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl MAD wants this stuff
AC_SUBST(CCAS)
AC_SUBST(CCASFLAGS)

AC_SUBST(MAD_SUBDIR)
AC_SUBST(MAD_LIB)
AC_SUBST(ID3_SUBDIR)
AC_SUBST(ID3_LIB)
AC_SUBST(MPD_LIBS)
AC_SUBST(MPD_CFLAGS)

MPD_CFLAGS="-Wall"
MPD_LIBS=""

AC_ARG_ENABLE(iconv,[  --disable-iconv     disable iconv support],,enable_iconv=yes)
AC_ARG_ENABLE(ipv6,[  --disable-ipv6     disable IPv6 support],,enable_ipv6=yes)
AC_ARG_ENABLE(ogg,[  --disable-ogg     disable ogg support],,enable_ogg=yes)
AC_ARG_ENABLE(flac,[  --disable-flac    disable flac support],,enable_flac=yes)
AC_ARG_ENABLE(mp3,[  --disable-mp3     disable mp3 support],,enable_mp3=yes)
AC_ARG_ENABLE(audiofile,[  --disable-audiofile disable audiofile support, disables wave support],,enable_audiofile=yes)
AC_ARG_ENABLE(mpd_mad,[  --enable-mpd-mad     use mpd libmad],use_mpd_mad=yes,)
AC_ARG_ENABLE(id3,[  --disable-id3     disable id3 support],,enable_id3=yes)
AC_ARG_ENABLE(mpd_id3tag,[  --enable-mpd-id3tag    use mpd libid3tag],use_mpd_id3tag=yes,)

AC_ARG_WITH(id3tag,[  --with-id3tag=PFX   Prefix where libid3tag is installed (optional)], id3tag_prefix="$withval", id3tag_prefix="")
AC_ARG_WITH(id3tag-libraries,[  --with-id3tag-libraries=DIR   Directory where libid3tag library is installed (optional)], id3tag_libraries="$withval", id3tag_libraries="")
AC_ARG_WITH(id3tag-includes,[  --with-id3tag-includes=DIR   Directory where libid3tag header files are installed (optional)], id3tag_includes="$withval", id3tag_includes="")

AC_ARG_WITH(mad,[  --with-mad=PFX   Prefix where libmad is installed (optional)], mad_prefix="$withval", mad_prefix="")
AC_ARG_WITH(mad-libraries,[  --with-mad-libraries=DIR   Directory where libmad library is installed (optional)], mad_libraries="$withval", mad_libraries="")
AC_ARG_WITH(mad-includes,[  --with-mad-includes=DIR   Directory where mad header files are installed (optional)], mad_includes="$withval", mad_includes="")

AC_C_BIGENDIAN

AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

AC_CHECK_LIB(socket,socket,MPD_LIBS="$MPD_LIBS -lsocket",)
AC_CHECK_LIB(nsl,gethostbyname,MPD_LIBS="$MPD_LIBS -lnsl",)

AC_CHECK_LIB(m,exp,MPD_LIBS="$MPD_LIBS -lm",)

AC_CHECK_HEADER(langinfo.h,MPD_CFLAGS="$MPD_CFLAGS -DHAVE_LANGINFO",)
AC_CHECK_HEADER(locale.h,MPD_CFLAGS="$MPD_CFLAGS -DHAVE_LOCALE",)

if test x$enable_iconv = xyes; then
	AC_CHECK_HEADER(iconv.h,MPD_CFLAGS="$MPD_CFLAGS -DHAVE_ICONV",enable_iconv=no)
	if test x$enable_iconv = xyes; then
		AC_CHECK_LIB(iconv,iconv,MPD_LIBS="$MPD_LIBS -liconv",)
	fi
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
	MPD_CFLAGS="$MPD_CFLAGS -DHAVE_IPV6"
	AC_MSG_RESULT([yes]),
	AC_MSG_RESULT([no])
)
fi

XIPH_PATH_AO(MPD_LIBS="$MPD_LIBS $AO_LIBS" MPD_CFLAGS="$MPD_CFLAGS $AO_CFLAGS",AC_MSG_ERROR(Must have libao installed!!!))
AC_CHECK_HEADER(sys/soundcard.h,,[MPD_CFLAGS="$MPD_CFLAGS -DNO_OSS_MIXER";AC_MSG_WARN(Soundcard headers not found -- disabling mixer)])

AC_MSG_CHECKING(for alsa)
AC_EGREP_CPP([AP_maGiC_VALUE],
[
#include <alsa/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR)
#if SND_LIB_MAJOR>0 || (SND_LIB_MAJOR==0 && SND_LIB_MINOR>=6)
AP_maGiC_VALUE
#endif
#endif
],
MPD_CFLAGS="$MPD_CFLAGS -DHAVE_ALSA"
MPD_LIBS="$MPD_LIBS -lasound"
AC_MSG_RESULT([yes]),
AC_MSG_RESULT([no])
)

ID3_SUBDIR=""

if test x$enable_id3 = xyes; then
	if test x$use_mpd_id3tag = xyes; then
		ID3_SUBDIR="libid3tag"
	else
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
    			ID3TAG_CFLAGS="-I$id3tag_prefix/lib"
  		elif test "x$prefix" != "xNONE"; then
    			ID3TAG_CFLAGS="-I$libdir"
  		fi

  		ID3TAG_CFLAGS="$ID3TAG_CFLAGS"

		oldcflags=$CFLAGS
		CFLAGS="$CFLAGS $ID3TAG_CFLAGS $ID3TAG_LIBS"
		AC_CHECK_LIB(id3tag,id3_file_open,[ID3_LIB="$ID3TAG_LIBS";
     			MPD_CFLAGS="$MPD_CFLAGS $ID3TAG_CFLAGS";
			use_libid3tag=yes],
			[use_libid3tag=no;use_mpd_id3tag=yes])
		CFLAGS=$oldcflags
	fi
	if test x$use_mpd_id3tag = xyes; then
		MPD_CFLAGS="$MPD_CFLAGS -DUSE_MPD_ID3TAG"
		ID3_LIB="libid3tag/libid3tag.la"
		ID3_SUBDIR="libid3tag"
		AC_CONFIG_SUBDIRS(src/libid3tag)
	fi
	MPD_CFLAGS="$MPD_CFLAGS -DHAVE_ID3TAG"
fi

MAD_SUBDIR=""

if test x$enable_mp3 = xyes; then
	if test x$use_mpd_mad = xyes; then
		MAD_SUBDIR="libmad"
	else
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
    			MAD_CFLAGS="-I$mad_prefix/lib"
  		elif test "x$prefix" != "xNONE"; then
    			MAD_CFLAGS="-I$libdir"
  		fi

		AC_CHECK_LIB(mad,mad_stream_init,[MAD_LIB="$MAD_LIBS";
			MPD_CFLAGS="$MPD_CFLAGS $MAD_CFLAGS";use_libmad=yes],
     			[use_libmad=no;use_mpd_mad=yes])
	fi
	if test x$use_mpd_mad = xyes; then
		MPD_CFLAGS="$MPD_CFLAGS -DUSE_MPD_MAD"
		MAD_LIB="libmad/libmad.la"
		MAD_SUBDIR="libmad"
		AC_CONFIG_SUBDIRS(src/libmad)
	fi
	MPD_CFLAGS="$MPD_CFLAGS -DHAVE_MAD"
fi

if test x$enable_ogg = xyes; then
	XIPH_PATH_OGG(MPD_LIBS="$MPD_LIBS $OGG_LIBS" MPD_CFLAGS="$MPD_CFLAGS $OGG_CFLAGS",enable_ogg=no)
fi

if test x$enable_ogg = xyes; then
	enable_vorbistest=no
	XIPH_PATH_VORBIS(,enable_ogg=no)
	if test x$enable_ogg = xyes; then

dnl
dnl Vorbis Test
dnl vorbistest in XIPH_PATH_OGG is broken on my debian system
dnl so here i used my own hacked up version till i get vorbis-dev gets
dnl upgraded in debian (hopefully when 1.0.1 comes out)
dnl
		
  		AC_MSG_CHECKING(that Vorbis is usable)

    		ac_save_CFLAGS="$CFLAGS"
    		ac_save_LIBS="$LIBS"
    		CFLAGS="$CFLAGS $VORBIS_CFLAGS $OGG_CFLAGS"
    		LIBS="$LIBS $VORBIS_LIBS $VORBISENC_LIBS $OGG_LIBS"

      		AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

int main ()
{
    vorbis_block 	vb;
    vorbis_dsp_state	vd;
    vorbis_info		vi;

    vorbis_info_init (&vi);
    vorbis_encode_init (&vi, 2, 44100, -1, 128, -1);
    /*vorbis_analysis_init (&vd, &vi);
    vorbis_block_init (&vd, &vb);*/
    /* this function was added in 1.0rc3, so this is what we're testing for */
    /*vorbis_bitrate_addblock (&vb);*/

    return 0;
}

], AC_MSG_RESULT(yes), AC_MSG_RESULT(no) enable_ogg=no,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       		CFLAGS="$ac_save_CFLAGS"
       		LIBS="$ac_save_LIBS"
	fi
dnl 
dnl End of Vorbis Test
dnl
	if test x$enable_ogg = xyes; then
		MPD_LIBS="$MPD_LIBS $VORBIS_LIBS $VORBISFILE_LIBS" 
		MPD_CFLAGS="$MPD_CFLAGS $VORBIS_CFLAGS $VORBISFILE_CFLAGS"
	fi
fi

if test x$enable_ogg = xyes; then
	MPD_CFLAGS="$MPD_CFLAGS -DHAVE_OGG"
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
	MPD_CFLAGS="$MPD_CFLAGS -DHAVE_FLAC"
fi

if test x$enable_audiofile = xyes; then
	AM_PATH_AUDIOFILE(0.1.7, MPD_LIBS="$MPD_LIBS -laudiofile" MPD_CFLAGS="$MPD_CFLAGS -DHAVE_AUDIOFILE",
		[enable_audiofile=no;AC_MSG_WARN(You need audiofile  -- disabling audiofile support)])
fi

AC_OUTPUT(doc/Makefile src/Makefile Makefile )
