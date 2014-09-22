#ifndef _CU_DST_UNPACK_H_INCLUDED
#define _CU_DST_UNPACK_H_INCLUDED

#include "types.h"
#include "dst_data.h"

__FUNCTION_ATTRIBUTES__ int UnpackDSTframe(DstDec* D, uint8_t* DSTdataframe, uint8_t* DSDdataframe);

#endif
