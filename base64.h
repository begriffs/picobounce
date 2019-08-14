#ifndef BASE64_H
#define BASE64_H

char* base64( const void* binaryData, int len, int *flen );
unsigned char* unbase64( const char* ascii, int len, int *flen );

#endif
