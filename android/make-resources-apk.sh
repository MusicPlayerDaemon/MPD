#!/bin/sh -e

S=`dirname "$0"`
AAPT=$1
BASE_JAR=$2
JAVA_PKG=$3
JAVA_PKG_PATH=$4
APK_FILE="$5"
D=`dirname "$APK_FILE"`

rm -rf "$D/res"
mkdir -p "$D/res/drawable" "$D/src"
cp "$D/icon.png" "$D/notification_icon.png"  "$D/res/drawable/"

"$AAPT" package -f -m --auto-add-overlay \
  --custom-package "$JAVA_PKG" \
  -M "$S/AndroidManifest.xml" \
  -S "$D/res" \
  -S "$S/res" \
  -J "$D/src" \
  -I "$BASE_JAR" \
  -F "$D/resources.apk"

cp "$D/src/$JAVA_PKG_PATH/R.java" "$D/"
