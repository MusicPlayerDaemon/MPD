# Configure paths for the Audio File Library
# Bertrand Guiheneuf 98-10-21
# stolen from esd.m4 in esound :
# Manish Singh    98-9-30
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_AUDIOFILE([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for Audio File Library, and define AUDIOFILE_CFLAGS and AUDIOFILE_LIBS.
dnl
AC_DEFUN([AM_PATH_AUDIOFILE],
[dnl 
dnl Get compiler flags and libraries from the audiofile-config script.
dnl
AC_ARG_WITH(audiofile-prefix,[  --with-audiofile-prefix=PFX   Prefix where Audio File Library is installed (optional)],
            audiofile_prefix="$withval", audiofile_prefix="")
AC_ARG_WITH(audiofile-exec-prefix,[  --with-audiofile-exec-prefix=PFX Exec prefix where Audio File Library is installed (optional)],
            audiofile_exec_prefix="$withval", audiofile_exec_prefix="")
AC_ARG_ENABLE(audiofiletest, [  --disable-audiofiletest       Do not try to compile and run a test Audio File Library program], , enable_audiofiletest=yes)

  if test x$audiofile_exec_prefix != x ; then
     audiofile_args="$audiofile_args --exec-prefix=$audiofile_exec_prefix"
     if test x${AUDIOFILE_CONFIG+set} != xset ; then
        AUDIOFILE_CONFIG=$audiofile_exec_prefix/bin/audiofile-config
     fi
  fi
  if test x$audiofile_prefix != x ; then
     audiofile_args="$audiofile_args --prefix=$audiofile_prefix"
     if test x${AUDIOFILE_CONFIG+set} != xset ; then
        AUDIOFILE_CONFIG=$audiofile_prefix/bin/audiofile-config
     fi
  fi

  AC_PATH_PROG(AUDIOFILE_CONFIG, audiofile-config, no)
  min_audiofile_version=ifelse([$1], ,0.2.5,$1)
  AC_MSG_CHECKING(for Audio File Library - version >= $min_audiofile_version)
  no_audiofile=""
  if test "$AUDIOFILE_CONFIG" = "no" ; then
    no_audiofile=yes
  else
    AUDIOFILE_LIBS=`$AUDIOFILE_CONFIG $audiofileconf_args --libs`
    AUDIOFILE_CFLAGS=`$AUDIOFILE_CONFIG $audiofileconf_args --cflags`
    audiofile_major_version=`$AUDIOFILE_CONFIG $audiofile_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    audiofile_minor_version=`$AUDIOFILE_CONFIG $audiofile_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    audiofile_micro_version=`$AUDIOFILE_CONFIG $audiofile_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_audiofiletest" = "xyes" ; then
      AC_LANG_SAVE
      AC_LANG_C
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $AUDIOFILE_CFLAGS"
      LIBS="$LIBS $AUDIOFILE_LIBS"
dnl
dnl Now check if the installed Audio File Library is sufficiently new. 
dnl (Also checks the sanity of the results of audiofile-config to some extent.)
dnl
      rm -f conf.audiofiletest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audiofile.h>

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.audiofiletest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_audiofile_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_audiofile_version");
     exit(1);
   }

   if (($audiofile_major_version > major) ||
      (($audiofile_major_version == major) && ($audiofile_minor_version > minor)) ||
      (($audiofile_major_version == major) && ($audiofile_minor_version == minor) && ($audiofile_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'audiofile-config --version' returned %d.%d.%d, but the minimum version\n", $audiofile_major_version, $audiofile_minor_version, $audiofile_micro_version);
      printf("*** of the Audio File Library required is %d.%d.%d. If audiofile-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If audiofile-config was wrong, set the environment variable AUDIOFILE_CONFIG\n");
      printf("*** to point to the correct copy of audiofile-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_audiofile=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
       AC_LANG_RESTORE
     fi
  fi
  if test "x$no_audiofile" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$AUDIOFILE_CONFIG" = "no" ; then
       cat <<END
*** The audiofile-config script installed by the Audio File Library could
*** not be found.  If the Audio File Library was installed in PREFIX, make
*** sure PREFIX/bin is in your path, or set the AUDIOFILE_CONFIG
*** environment variable to the full path to audiofile-config.
END
     else
       if test -f conf.audiofiletest ; then
        :
       else
          echo "*** Could not run Audio File Library test program; checking why..."
          AC_LANG_SAVE
          AC_LANG_C
          CFLAGS="$CFLAGS $AUDIOFILE_CFLAGS"
          LIBS="$LIBS $AUDIOFILE_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <audiofile.h>
],      [ return 0; ],
        [ cat <<END
*** The test program compiled, but did not run.  This usually means that
*** the run-time linker is not finding Audio File Library or finding the
*** wrong version of Audio File Library.
***
*** If it is not finding Audio File Library, you'll need to set your
*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point
*** to the installed location.  Also, make sure you have run ldconfig if
*** that is required on your system.
***
*** If you have an old version installed, it is best to remove it, although
*** you may also be able to get things to work by modifying
*** LD_LIBRARY_PATH.
END
        ],
        [ echo "*** The test program failed to compile or link. See the file config.log"
          echo "*** for the exact error that occured. This usually means the Audio File"
          echo "*** Library was incorrectly installed or that you have moved the Audio"
          echo "*** File Library since it was installed. In the latter case, you may want"
          echo "*** to edit the audiofile-config script: $AUDIOFILE_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
          AC_LANG_RESTORE
       fi
     fi
     AUDIOFILE_CFLAGS=""
     AUDIOFILE_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(AUDIOFILE_CFLAGS)
  AC_SUBST(AUDIOFILE_LIBS)
  rm -f conf.audiofiletest
])
