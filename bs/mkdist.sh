#!/bin/sh
set -e
. bs/pkginfo.sh
head=${1-'HEAD'}
git_ver=
if git rev-parse --git-dir > /dev/null 2>&1; then
	git_ver=`git describe $head`
	git_ver=`expr "z$git_ver" : 'z.*\(-g[0-9a-f].*\)' || true`
	if test -n "$git_ver"; then
		dirty=`git diff-index --name-only HEAD 2>/dev/null || true`
		if test -n "$dirty"; then
			git_ver=$git_ver-dirty
		fi
	fi
fi

v=$v$git_ver
dir=$O/$p-$v
rm -rf "$dir"
git tar-tree $head $dir | tar x

at_files='
Makefile.in
aclocal.m4
compile
config.guess
config.h.in
config.sub
configure
depcomp
doc/Makefile.in
install-sh
ltmain.sh
missing
mkinstalldirs
src/Makefile.in
src/mp4ff/Makefile.in
'

for i in $at_files; do
	if test -f $i; then
		echo cp $i $dir/$i
		cp $i $dir/$i
	fi
done

cd $O
tar c $p-$v | gzip -9 > $p-$v.tar.gz
rm -rf $p-$v
echo "Generated tarball in: $p-$v.tar.gz"
