#!/bin/sh -e

JAVAC=$1
CLASSPATH=$2
GENINCLUDE=$3

"$JAVAC" -source 1.8 -target 1.8 -Xlint:-options \
	 -cp "$CLASSPATH" \
	 -h "$GENINCLUDE"  \
	 "$4"

