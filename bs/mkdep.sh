#!/bin/sh
# $d must have a trailing slash $(dir file) in GNU Make
f="$1"
d="$2"
x="$3"
if test -z "$O"; then
	echo '$O= not defined or not a directory' >&2
	exit 1
fi
test -d "$O/$d" || "$SHELL" ./bs/mkdir_p.sh "$O/$d"
t="$O/t.$$.d"
depmode=
out=
if test -e "$O/depmode"; then
	. "$O/depmode"
fi

case "$depmode" in
mm)
	$CC -MM $CPPFLAGS $CFLAGS "$f" > "$t" 2>/dev/null
	;;
m)
	$CC -M $CPPFLAGS $CFLAGS "$f" > "$t" 2>/dev/null
	;;
none)
	echo "$O/$f: $f $HDR_DEP_HACK" | sed -e 's#c:#o:#' > "$x"+
	;;
*)
	# detect our depmode
	# -MM is gcc-specific...
	$CC -MM $CPPFLAGS $CFLAGS "$f" > "$t" 2>/dev/null
	if test $? -eq 0; then
		depmode=mm
	else
		# ok, maybe -M is supported...
		$CC -M $CPPFLAGS $CFLAGS "$f" \
				> "$t" 2>/dev/null
		if test $? -eq 0; then
			depmode=m
		else
			depmode=none
			# don't guess, fudge the dependencies by using
			# all headers
			echo "$O/$f: $f $HDR_DEP_HACK" \
					| sed -e 's#c:#o:#' > "$x"+
		fi
	fi
	echo "depmode=$depmode" > "$O/depmode"
	;;
esac

case "$depmode" in
m|mm)
	sed -e 's#.c$#.o#' -e "1s#^#$O/$d&#" < "$t" > "$x"+
	;;
esac
rm -f "$t"
exec mv "$x"+ "$x"
