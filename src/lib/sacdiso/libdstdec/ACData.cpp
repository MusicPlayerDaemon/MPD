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

#include <stdio.h>
#include <stdlib.h>
#include "ACData.h"

#define PBITS   AC_BITS          /* number of bits for Probabilities             */
#define NBITS   4                /* number of overhead bits: must be at least 2! */
                                 /* maximum "variable shift length" is (NBITS-1) */
#define PSUM    (1 << (PBITS))
#define ABITS   (PBITS + NBITS)  /* must be at least PBITS+2     */
#define MB      0                /* if (MB) print max buffer use */
#define ONE     (1 << ABITS)
#define HALF    (1 << (ABITS - 1))

int CACData::getPtableIndex(long PredicVal, int PtableLen) {
	int j;
	j = (PredicVal > 0 ? PredicVal : -PredicVal) >> AC_QSTEP;
	if (j >= PtableLen) {
		j = PtableLen - 1;
	}
	return j;
}

void CACData::decodeBit(uint8_t& b, int p, uint8_t* cb, int fs, int flush) {
	unsigned int ap;
	unsigned int h;
	if (Init == 1) {
		Init = 0;
		A = ONE - 1;
		C = 0;
		for (cbptr = 1; cbptr <= ABITS; cbptr++) {
			C <<= 1;
			if (cbptr < fs) {
				C |= cb[cbptr];
			}
		}
	}
	if (flush == 0) {
		/* approximate (A * p) with "partial rounding". */
		ap = ((A >> PBITS) | ((A >> (PBITS - 1)) & 1)) * p;
		h = A - ap;
		if (C >= h) {
			b = 0;
			C -= h;
			A = ap;
		}
		else {
			b = 1;
			A = h;
		}
		while (A < HALF) {
			A <<= 1;
			/* Use new flushing technique; insert zero in LSB of C if reading past the end of the arithmetic code */
			C <<= 1;
			if (cbptr < fs) {
				C |= cb[cbptr];
			}
			cbptr++;
		}
	}
	else {
		Init = 1;
		if (cbptr < fs - 7) {
			b = 0;
		}
		else {
			b = 1;
			while ((cbptr < fs) && (b == 1)) {
				if (cb[cbptr] != 0) {
					b = 1;
				}
				cbptr++;
			}
		}
	}
}

void CACData::decodeBit_Init(ADataByte* cb, int fs) {
	Init = 0;
	A = ONE - 1;
	C = 0;
	for (cbptr = 1; cbptr <= ABITS; cbptr++) {
		C <<= 1;
		if (cbptr < fs) {
			C |= GET_BIT(cb, cbptr);
		}
	}
}

void CACData::decodeBit_Decode(uint8_t* b, int p, ADataByte* cb, int fs) {
	unsigned int ap;
	unsigned int h;
	/* approximate (A * p) with "partial rounding". */
	ap = ((A >> PBITS) | ((A >> (PBITS - 1)) & 1)) * p;
	h = A - ap;
	if (C >= h) {
		*b = 0;
		C -= h;
		A = ap;
	}
	else {
		*b = 1;
		A = h;
	}
	while (A < HALF) {
		A <<= 1;
		/* Use new flushing technique; insert zero in LSB of C if reading past the end of the arithmetic code */
		C <<= 1;
		if (cbptr < fs) {
			C |= GET_BIT(cb, cbptr);
		}
		cbptr++;
	}
}

void CACData::decodeBit_Flush(uint8_t* b, int p, ADataByte* cb, int fs) {
	(void)p;
	Init = 1;
	if (cbptr < fs - 7) {
		*b = 0;
	}
	else {
		*b = 1;
		while ((cbptr < fs) && (*b == 1)) {
			if (GET_BIT(cb, cbptr) != 0) {
				*b = 1;
			}
			cbptr++;
		}
	}
}
