#!/bin/sh
#
echo "Generating build information using aclocal, automake and autoconf"
echo "This may take a while ..."

# Touch the timestamps on all the files since CVS messes them up
#touch Makefile.am configure.in
#touch libid3tag/Makefile.am libid3tag/configure.ac
#touch libmad/Makefile.am libmad/configure.ac

#rm -f configure
#rm -f libid3tag/configure
#rm -f libmad/configure
#rm -f config.cache
#rm -f config.status
#rm -f libid3tag/config.status
#rm -f libmad/config.status
#rm -rf autom4te*.cache
#rm -rf libid3tag/autom4te*.cache
#rm -rf libmad/autom4te*.cache
#rm -f aclocal.m4
#rm -f libid3tag/aclocal.m4
#rm -f libmad/aclocal.m4

# Regenerate configuration files
libtoolize -f -c

for i in -1.7 -1.6 ''; do
	if [ -z $ACLOCAL ]; then
		which aclocal$i
		if [ "$?" = "0" ]; then
			ACLOCAL=aclocal$i
		fi
	fi
	if [ -z $AUTOMAKE ]; then
		which automake$i
		if [ "$?" = "0" ]; then
			AUTOMAKE=automake$i
		fi
	fi
done

if [ -d /usr/local/share/aclocal ]; then
	$ACLOCAL -I /usr/local/share/aclocal
else
	$ACLOCAL
fi
$AUTOMAKE --foreign --add-missing -c
autoconf

cd src/libid3tag
$ACLOCAL 
$AUTOMAKE --foreign --add-missing -c
autoconf
cd ../..

cd src/libmad
$ACLOCAL 
$AUTOMAKE --foreign --add-missing -c
autoconf
cd ../..

# Run configure for this platform
./configure "$@"

