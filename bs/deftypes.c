#include <stdio.h>

int main(int argc, char argv[])
{
	long one = 1;
	puts( *((char *)(&one)) ? "/* #undef WORDS_BIGENDIAN */"
				: "#define WORDS_BIGENDIAN" );
	printf( "#define SIZEOF_INT %ld\n"
		"#define SIZEOF_LONG %ld\n"
		"#define SIZEOF_LONG_LONG %ld\n"
		"#define SIZEOF_SHORT %ld\n",
		(long int)(sizeof(int)),
		(long int)(sizeof(long)),
		(long int)(sizeof(long long)),
		(long int)(sizeof(short)) );

	return 0;
}
