#!/bin/sh
PWD=`pwd`

## If we're not in the scripts directory
## assume the base directory.
if test "`basename $PWD`" = "scripts"; then
	cd ../
else
	MYOLDPWD=`pwd`
	cd `dirname $0`/../
fi

if test -e Makefile
then
	make distclean
fi
./autogen.sh
make
make dist

if test "`basename $PWD`" = "scripts"; then
	cd contrib/
else
	cd $MYOLDPWD
fi
