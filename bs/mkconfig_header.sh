#!/bin/sh

# basic package info
p=mpd
v=0.12.0
b=warren.dukes@gmail.com

. bs/bs-lib.sh

cat <<EOF
#define PACKAGE "$p"
#define VERSION "$p"
#define PACKAGE_BUGREPORT "$b"
#define PACKAGE_NAME "$p"
#define PACKAGE_STRING "$p $v"
#define PACKAGE_TARNAME "$p"
#define PACKAGE_VERSION "$v"
EOF

# check for common headers:
ansi_headers='
assert
ctype
errno
limits
locale
math
signal
stdarg
stddef
stdint
stdio
stdlib
string
'
common_headers='
dlfcn
inttypes
memory
strings
sys/inttypes
sys/stat
sys/types
unistd
'

all_ansi=t
for h in $ansi_headers; do
	H="HAVE_`echo $h | tr a-z A-Z | tr / _`_H"
	if test_header $h; then
		echo "#define $H 1"
	else
		echo "/* #undef $H */"
		all_ansi=
	fi
done
test x$all_ansi = xt && echo "#define STDC_HEADERS 1"

for h in $common_headers; do
	H="HAVE_`echo $h | tr a-z A-Z | tr / _`_H"
	if test_header $h; then
		echo "#define $H 1"
	else
		echo "/* #undef $H */"
	fi
done

# test for langinfo.h and codeset
cat > t.c <<EOF
#include <langinfo.h>
int main () { char *cs = nl_langinfo(CODESET); return 0; }
EOF
run_cc
test $? -eq 0 && echo '#define HAVE_LANGINFO_CODESET 1'

# the only feature (non-external library) feature we currently have
if test x$want_ipv6 != xno; then
	cat > t.c <<EOF
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#ifdef PF_INET6
#ifdef AF_INET6
AP_maGiC_VALUE
#endif
#endif
EOF
	if $CPP t.c 2>&1 | grep AP_maGiC_VALUE >/dev/null 2>&1; then
		echo '#define HAVE_IPV6 1'
	fi
fi

rm -f t.o t.c
