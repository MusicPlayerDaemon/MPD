AC_DEFUN([results], [
	dnl This is a hack to allow "with" names, otherwise "enable".
	num=`expr match $1 "with"`
	if test "$num" != "0"; then
		var="`echo '$'$1`"
	else
		var="`echo '$'enable_$1`"
	fi

	echo -n "("
	if eval "test x$var = xyes"; then
		echo -n "+"
	else
		echo -n "-"
	fi
	echo -n "$2) "
])
