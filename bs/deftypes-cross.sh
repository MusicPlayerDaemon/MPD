#!/bin/sh
cat <<EOF
/*
 * Manually set type sizes for cross compile
 * build-target: $TARGET
 * build-host:   $HOST
 */

/* If your target is big-endian, define the below: */
EOF

be='/* #undef WORDS_BIGENDIAN */'

# add more targets here
case "$TARGET" in
*-ppc* | *-sparc*)
	be='#define WORDS_BIGENDIAN'
	;;
esac
echo "$bs"

sizeof_int=0
sizeof_long=0
sizeof_long_long=0
sizeof_short=0

cat <<EOF
#define SIZEOF_INT $sizeof_int
#define SIZEOF_LONG $sizeof_long
#define SIZEOF_LONG_LONG $sizeof_long_long
#define SIZEOF_SHORT $sizeof_short
EOF

