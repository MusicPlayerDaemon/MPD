#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.
# (at one point this was based on the version in enlightenment's cvs)

package="mpd"

olddir="`pwd`"
srcdir="`dirname $0`"
test -z "$srcdir" && srcdir=.
cd "$srcdir"
DIE=
AM_VERSIONGREP="sed -e s/.*[^0-9\.]\([0-9]\.[0-9]\+\).*/\1/"
AC_VERSIONGREP="sed -e s/.*[^0-9\.]\([0-9]\.[0-9][0-9]\).*/\1/"
VERSIONMKINT="sed -e s/[^0-9]//"
if test -n "$AM_FORCE_VERSION"
then
	AM_VERSIONS="$AM_FORCE_VERSION"
else
	AM_VERSIONS='1.10'
fi
if test -n "$AC_FORCE_VERSION"
then
	AC_VERSIONS="$AC_FORCE_VERSION"
else
	AC_VERSIONS='2.60 2.61'
fi

versioned_bins ()
{
	bin="$1"
	needed_int=`echo $VERNEEDED | $VERSIONMKINT`
	for i in $VERSIONS
	do
		i_int=`echo $i | $VERSIONMKINT`
		if test $i_int -ge $needed_int
		then
			echo $bin-$i $bin$i $bin-$i_int $bin$i_int
		fi
	done
	echo $bin
}

for c in autoconf autoheader automake aclocal
do
	uc=`echo $c | tr '[:lower:]' '[:upper:]'`
	eval "val=`echo '$'$uc`"
	if test -n "$val"
	then
		echo "$uc=$val in environment, will not attempt to auto-detect"
		continue
	fi

	case "$c" in
	autoconf|autoheader)
		VERNEEDED=`fgrep AC_PREREQ configure.ac | $AC_VERSIONGREP`
		VERSIONS="$AC_VERSIONS"
		pkg=autoconf
		;;
	automake|aclocal)
		VERNEEDED=`fgrep AUTOMAKE_OPTIONS Makefile.am | $AM_VERSIONGREP`
		VERSIONS="$AM_VERSIONS"
		pkg=automake
		;;
	esac
	printf "checking for $c ... "
	for x in `versioned_bins $c`; do
		($x --version < /dev/null > /dev/null 2>&1) > /dev/null 2>&1
		if test $? -eq 0
		then
			echo $x
			eval $uc=$x
			break
		fi
	done
	eval "val=`echo '$'$uc`"
	if test -z "$val"
	then
		if test $c = $pkg
		then
			DIE="$DIE $c=$VERNEEDED"
		else
			DIE="$DIE $c($pkg)=$VERNEEDED"
		fi
	fi
done

if test -n "$DIE"
then
	echo "You must have the following installed to compile $package:"
	for i in $DIE
	do
		printf '  '
		echo $i | sed -e 's/(/ (from /' -e 's/=\(.*\)/ (>= \1)/'
	done
	echo "Download the appropriate package(s) for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
        exit 1
fi

echo "Generating configuration files for $package, please wait...."

ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I m4"

# /usr/share/aclocal is most likely included by default, already...
ac_local_paths='
/usr/local/share/aclocal
/sw/share/aclocal
/usr/pkg/share/aclocal
/opt/share/aclocal
/usr/gnu/share/aclocal
'

for i in $ac_local_paths; do
	if test -d "$i"; then
		ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $i"
		# we probably only want one of these...
		break
	fi
done

echo "  $ACLOCAL $ACLOCAL_FLAGS"
$ACLOCAL $ACLOCAL_FLAGS || exit 1

echo "  $AUTOHEADER"
$AUTOHEADER || exit 1

echo "  $AUTOMAKE --add-missing $AUTOMAKE_FLAGS"
$AUTOMAKE --add-missing $AUTOMAKE_FLAGS || exit 1

echo "  $AUTOCONF"
$AUTOCONF || exit 1

cd "$olddir"
if test x$NOCONFIGURE = x; then
	"$srcdir"/configure "$@" || exit 1
fi
