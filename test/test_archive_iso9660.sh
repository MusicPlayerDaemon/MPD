#!/bin/sh -e

SRC_BASE=meson.build
SRC="$(dirname $0)/../${SRC_BASE}"
DST="$(pwd)/test/tmp/${SRC_BASE}.iso"

mkdir -p test/tmp
rm -f "$DST"
mkisofs -quiet -l -o "$DST" "$SRC"
./test/run_input "$DST/${SRC_BASE}" |diff "$SRC" -
