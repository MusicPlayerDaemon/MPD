AC_DEFUN([results], [
	dnl This is a hack to allow "with" names, otherwise "enable".
	num=`expr $1 : 'with'`
	if test "$num" != "0"; then
		var="`echo '$'$1`"
	else
		var="`echo '$'enable_$1`"
	fi

	printf '('
	if eval "test x$var = xyes"; then
		printf '+'
	elif test -n "$3" && eval "test x$var = x$3"; then
		printf '+'
	else
		printf '-'
	fi
	printf '%s) ' "$2"
])
