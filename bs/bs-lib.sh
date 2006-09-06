#!/bin/sh
# common shell functions and variable setup for the bs build system,
# expect variables to be set and exported in bs.mk

test -z "$CC" && CC=cc
test -z "$CPP" && CPP='cc -E'
test -z "$PKGCONFIG" && PKGCONFIG=pkg-config

p ()
{
	echo >&2 "$@"
}

run_cc ()
{
	$CC $CPPFLAGS $CFLAGS $LDFLAGS \
		$cppflags $cflags $ldflags -o t.o t.c >/dev/null 2>&1
}

test_header ()
{
	cat > t.c <<EOF
#include <$1.h>
int main (void) { return 0; }
EOF
	run_cc
}

dep_paths ()
{
	name=$1
	eval "cflags=`echo '$'`${name}_cflags"
	eval "ldflags=`echo '$'`${name}_ldflags"
	eval "pfx=`echo '$'`${name}_pfx"
	if test -n "$pfx"; then
		cflags="$cflags -I$pfx/include"
		ldflags="$ldflags -L$pfx/lib"
	fi
}

test_compile ()
{
	h=shift
	cat > t.c <<EOF
#include <$h.h>
int main () { $@ return 0; }
EOF
	run_cc
}

