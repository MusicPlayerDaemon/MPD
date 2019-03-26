#!/bin/sh

cp "$2" "$1" && xres -o "$1" -- "$3" && mimeset -f "$1" || (rm -f "$1"; exit 1)
