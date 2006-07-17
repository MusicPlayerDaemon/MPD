#!/bin/sh
if test -e Makefile
then
	make distclean
fi
./autogen.sh --disable-aac
make
make dist
