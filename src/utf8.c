#include "utf8.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned char * asciiToUtf8(unsigned char c) {
	static unsigned char utf8[3];

	memset(utf8,0,3);

	if(c < 128) utf8[0] = c;
	else if(c<192) {
		utf8[0] = 194;
		utf8[1] = c;
	}
	else {
		utf8[0] = 195;
		utf8[1] = c-64;
	}

	return utf8;
}

unsigned char * asciiStrToUtf8Dup(unsigned char * ascii) {
	/* utf8 should have at most two char's per ascii char */
	int len = strlen(ascii)*2+1;
	unsigned char * ret = malloc(len);
	unsigned char * cp = ret;
	unsigned char * utf8;

	memset(ret,0,len);

	len = 0;

	while(*ascii) {
		utf8 = asciiToUtf8(*ascii);
		while(*utf8) {
			*(cp++) = *(utf8++);
			len++;
		}
		ascii++;
	}

	return realloc(ret,len+1);
}

unsigned char utf8ToAscii(unsigned char * utf8) {
	unsigned char c = 0;

	if(utf8[0]<128) return utf8[0];
	else if(utf8[0]==195) c+=64;
	else if(utf8[0]!=194) return '?';
	return c+utf8[1];
}

int validateUtf8Char(unsigned char * utf8Char) {
	if(utf8Char[0]<0x80) return 1;
	
	if(utf8Char[0]>=0xC0 && utf8Char[0]<=0xFD) {
		int count = 1;
		unsigned char t = 0x20;
		int i;
		while(count < 6 && (t & utf8Char[0])) {
			t = (t >> 1);
			count++;
		}
		if(count > 5) return 0;
		for(i=1;i<=count;i++) {
			if(utf8Char[i] < 0x80 || utf8Char[i] > 0xBF) return 0;
		}
		return count;
	}
	else return 0;
}

int validUtf8String(unsigned char * string) {
	int ret;

	while(*string) {
		ret = validateUtf8Char(string);
		if(!ret) return 0;
		string+= ret;
	}

	return 1;
}
