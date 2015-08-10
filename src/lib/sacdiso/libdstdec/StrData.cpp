/*

MPEG-4 Audio RM Module
Lossless coding of 1-bit oversampled audio - DST (Direct Stream Transfer)

This software was originally developed by:

* Aad Rijnberg
Philips Digital Systems Laboratories Eindhoven
<aad.rijnberg@philips.com>

* Fons Bruekers
Philips Research Laboratories Eindhoven
<fons.bruekers@philips.com>

* Eric Knapen
Philips Digital Systems Laboratories Eindhoven
<h.w.m.knapen@philips.com>

And edited by:

* Richard Theelen
Philips Digital Systems Laboratories Eindhoven
<r.h.m.theelen@philips.com>

* Maxim V.Anisiutkin
<maxim.anisiutkin@gmail.com>

in the course of development of the MPEG-4 Audio standard ISO-14496-1, 2 and 3.
This software module is an implementation of a part of one or more MPEG-4 Audio
tools as specified by the MPEG-4 Audio standard. ISO/IEC gives users of the
MPEG-4 Audio standards free licence to this software module or modifications
thereof for use in hardware or software products claiming conformance to the
MPEG-4 Audio standards. Those intending to use this software module in hardware
or software products are advised that this use may infringe existing patents.
The original developers of this software of this module and their company,
the subsequent editors and their companies, and ISO/EIC have no liability for
use of this software module or modifications thereof in an implementation.
Copyright is not released for non MPEG-4 Audio conforming products. The
original developer retains full right to use this code for his/her own purpose,
assign or donate the code to a third party and to inhibit third party from
using the code for non MPEG-4 Audio conforming products. This copyright notice
must be included in all copies of derivative works.

Copyright © 2004.

*/

#include "StrData.h"

void CStrData::getDSTDataPointer(uint8_t** pBuffer) {
	*pBuffer = DSTdata;
}

void CStrData::resetReadingIndex() {
	BitPosition = 0;
	ByteCounter = 0;
	DataByte = 0;
}

void CStrData::createBuffer(int size) {
	if (size > (int)sizeof(DSTdata)) {
		TotalBytes = (int)sizeof(DSTdata);
	}
	else {
		TotalBytes = size;
	}
}

void CStrData::deleteBuffer(){ 
	TotalBytes = 0;
	resetReadingIndex();
}

void CStrData::fillBuffer(uint8_t* pBuf, int size) {
	createBuffer(size);
	dst_memcpy(DSTdata, pBuf, TotalBytes);
	resetReadingIndex();
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

void CStrData::getChrUnsigned(int length, uint8_t& x) {
	long tmp;
	if (length > 0) {
		getbits(tmp, length);
		x = (unsigned char)tmp;
	}
	else if (length == 0) {
		x = 0;
	}
	else {
		log_printf("ERROR: a negative number of bits allocated");
	}
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

void CStrData::getIntUnsigned(int length, int& x) {
	long tmp;
	if (length > 0) {
		getbits(tmp, length);
		x = (int)tmp;
	}
	else if (length == 0) {
		x = 0;
	}
	else {
		log_printf("ERROR: a negative number of bits allocated");
	}
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

void CStrData::getIntSigned(int length, int& x) {
	long tmp;
	if (length > 0) {
		getbits(tmp, length);
		x = (int)tmp;
		if (x >= (1 << (length - 1))) {
			x -= (1 << length);
		}
	}
	else if (length == 0) {
		x = 0;
	}
	else {
		log_printf("ERROR: a negative number of bits allocated");
	}
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

void CStrData::getShortSigned(int length, short& x) {
	long tmp;
	if (length > 0) {
		getbits(tmp, length);
		x = (short)tmp;
		if (x >= (1 << (length - 1))) {
			x -= (1 << length);
		}
	}
	else if (length == 0) {
		x = 0;
	}
	else {
		log_printf("ERROR: a negative number of bits allocated");
	}
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

int CStrData::get_in_bitcount() {
	return ByteCounter * 8 - BitPosition;
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

int CStrData::getbits(long& outword, int out_bitptr) {
	const int masks[] = { 0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff };
	if (out_bitptr == 1) {
		if (BitPosition == 0) {
			DataByte = DSTdata[ByteCounter++];
			if (ByteCounter > TotalBytes) {
				return -1; /* EOF */
			}
			BitPosition = 8;
		}
		BitPosition--;
		outword = (DataByte >> BitPosition) & 1;
		return 0;
	}
	outword = 0;
	while (out_bitptr > 0) {
		int thisbits, mask, shift;
		if (!BitPosition) {
			DataByte = DSTdata[ByteCounter++];
			if (ByteCounter > TotalBytes) {
				return -1; /* EOF */
			}
			BitPosition = 8;
		}
		thisbits = MIN(BitPosition, out_bitptr);
		shift = (BitPosition - thisbits);
		mask = masks[thisbits] << shift;
		shift = (out_bitptr - thisbits) - shift;
		if (shift <= 0)
			outword |= ((DataByte & mask) >> -shift);
		else
			outword |= ((DataByte & mask) << shift);
		out_bitptr -= thisbits;
		BitPosition -= thisbits;
	}
	return 0;
}
