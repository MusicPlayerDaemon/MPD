#ifndef __DST_AC_H_INCLUDED
#define __DST_AC_H_INCLUDED

#include "types.h"

__FUNCTION_ATTRIBUTES__ void DST_ACDecodeBit(ACData* AC, uint8_t* b, int p, uint8_t* cb, int fs, int flush);
__FUNCTION_ATTRIBUTES__ int DST_ACGetPtableIndex(long PredicVal, int PtableLen);

#endif
