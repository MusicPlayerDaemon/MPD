#ifndef UTF_8_H
#define UTF_8_H

unsigned char * latin1ToUtf8(unsigned char c);

unsigned char * latin1StrToUtf8Dup(unsigned char * latin1);

unsigned char * utf8StrToLatin1Dup(unsigned char * utf8);

unsigned char utf8ToLatin1(unsigned char * utf8);

int validateUtf8Char(unsigned char * utf8Char);

int validUtf8String(unsigned char * string);

#endif
