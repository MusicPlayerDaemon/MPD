#!/bin/sh
# based on mkinstalldirs:
#	Original author: Noah Friedman <friedman@prep.ai.mit.edu>
#	Created: 1993-05-16
#	Public domain.
errstatus=0
for file
do
	test -d "$file" && continue
	case $file in
	/*) pathcomp=/ ;;
	*)  pathcomp= ;;
	esac
	oIFS=$IFS
	IFS=/
	set fnord $file
	shift
	IFS=$oIFS
	for d
	do
		test "x$d" = x && continue
		pathcomp=$pathcomp$d
		case $pathcomp in
		-*) pathcomp=./$pathcomp ;;
		esac

		if test ! -d "$pathcomp"; then
			mkdir "$pathcomp" || lasterr=$?
			if test ! -d "$pathcomp"; then
				errstatus=$lasterr
			fi
		fi
		pathcomp=$pathcomp/
	done
done

exit $errstatus
