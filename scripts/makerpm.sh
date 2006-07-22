#!/bin/bash

PWD=`pwd`

## If we're not in the scripts directory 
## assume the base directory.
if test "`basename $PWD`" != "scripts" && \
	test -d scripts; then
	cd scripts
fi

./makedist.sh

rpmbuild -bb mpd.spec

if test $? -eq 0; then
	echo 'Your RPM should be ready now'
else
	echo 'Something went wrong when building your RPM'
fi

if test -f ../mpd-?.??.?.tar.gz;
then
	rm ../mpd-?.??.?.tar.gz
fi

if test "`basename $PWD`" != "scripts"; then
	cd ..
fi
