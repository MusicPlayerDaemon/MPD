#!/bin/sh -e

SRC_BASE=meson.build
SRC="$(dirname $0)/../${SRC_BASE}"
DST="$(pwd)/test/tmp/${SRC_BASE}.zip"

mkdir -p test/tmp
rm -f "$DST"
zip --quiet --junk-paths "$DST" "$SRC"
./test/run_input "$DST/${SRC_BASE}" |diff "$SRC" -
