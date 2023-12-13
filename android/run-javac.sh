#!/bin/sh -e

JAVAC=$1
CLASSPATH=$2
JAVA_PKG_PATH=$3
ZIP=$4
DIRNAME=`dirname "$5"`
REALDIR=`realpath "$DIRNAME"`
BASENAME=`basename "$5"`
JARFILE=$REALDIR/$BASENAME
shift 5

D=`dirname "$JARFILE"`
GENSRC="$D/src"
GENCLASS="$D/classes"
GENINCLUDE="$D/include"

mkdir -p "$GENSRC/$JAVA_PKG_PATH"
"$JAVAC" -source 1.7 -target 1.7 -Xlint:-options \
	 -cp "$CLASSPATH" \
	 -h "$GENINCLUDE" \
	 -d "$GENCLASS" \
	 "$@"
cd "$GENCLASS"
zip -q -r "$JARFILE" .
