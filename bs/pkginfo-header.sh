#!/bin/sh
# basic package info:
. bs/pkginfo.sh
cat <<EOF
/* Basic package info: */
#define PACKAGE "$p"
#define VERSION "$v"
#define PACKAGE_BUGREPORT "$b"
#define PACKAGE_NAME "$p"
#define PACKAGE_STRING "$p $v"
#define PACKAGE_TARNAME "$p"
#define PACKAGE_VERSION "$v"
#define PROTOCOL_VERSION "$pv"

EOF

