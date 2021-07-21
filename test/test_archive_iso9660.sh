#!/bin/sh -e

SRC_BASE=meson.build
SRC="$(dirname $0)/../${SRC_BASE}"
DST="$(pwd)/test/tmp/${SRC_BASE}.iso"

mkdir -p test/tmp
rm -f "$DST"
mkisofs -quiet -l -o "$DST" "$SRC"

# Using an odd chunk size to check whether the plugin can cope with
# partial sectors
./test/run_input --chunk-size=1337 "$DST/${SRC_BASE}" |diff "$SRC" -
