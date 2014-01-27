#!/bin/sh

set -e

rm -rf config.cache build
mkdir build

aclocal -I m4 $ACLOCAL_FLAGS
autoheader
automake --add-missing $AUTOMAKE_FLAGS
autoconf
