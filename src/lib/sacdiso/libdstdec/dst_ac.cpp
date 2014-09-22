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
