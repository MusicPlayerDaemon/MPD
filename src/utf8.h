#ifndef UTF_8_H
#define UTF_8_H

unsigned char * asciiToUtf8(unsigned char c);

unsigned char * asciiStrToUtf8Dup(unsigned char * ascii);

unsigned char utf8ToAscii(unsigned char * utf8);

int validateUtf8Char(unsigned char * utf8Char);

int validUtf8String(unsigned char * string);

#endif
