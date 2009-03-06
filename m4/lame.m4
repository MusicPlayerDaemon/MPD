dnl borrowed from oddsock.org
dnl AM_PATH_LAME([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for liblame, and define LAME_CFLAGS and LAME_LIBS
dnl
AC_DEFUN([AM_PATH_LAME],
[dnl
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(lame,
	AS_HELP_STRING([--with-lame=PFX],
		[prefix where liblame is installed (optional)]),,
	lame_prefix="")
AC_ARG_WITH(lame-libraries,
	AS_HELP_STRING([--with-lame-libraries=DIR],
		[directory where liblame library is installed (optional)]),,
	lame_libraries="")
AC_ARG_WITH(lame-includes,
	AS_HELP_STRING([--with-lame-includes=DIR],
		[directory where liblame header files are installed (optional)]),,
	lame_includes="")
AC_ARG_ENABLE(lametest,
	AS_HELP_STRING([--disable-lametest],
		[do not try to compile and run a test liblame program]),,
	enable_lametest=yes)

if test "x$lame_prefix" != "xno" ; then

  if test "x$lame_libraries" != "x" ; then
    LAME_LIBS="-L$lame_libraries"
  elif test "x$lame_prefix" != "x" ; then
    LAME_LIBS="-L$lame_prefix/lib"
  elif test "x$prefix" != "xNONE" ; then
    LAME_LIBS="-L$prefix/lib"
  fi

  LAME_LIBS="$LAME_LIBS -lmp3lame -lm"

  if test "x$lame_includes" != "x" ; then
    LAME_CFLAGS="-I$lame_includes"
  elif test "x$lame_prefix" != "x" ; then
    LAME_CFLAGS="-I$lame_prefix/include"
  elif test "x$prefix" != "xNONE"; then
    LAME_CFLAGS="-I$prefix/include"
  fi

  AC_MSG_CHECKING(for liblame)
  no_lame=""


  if test "x$enable_lametest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $LAME_CFLAGS"
    LIBS="$LIBS $LAME_LIBS"
dnl
dnl Now check if the installed liblame is sufficiently new.
dnl
      rm -f conf.lametest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lame/lame.h>

int main ()
{
  system("touch conf.lametest");
  return 0;
}

],, no_lame=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_lame" = "x" ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])
  else
     AC_MSG_RESULT(no)
     if test -f conf.lametest ; then
       :
     else
       echo "*** Could not run liblame test program, checking why..."
       CFLAGS="$CFLAGS $LAME_CFLAGS"
       LIBS="$LIBS $LAME_LIBS"
       AC_TRY_LINK([
#include <stdio.h>
#include <lame/lame.h>
],     [ return 0; ],
       [ echo "*** The test program compiled, but did not run. This usually means"
       echo "*** that the run-time linker is not finding liblame or finding the wrong"
       echo "*** version of liblame. If it is not finding liblame, you'll need to set your"
       echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
       echo "*** to the installed location  Also, make sure you have run ldconfig if that"
       echo "*** is required on your system"
       echo "***"
       echo "*** If you have an old version installed, it is best to remove it, although"
       echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
       [ echo "*** The test program failed to compile or link. See the file config.log for the"
       echo "*** exact error that occured. This usually means liblame was incorrectly installed"
       echo "*** or that you have moved liblame since it was installed." ])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
     LAME_CFLAGS=""
     LAME_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_DEFINE(HAVE_LAME, 1, [Define if you have liblame.])
  use_lame="1"
else
  LAME_CFLAGS=""
  LAME_LIBS=""
fi
  AC_SUBST(LAME_CFLAGS)
  AC_SUBST(LAME_LIBS)
  rm -f conf.lametest
])

