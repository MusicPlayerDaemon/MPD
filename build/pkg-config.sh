#!/bin/sh -e

BIN=`dirname $0`
ROOT=`dirname "$BIN"`

export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR="${ROOT}/lib/pkgconfig:${ROOT}/share/pkgconfig"

exec /usr/bin/pkg-config "$@"
