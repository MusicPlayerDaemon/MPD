AC_DEFUN([results], [
	printf '('
	if test x$[]enable_$1 = xyes; then
		printf '+'
	else
		printf '-'
	fi
	printf '%s) ' "$2"
])
