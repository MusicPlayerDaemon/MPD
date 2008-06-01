#!/bin/sh
# detect the uninteresting (unlikely to be manually changed by the user)
# parts of config.h

if test -z "$O" || ! cd "$O"; then
	echo '$O= not defined or not a directory' >&2
	exit 1
fi

> config_detected.mk

ansi_headers='assert ctype errno limits locale math signal
stdarg stddef stdint stdio stdlib string'
common_headers='dlfcn inttypes memory strings sched
sys/inttypes sys/stat sys/types unistd'

all_ansi=t
echo '/* ANSI C headers: */'
for h in $ansi_headers; do
	if test x$h = xlocale; then
		H=HAVE_LOCALE
	else
		H="HAVE_`echo $h | tr a-z A-Z | tr / _`_H"
	fi
	cat > t.c <<EOF
#include <$h.h>
int main(void) { return 0; }
EOF
	if $CC -o t.o $CFLAGS $CPPFLAGS t.c >> out 2>&1; then
		echo "#define $H 1"
		echo "$H := 1" >> config_detected.mk
	else
		echo "/* #undef $H */"
		echo "# $H := " >> config_detected.mk
		all_ansi=
	fi
done
if test x$all_ansi = xt; then
	echo '#define STDC_HEADERS 1'
else
	echo '/* #undef STDC_HEADERS */'
fi
echo ''
echo '/* common system headers/features on UNIX and UNIX-like system: */'
for h in $common_headers; do
	H="HAVE_`echo $h | tr a-z A-Z | tr / _`_H"
	cat > t.c <<EOF
#include <$h.h>
int main(void) { return 0; }
EOF
	if $CC -o t.o $CFLAGS $CPPFLAGS t.c >> out 2>&1; then
		echo "#define $H 1"
		echo "$H := 1" >> config_detected.mk
	else
		echo "/* #undef $H */"
		echo "# $H :=" >> config_detected.mk
	fi
done

# test for setenv
cat > t.c <<EOF
#include <stdlib.h>
int main(void) { setenv("mpd","rocks", 1); return 0; }
EOF
if $CC -o t.o $CFLAGS $CPPFLAGS t.c >> out 2>&1; then
	echo '#define HAVE_SETENV 1'
	echo "HAVE_SETENV := 1" >> config_detected.mk
else
	echo '/* #undef HAVE_SETENV */'
	echo "# HAVE_SETENV :=" >> config_detected.mk
fi
echo ''

# test for alloca
cat > t.c <<EOF
#include <alloca.h>
int main(void) { char *x = (char *)alloca(2 * sizeof(int)); return 0; }
EOF
if $CC -o t.o $CFLAGS $CPPFLAGS t.c >> out 2>&1; then
	echo '#define HAVE_ALLOCA_H 1'
	echo "HAVE_ALLOCA_H := 1" >> config_detected.mk
else
	echo '/* #undef HAVE_ALLOCA_H */'
	echo "# HAVE_ALLOCA_H :=" >> config_detected.mk
fi
echo ''

exec rm -f out t.c t.o
