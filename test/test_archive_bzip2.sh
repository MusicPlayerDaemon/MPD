#!/bin/sh -e

SRC_BASE=meson.build
SRC="$(dirname $0)/../${SRC_BASE}"
DST="$(pwd)/test/tmp/${SRC_BASE}.bz2"

mkdir -p test/tmp
rm -f "$DST"
bzip2 -c "$SRC" >"$DST"
./test/run_input "$DST/${SRC_BASE}" |diff "$SRC" -
