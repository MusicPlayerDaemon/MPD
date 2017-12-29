#!/bin/sh -e

AIDL=$1
SRC=$2
DST=$3
GENSRC=$4
JAVA_PKG_PATH=$5

mkdir -p "$GENSRC/$JAVA_PKG_PATH"
cp "$SRC" "$GENSRC/$JAVA_PKG_PATH/"
"$AIDL" -I"$GENSRC" -o"$GENSRC" "$GENSRC/$JAVA_PKG_PATH/`basename $SRC`"
exec cp "$GENSRC/$JAVA_PKG_PATH/`basename $DST`" "$DST"
