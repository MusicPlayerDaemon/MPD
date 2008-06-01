#!/bin/sh
# many parts of this blatantly ripped from GNU Autoconf macros
# Use this for cross-compiling, as we can't run cross-compiled binaries
# on our host.
if test -z "$O" || ! cd "$O"; then
	echo '$O= not defined or not a directory' >&2
	exit 1
fi

echo '/* architecture-specific settings (auto-detected): */'
# check sizes of various types we care for
# this is slightly slower but much easier to read and follow than the
# bisecting autoconf version
for t in short int long 'long long'; do
	> out
	for s in 2 4 6 8 12 1 16 24 32 0; do
		cat > t.c <<EOF
int main(void)
{
	static int x[(long int)(sizeof($t)) == $s ? 1 : -1];
	x[0] = 0;
	return 0;
}
EOF
		echo "trying sizeof($t) == $s" >> out
		$CC -o t.o $CFLAGS $CPPFLAGS t.c \
			>> out 2>&1 && break
	done
	if test $s -eq 0; then
		echo "Unable to calculate sizeof($t) for cross-compiling" >&2
		cat out >&2
		exit 1
	fi
	t=`echo $t | tr a-z A-Z | tr ' ' _`
	echo "#define SIZEOF_$t $s"
	echo "SIZEOF_$t := $s" >> config_detected.mk
done

# check endian-ness
cat > t.c <<EOF
short ascii_mm[] = { 0x4249, 0x4765, 0x6E44, 0x6961, 0x6E53, 0x7953, 0 };
short ascii_ii[] = { 0x694C, 0x5454, 0x656C, 0x6E45, 0x6944, 0x6E61, 0 };
void _ascii(void)
{
	char *s = (char *)ascii_mm;
	s = (char *)ascii_ii;
}

short ebcdic_ii[] = { 0x89D3, 0xE3E3, 0x8593, 0x95C5, 0x89C4, 0x9581, 0 };
short ebcdic_mm[] = { 0xC2C9, 0xC785, 0x95C4, 0x8981, 0x95E2, 0xA8E2, 0 };
void _ebcdic(void)
{
	char *s = (char *)ebcdic_mm;
	s = (char *)ebcdic_ii;
}

int main(void)
{
	_ascii();
	_ebcdic();

	return 0;
}
EOF

echo "compiling endian test" > out
$CC -o t.o $CFLAGS $CPPFLAGS t.c >> out 2>&1
if grep BIGenDianSyS t.o >/dev/null 2>&1; then
	echo "#define WORDS_BIGENDIAN 1"
	echo "WORDS_BIGENDIAN := 1" >> config_detected.mk
elif grep LiTTleEnDian t.o >/dev/null 2>&1; then
	echo "/* #undef WORDS_BIGENDIAN */"
	echo "# WORDS_BIGENDIAN :=" >> config_detected.mk
else
	echo "Unable to determine endian-ness for cross compiling" >&2
	cat out >&2
	exit 1
fi

echo ''

exec rm -f t.o out t.c
