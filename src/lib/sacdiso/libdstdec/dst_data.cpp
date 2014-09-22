#include "dst_memory.h"
#include "dst_data.h"

__FUNCTION_ATTRIBUTES__ int getbits(StrData* S, long* outword, int out_bitptr);

__FUNCTION_ATTRIBUTES__ int GetDSTDataPointer(StrData* SD, uint8_t** pBuffer) {
	int hr = 0;
	*pBuffer = SD->DSTdata;
	return hr;
}

__FUNCTION_ATTRIBUTES__ int ResetReadingIndex(StrData* SD) {
	int hr = 0;
	SD->BitPosition = 0;
	SD->ByteCounter = 0;
	SD->DataByte    = 0;
	return hr;
}

__FUNCTION_ATTRIBUTES__ int CreateBuffer(StrData* SD, int32_t Size) {
	int hr = 0;
	if (Size > sizeof(SD->DSTdata)) {
		SD->TotalBytes = sizeof(SD->DSTdata);
		hr = -1;
	}
	else {
		SD->TotalBytes = Size;
	}
  return hr;
}

__FUNCTION_ATTRIBUTES__ int DeleteBuffer(StrData* SD) {
	int hr = 0;
	SD->TotalBytes = 0;
	ResetReadingIndex(SD);
	return hr;
}

__FUNCTION_ATTRIBUTES__ int FillBuffer(StrData* SD, uint8_t* pBuf, int32_t Size) {
	int hr = 0;
	hr = CreateBuffer(SD, Size);
	dst_memcpy(SD->DSTdata, pBuf, SD->TotalBytes);
  ResetReadingIndex(SD);
  return hr;
}

/***************************************************************************/
/*                                                                         */
/* name     : FIO_BitGetChrUnsigned                                        */
/*                                                                         */
/* function : Read a character as an unsigned number from file with a      */
/*            given number of bits.                                        */
/*                                                                         */
/* pre      : Len, x, output file must be open by having used getbits_init */
/*                                                                         */
/* post     : The second variable in function call is filled with the      */
/*            unsigned character read                                      */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int FIO_BitGetChrUnsigned(StrData* SD, int32_t Len, uint8_t* x) {
	int  return_value;
	long tmp;
	return_value = -1;
	if (Len > 0) {
		return_value = getbits(SD, &tmp, Len);
		*x = (unsigned char)tmp;
	}
	else if (Len == 0) {
		*x = 0;
		return_value = 0;
	}
	else {
		trc_printf("\nERROR: a negative number of bits allocated\n");
	}
	return return_value;
}

/***************************************************************************/
/*                                                                         */
/* name     : FIO_BitGetIntUnsigned                                        */
/*                                                                         */
/* function : Read an integer as an unsigned number from file with a       */
/*            given number of bits.                                        */
/*                                                                         */
/* pre      : Len, x, output file must be open by having used getbits_init */
/*                                                                         */
/* post     : The second variable in function call is filled with the      */
/*            unsigned integer read                                        */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int FIO_BitGetIntUnsigned(StrData* SD, int32_t Len, int32_t* x) {
	int  return_value;
	long tmp;
	return_value = -1;
	if (Len > 0) {
		return_value = getbits(SD, &tmp, Len);
		*x = (int)tmp;
	}
	else if (Len == 0) {
		*x = 0;
		return_value = 0;
	}
	else {
		trc_printf("\nERROR: a negative number of bits allocated\n");
	}
	return return_value;
}

/***************************************************************************/
/*                                                                         */
/* name     : FIO_BitGetIntSigned                                          */
/*                                                                         */
/* function : Read an integer as a signed number from file with a          */
/*            given number of bits.                                        */
/*                                                                         */
/* pre      : Len, x, output file must be open by having used getbits_init */
/*                                                                         */
/* post     : The second variable in function call is filled with the      */
/*            signed integer read                                          */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int FIO_BitGetIntSigned(StrData* SD, int32_t Len, int32_t* x) {
	int  return_value;
	long tmp;
	return_value = -1;
	if (Len > 0) {
		return_value = getbits(SD, &tmp, Len);
		*x = (int)tmp;
		if (*x >= (1 << (Len - 1))) {
			*x -= (1 << Len);
		}
	}
	else if (Len == 0) {
		*x = 0;
		return_value = 0;
	}
	else {
		trc_printf("\nERROR: a negative number of bits allocated\n");
	}
	return return_value;
}

/***************************************************************************/
/*                                                                         */
/* name     : FIO_BitGetShortSigned                                        */
/*                                                                         */
/* function : Read a short integer as a signed number from file with a     */
/*            given number of bits.                                        */
/*                                                                         */
/* pre      : Len, x, output file must be open by having used getbits_init */
/*                                                                         */
/* post     : The second variable in function call is filled with the      */
/*            signed short integer read                                    */
/*                                                                         */
/* uses     : stdio.h, stdlib.h                                            */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int FIO_BitGetShortSigned(StrData* SD, int32_t Len, int16_t* x) {
	int  return_value;
	long tmp;
	return_value = -1;
	if (Len > 0) {
		return_value = getbits(SD, &tmp, Len);
		*x = (short)tmp;
		if (*x >= (1 << (Len - 1))) {
			*x -= (1 << Len);
		}
	}
	else if (Len == 0) {
		*x = 0;
		return_value = 0;
	}
	else {
		trc_printf("\nERROR: a negative number of bits allocated\n");
	}
	return return_value;
}

/***************************************************************************/
/*                                                                         */
/* name     : getbits                                                      */
/*                                                                         */
/* function : Read bits from the bitstream and decrement the counter.      */
/*                                                                         */
/* pre      : out_bitptr                                                   */
/*                                                                         */
/* post     : m_ByteCounter, outword, returns EOF on EOF or 0 otherwise.   */
/*                                                                         */
/* uses     : stdio.h                                                      */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int getbits(StrData* SD, long* outword, int out_bitptr) {
	const int masks[] = { 0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff };
	if (out_bitptr == 1) {
		if (SD->BitPosition == 0) {
			SD->DataByte = SD->DSTdata[SD->ByteCounter++];
			if (SD->ByteCounter > SD->TotalBytes) {
				return -1; /* EOF */
			}
			SD->BitPosition = 8;
		}
		SD->BitPosition--;
		*outword = (SD->DataByte >> SD->BitPosition) & 1;
		return 0;
	}
	*outword = 0;
	while(out_bitptr > 0)	{
		int thisbits, mask, shift;
		if (!SD->BitPosition) {
			SD->DataByte = SD->DSTdata[SD->ByteCounter++];
			if (SD->ByteCounter > SD->TotalBytes)	{
				return -1; /* EOF */
			}
			SD->BitPosition = 8;
		}
		thisbits = MIN(SD->BitPosition, out_bitptr);
		shift = (SD->BitPosition - thisbits);
		mask = masks[thisbits] << shift;
		shift = (out_bitptr - thisbits) - shift;
		if (shift <= 0)
			*outword |= ((SD->DataByte & mask) >> -shift);
		else
			*outword |= ((SD->DataByte & mask) << shift);
		out_bitptr -= thisbits;
		SD->BitPosition -= thisbits;
	}
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : get_bitcount                                                 */
/*                                                                         */
/* function : Reset the bits-written counter.                              */
/*                                                                         */
/* pre      : None                                                         */
/*                                                                         */
/* post     : Returns the number of bits written after an init_bitcount.   */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int get_in_bitcount(StrData* SD) {
	return SD->ByteCounter * 8 - SD->BitPosition;
}


