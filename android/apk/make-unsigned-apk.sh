#!/bin/sh -e

S=`dirname "$0"`
ANDROID_ABI=$1
STRIP=$2
ZIP=$3
UNSIGNED_APK=$4
LIBMPD_SO=$5
CLASSES_DEX=$6
RESOURCES_APK=$7
D=`dirname "$UNSIGNED_APK"`

rm -rf "$D/apk"
mkdir -p "$D/apk/lib/$ANDROID_ABI"

"$STRIP" "$LIBMPD_SO" -o "$D/apk/lib/$ANDROID_ABI/`basename $LIBMPD_SO`"
cp "$CLASSES_DEX" "$D/apk/"
cp "$RESOURCES_APK" "$UNSIGNED_APK"

cd "$D/apk"
exec zip -q -r -X "../`basename $UNSIGNED_APK`" .
