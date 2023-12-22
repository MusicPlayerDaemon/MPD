#!/bin/sh -e

JAVAC=$1
CLASSPATH=$2
DIRNAME=$3

GENINCLUDE="$DIRNAME/include"

mkdir -p "$GENINCLUDE"
"$JAVAC" -source 1.8 -target 1.8 -Xlint:-options \
	 -cp "$CLASSPATH" \
	 -h "$GENINCLUDE"  \
	 "$4"

