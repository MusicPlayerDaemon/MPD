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

#include "dst_ac.h"

#define PBITS   AC_BITS          /* number of bits for Probabilities             */
#define NBITS   4                /* number of overhead bits: must be at least 2! */
                                 /* maximum "variable shift length" is (NBITS-1) */
#define PSUM    (1 << (PBITS))
#define ABITS   (PBITS + NBITS)  /* must be at least PBITS+2     */
#define MB      0                /* if (MB) print max buffer use */
#define ONE     (1 << ABITS)
#define HALF    (1 << (ABITS-1))

__FUNCTION_ATTRIBUTES__ void DST_ACDecodeBit(ACData* AC, uint8_t* b, int p, uint8_t* cb, int fs, int flush) {
	unsigned int ap;
	unsigned int h;
	if (AC->Init == 1) {
		AC->Init = 0;
		AC->A    = ONE - 1;
		AC->C    = 0;
		for (AC->cbptr = 1; AC->cbptr <= ABITS; AC->cbptr++) {
			AC->C <<= 1;
			if (AC->cbptr < fs) {
				AC->C |= cb[AC->cbptr];
			}
		}
	}
	if (flush == 0) {
		/* approximate (A * p) with "partial rounding". */
		ap = ((AC->A >> PBITS) | ((AC->A >> (PBITS - 1)) & 1)) * p;
		h = AC->A - ap;
		if (AC->C >= h) {
			*b = 0;
			AC->C -= h;
			AC->A  = ap;
		}
		else {
			*b = 1;
			AC->A  = h;
		}
		while (AC->A < HALF) {
			AC->A <<= 1;
			/* Use new flushing technique; insert zero in LSB of C if reading past the end of the arithmetic code */
			AC->C <<= 1;
			if (AC->cbptr < fs) {
				AC->C |= cb[AC->cbptr];
			}
			AC->cbptr++;
		}
	}
	else {
		AC->Init = 1;
		if (AC->cbptr < fs - 7) {
			*b = 0;
		}
		else {
			*b = 1;
			while ((AC->cbptr < fs) && (*b == 1)) {
				if (cb[AC->cbptr] != 0) {
					*b = 1;
				}
				AC->cbptr++;
			}
		}
	}
}

__FUNCTION_ATTRIBUTES__ int DST_ACGetPtableIndex(long PredicVal, int PtableLen) {
	int  j;
	j = labs(PredicVal) >> AC_QSTEP;
	if (j >= PtableLen) {
		j = PtableLen - 1;
	}
	return j;
}
