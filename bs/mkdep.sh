#!/bin/sh
f="$1"
d="`dirname $1`"
t=.tmp.$$

# -MM is gcc-specific...
$CC -MM $CPPFLAGS $CFLAGS "$f" > $t

if test $? -ne 0; then
	# ok, maybe -M is supported...
	$CC -M $CPPFLAGS $CFLAGS "$f" > "$t"

	# guess not, fudge the dependencies by using all headers
	if test $? -ne 0; then
		echo "$O/$f: $f $O/config.h $HDR_DEP_HACK" | sed -e 's#c:#o:#'
		exec rm -f $t
	fi
fi

sed -e 's#.c$#.o#' -e "1s#^#$O/$d/&#" < $t
exec rm -f $t
