#ifndef _CU_DST_DATA_H_INCLUDED
#define _CU_DST_DATA_H_INCLUDED

#include "types.h"

__FUNCTION_ATTRIBUTES__ int GetDSTDataPointer(StrData* SD, uint8_t** pBuffer);
__FUNCTION_ATTRIBUTES__ int ResetReadingIndex(StrData* SD);
__FUNCTION_ATTRIBUTES__ int ReadNextBitFromBuffer(StrData* SD, uint8_t* pBit);
__FUNCTION_ATTRIBUTES__ int ReadNextNBitsFromBuffer(StrData* SD, int32_t* pBits, int32_t NrBits);
__FUNCTION_ATTRIBUTES__ int ReadNextByteFromBuffer(StrData* SD, uint8_t* pByte);

__FUNCTION_ATTRIBUTES__ int FillBuffer(StrData* SD, uint8_t* pBuf, int32_t Size);

__FUNCTION_ATTRIBUTES__ int FIO_BitGetChrUnsigned(StrData* SD, int32_t Len, uint8_t* x);
__FUNCTION_ATTRIBUTES__ int FIO_BitGetIntUnsigned(StrData* SD, int32_t Len, int32_t* x);
__FUNCTION_ATTRIBUTES__ int FIO_BitGetIntSigned(StrData* SD, int32_t Len, int32_t* x);
__FUNCTION_ATTRIBUTES__ int FIO_BitGetShortSigned(StrData* SD, int32_t Len, int16_t* x);
__FUNCTION_ATTRIBUTES__ int get_in_bitcount(StrData* SD);

__FUNCTION_ATTRIBUTES__ int CreateBuffer(StrData* SD, int32_t Size);
__FUNCTION_ATTRIBUTES__ int DeleteBuffer(StrData* SD);

#endif

