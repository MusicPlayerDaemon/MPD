#!/bin/sh

set -e

rm -rf config.cache build
mkdir build

aclocal -I m4
autoheader
automake --add-missing --foreign
autoconf
