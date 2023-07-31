#!/bin/sh -e

# This is a wrapper for pkg-config which helps with cross-compiling;
# it sets up environment variables to pkg-config searches for
# libraries in the sysroot where a copy of this script is located.

BIN=`dirname $0`
ROOT=`dirname "$BIN"`

export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR="${ROOT}/lib/pkgconfig:${ROOT}/share/pkgconfig"

exec /usr/bin/pkg-config "$@"
