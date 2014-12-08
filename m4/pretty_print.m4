AC_DEFUN([results], [
	var="`echo '$'enable_$1`"

	printf '('
	if eval "test x$var = xyes"; then
		printf '+'
	else
		printf '-'
	fi
	printf '%s) ' "$2"
])
