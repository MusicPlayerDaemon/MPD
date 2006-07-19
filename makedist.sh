#!/bin/sh
if test -e Makefile
then
	make distclean
fi
./autogen.sh
make
make dist
