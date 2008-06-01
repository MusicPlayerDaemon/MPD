#!/bin/sh
# sourcing this defines the folling variables:
# p: package, v: version, b: bugreport email, pv: protocol version

script='
/^AC_INIT\(/ {
	gsub("[() ]","",$0);
	gsub("AC_INIT","",$1);
	print "p="$1;
	print "v="$2;
	print "b="$3;
}
/^AC_DEFINE\(PROTOCOL_VERSION, / {
	gsub("[\" ]", "", $2);
	print "pv="$2;
}
'

eval $(awk -F, "$script" < configure.ac)
