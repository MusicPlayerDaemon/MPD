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

#include "dst_memory.h"
#include "dst_ac.h"
#include "dst_unpack.h"
#include "dst_fram.h"

#define PBITS   AC_BITS             /* number of bits for Probabilities             */
#define NBITS   4                   /* number of overhead bits: must be at least 2! */
                                    /* maximum "variable shift length" is (NBITS-1) */
#define PSUM    (1 << (PBITS))
#define ABITS   (PBITS + NBITS)     /* must be at least PBITS+2     */
#define MB      0                   /* if (MB) print max buffer use */
#define ONE     (1 << ABITS)
#define HALF    (1 << (ABITS - 1))

__FUNCTION_ATTRIBUTES__ void LT_ACDecodeBit_Init(ACData* AC, ADataByte* cb, int fs) {
	AC->Init = 0;
	AC->A = ONE - 1;
	AC->C = 0;
	for (AC->cbptr = 1; AC->cbptr <= ABITS; AC->cbptr++) {
		AC->C <<= 1;
		if (AC->cbptr < fs) {
			AC->C |= GET_BIT(cb, AC->cbptr);
		}
	}
}

__FUNCTION_ATTRIBUTES__ void LT_ACDecodeBit_Decode(ACData* AC, uint8_t* b, int p, ADataByte* cb, int fs) {
	unsigned int ap;
	unsigned int h;
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
			AC->C |= GET_BIT(cb, AC->cbptr);
		}
		AC->cbptr++;
	}
}

__FUNCTION_ATTRIBUTES__ void LT_ACDecodeBit_Flush(ACData* AC, uint8_t* b, int p, ADataByte* cb, int fs) {
	AC->Init = 1;
	if (AC->cbptr < fs - 7) {
		*b = 0;
	}
	else {
		*b = 1;
		while ((AC->cbptr < fs) && (*b == 1)) {
			if (GET_BIT(cb, AC->cbptr) != 0) {
				*b = 1;
			}
			AC->cbptr++;
		}
	}
}

__FUNCTION_ATTRIBUTES__ int LT_ACGetPtableIndex(int16_t PredicVal, int PtableLen) {
  int  j;
  j = (PredicVal > 0 ? PredicVal : -PredicVal) >> AC_QSTEP;
  if (j >= PtableLen) {
    j = PtableLen - 1;
  }
  return j;
}

/***************************************************************************/
/*                                                                         */
/* name     : FillTable4Bit                                                */
/*                                                                         */
/* function : Fill an array that indicates for each bit of each channel    */
/*            which table number must be used.                             */
/*                                                                         */
/* pre      : NrOfChannels, NrOfBitsPerCh, S->NrOfSegments[],              */
/*            S->SegmentLen[][], S->Resolution, S->Table4Segment[][]       */
/*                                                                         */
/* post     : Table4Bit[][]                                                */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ void FillTable4Bit(int NrOfChannels, int NrOfBitsPerCh, Segment* S, uint8_t Table4Bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME / 2]) {
	int BitNr;
	int ChNr;
	int SegNr;
	int Start;
	int End;
	int8_t Val;
	for (ChNr = 0; ChNr < NrOfChannels; ChNr++) {
		for (SegNr = 0, Start = 0; SegNr < S->NrOfSegments[ChNr] - 1; SegNr++) {
			Val = (int8_t)S->Table4Segment[ChNr][SegNr];
			End = Start + S->Resolution * 8 * S->SegmentLen[ChNr][SegNr];
			for (BitNr = Start; BitNr < End; BitNr++) {
				uint8_t* p = (uint8_t*)&Table4Bit[ChNr][BitNr / 2];
				int s = (BitNr & 1) << 2;
				*p = ((uint8_t)Val << s) | (*p & (0xf0 >> s));
			}
			Start += S->Resolution * 8 * S->SegmentLen[ChNr][SegNr];
		}
		Val = (int8_t)S->Table4Segment[ChNr][SegNr];
		for (BitNr = Start; BitNr < NrOfBitsPerCh; BitNr++) {
			uint8_t* p = (uint8_t*)&Table4Bit[ChNr][BitNr / 2];
			int s = (BitNr & 1) << 2;
			*p = ((uint8_t)Val << s) | (*p & (0xf0 >> s));
		}
	}
}

/***************************************************************************/
/*                                                                         */
/* name     : Reverse7LSBs                                                 */
/*                                                                         */
/* function : Take the 7 LSBs of a number consisting of SIZE_PREDCOEF bits */
/*            (2's complement), reverse the bit order and add 1 to it.     */
/*                                                                         */
/* pre      : c                                                            */
/*                                                                         */
/* post     : Returns the translated number                                */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ static int16_t Reverse7LSBs(int16_t c) {
	const int16_t reverse[128] = {
		1, 65, 33, 97, 17, 81, 49, 113, 9, 73, 41, 105, 25, 89, 57, 121,
		5, 69, 37, 101, 21, 85, 53, 117, 13, 77, 45, 109, 29, 93, 61, 125,
		3, 67, 35, 99, 19, 83, 51, 115, 11, 75, 43, 107, 27, 91, 59, 123,
		7, 71, 39, 103, 23, 87, 55, 119, 15, 79, 47, 111, 31, 95, 63, 127,
		2, 66, 34, 98, 18, 82, 50, 114, 10, 74, 42, 106, 26, 90, 58, 122,
		6, 70, 38, 102, 22, 86, 54, 118, 14, 78, 46, 110, 30, 94, 62, 126,
		4, 68, 36, 100, 20, 84, 52, 116, 12, 76, 44, 108, 28, 92, 60, 124,
		8, 72, 40, 104, 24, 88, 56, 120, 16, 80, 48, 112, 32, 96, 64, 128 };
	return reverse[(c + (1 << SIZE_PREDCOEF)) & 127];
}

__FUNCTION_ATTRIBUTES__ static void LT_InitCoefTablesI(DstDec*D, int16_t ICoefI[2 * MAX_CHANNELS][16][256]) {
  int FilterNr, FilterLength, TableNr, k, i, j;
  for (FilterNr = 0; FilterNr < D->FrameHdr.NrOfFilters; FilterNr++) {
    FilterLength = D->FrameHdr.PredOrder[FilterNr];
    for (TableNr = 0; TableNr < 16; TableNr++) {
      k = FilterLength - TableNr * 8;
      if (k > 8) {
        k = 8;
			}
			else if (k < 0) {
        k = 0;
      }
      for (i = 0; i < 256; i++) {
        int cvalue = 0;
        for (j = 0; j < k; j++) {
          cvalue += (((i >> j) & 1) * 2 - 1) * D->FrameHdr.ICoefA[FilterNr][TableNr * 8 + j];
        }
        ICoefI[FilterNr][TableNr][i] = (int16_t)cvalue;
      }
    }
  }
}

__FUNCTION_ATTRIBUTES__ static void LT_InitCoefTablesU(DstDec* D, uint16_t ICoefU[2 * MAX_CHANNELS][16][256]) {
	int FilterNr, FilterLength, TableNr, k, i, j;
	for (FilterNr = 0; FilterNr < D->FrameHdr.NrOfFilters; FilterNr++) {
		FilterLength = D->FrameHdr.PredOrder[FilterNr];
		for (TableNr = 0; TableNr < 16; TableNr++) {
			k = FilterLength - TableNr * 8;
			if (k > 8) {
				k = 8;
			}
			else if (k < 0) {
				k = 0;
			}
			for (i = 0; i < 256; i++) {
				int cvalue = 0;
				for (j = 0; j < k; j++) {
					cvalue += (int16_t)(((i >> j) & 1) * 2 - 1) * D->FrameHdr.ICoefA[FilterNr][TableNr * 8 + j];
				}
				ICoefU[FilterNr][TableNr][i] = (uint16_t)(cvalue + (1 << SIZE_PREDCOEF) * 8);
			}
		}
	}
}

__FUNCTION_ATTRIBUTES__ static void LT_InitStatus(DstDec* D, uint8_t Status[MAX_CHANNELS][16]) {
  int ChNr, TableNr;
  for (ChNr = 0; ChNr < D->FrameHdr.NrOfChannels; ChNr++) {
    for (TableNr = 0; TableNr < 16; TableNr++) {
      Status[ChNr][TableNr] = 0xaa;
    }
  }
}

__FUNCTION_ATTRIBUTES__ static int16_t LT_RunFilterI(int16_t FilterTable[16][256], uint8_t ChannelStatus[16]) {
  int Predict;
  Predict  = FilterTable[ 0][ChannelStatus[ 0]];
  Predict += FilterTable[ 1][ChannelStatus[ 1]];
  Predict += FilterTable[ 2][ChannelStatus[ 2]];
  Predict += FilterTable[ 3][ChannelStatus[ 3]];
  Predict += FilterTable[ 4][ChannelStatus[ 4]];
  Predict += FilterTable[ 5][ChannelStatus[ 5]];
  Predict += FilterTable[ 6][ChannelStatus[ 6]];
  Predict += FilterTable[ 7][ChannelStatus[ 7]];
  Predict += FilterTable[ 8][ChannelStatus[ 8]];
  Predict += FilterTable[ 9][ChannelStatus[ 9]];
  Predict += FilterTable[10][ChannelStatus[10]];
  Predict += FilterTable[11][ChannelStatus[11]];
  Predict += FilterTable[12][ChannelStatus[12]];
  Predict += FilterTable[13][ChannelStatus[13]];
  Predict += FilterTable[14][ChannelStatus[14]];
  Predict += FilterTable[15][ChannelStatus[15]];
  return (int16_t)Predict;
}

__FUNCTION_ATTRIBUTES__ static int16_t LT_RunFilterU(uint16_t FilterTable[16][256], uint8_t ChannelStatus[16]) {
  uint32_t Predict32;
  int      Predict;
  Predict32  = FilterTable[ 0][ChannelStatus[ 0]] | (FilterTable[ 1][ChannelStatus[ 1]] << 16);
  Predict32 += FilterTable[ 2][ChannelStatus[ 2]] | (FilterTable[ 3][ChannelStatus[ 3]] << 16);
  Predict32 += FilterTable[ 4][ChannelStatus[ 4]] | (FilterTable[ 5][ChannelStatus[ 5]] << 16);
  Predict32 += FilterTable[ 6][ChannelStatus[ 6]] | (FilterTable[ 7][ChannelStatus[ 7]] << 16);
  Predict32 += FilterTable[ 8][ChannelStatus[ 8]] | (FilterTable[ 9][ChannelStatus[ 9]] << 16);
  Predict32 += FilterTable[10][ChannelStatus[10]] | (FilterTable[11][ChannelStatus[11]] << 16);
  Predict32 += FilterTable[12][ChannelStatus[12]] | (FilterTable[13][ChannelStatus[13]] << 16);
  Predict32 += FilterTable[14][ChannelStatus[14]] | (FilterTable[15][ChannelStatus[15]] << 16);
  Predict = (Predict32 >> 16) + (Predict32 & 0xffff);
  return (int16_t)Predict;
}

/***************************************************************************/
/*                                                                         */
/* name     : DST_FramDSTDecode                                            */
/*                                                                         */
/* function : DST decode a complete frame (all channels)     .             */
/*                                                                         */
/* pre      : D->CodOpt  : .NrOfBitsPerCh, .NrOfChannels,                  */
/*            D->FrameHdr: .PredOrder[], .NrOfHalfBits[], .ICoefA[][],     */
/*                         .NrOfFilters, .NrOfPtables, .FrameNr            */
/*            D->P_one[][], D->AData[], D->ADataLen,                       */
/*                                                                         */
/* post     : D->WM.Pwm                                                    */
/*                                                                         */
/***************************************************************************/

#define LT_RUN_FILTER_I(FilterTable, ChannelStatus) \
    Predict  = FilterTable[ 0][ChannelStatus[ 0]]; \
    Predict += FilterTable[ 1][ChannelStatus[ 1]]; \
    Predict += FilterTable[ 2][ChannelStatus[ 2]]; \
    Predict += FilterTable[ 3][ChannelStatus[ 3]]; \
    Predict += FilterTable[ 4][ChannelStatus[ 4]]; \
    Predict += FilterTable[ 5][ChannelStatus[ 5]]; \
    Predict += FilterTable[ 6][ChannelStatus[ 6]]; \
    Predict += FilterTable[ 7][ChannelStatus[ 7]]; \
    Predict += FilterTable[ 8][ChannelStatus[ 8]]; \
    Predict += FilterTable[ 9][ChannelStatus[ 9]]; \
    Predict += FilterTable[10][ChannelStatus[10]]; \
    Predict += FilterTable[11][ChannelStatus[11]]; \
    Predict += FilterTable[12][ChannelStatus[12]]; \
    Predict += FilterTable[13][ChannelStatus[13]]; \
    Predict += FilterTable[14][ChannelStatus[14]]; \
    Predict += FilterTable[15][ChannelStatus[15]];

#define LT_RUN_FILTER_U(FilterTable, ChannelStatus) \
    { \
        uint32_t Predict32; \
         \
        Predict32  = FilterTable[ 0][ChannelStatus[ 0]] | (FilterTable[ 1][ChannelStatus[ 1]] << 16); \
        Predict32 += FilterTable[ 2][ChannelStatus[ 2]] | (FilterTable[ 3][ChannelStatus[ 3]] << 16); \
        Predict32 += FilterTable[ 4][ChannelStatus[ 4]] | (FilterTable[ 5][ChannelStatus[ 5]] << 16); \
        Predict32 += FilterTable[ 6][ChannelStatus[ 6]] | (FilterTable[ 7][ChannelStatus[ 7]] << 16); \
        Predict32 += FilterTable[ 8][ChannelStatus[ 8]] | (FilterTable[ 9][ChannelStatus[ 9]] << 16); \
        Predict32 += FilterTable[10][ChannelStatus[10]] | (FilterTable[11][ChannelStatus[11]] << 16); \
        Predict32 += FilterTable[12][ChannelStatus[12]] | (FilterTable[13][ChannelStatus[13]] << 16); \
        Predict32 += FilterTable[14][ChannelStatus[14]] | (FilterTable[15][ChannelStatus[15]] << 16); \
        Predict = (Predict32 >> 16) + (Predict32 & 0xffff); \
    }

__FUNCTION_ATTRIBUTES__ int DST_FramDSTDecode(DstDec* D, uint8_t* DSTdata, uint8_t* MuxedDSDdata, int FrameSizeInBytes, int FrameCnt) {
	int       retval = 0;
	int       ChNr;
	int       BitNr;
	uint8_t   ACError;
	const int NrOfBitsPerCh = D->FrameHdr.NrOfBitsPerCh;
	const int NrOfChannels = D->FrameHdr.NrOfChannels;
	uint8_t*  MuxedDSD = MuxedDSDdata;

	D->FrameHdr.FrameNr       = FrameCnt;
	D->FrameHdr.CalcNrOfBytes = FrameSizeInBytes;
	D->FrameHdr.CalcNrOfBits  = D->FrameHdr.CalcNrOfBytes * 8;

	/* unpack DST frame: segmentation, mapping, arithmatic data */
	retval = UnpackDSTframe(D, DSTdata, MuxedDSDdata);
	if (retval == -1) {
		return -1;
	}

	if (D->FrameHdr.DSTCoded == 1) {
		ACData AC;
		int16_t  LT_ICoefI[2 * MAX_CHANNELS][16][256];
		//uint16_t LT_ICoefU[2 * MAX_CHANNELS][16][256];
		uint8_t  LT_Status[MAX_CHANNELS][16];

		FillTable4Bit(NrOfChannels, NrOfBitsPerCh, &D->FrameHdr.FSeg, D->FrameHdr.Filter4Bit);
		FillTable4Bit(NrOfChannels, NrOfBitsPerCh, &D->FrameHdr.PSeg, D->FrameHdr.Ptable4Bit);

		LT_InitCoefTablesI(D, LT_ICoefI);
		//LT_InitCoefTablesU(D, LT_ICoefU);
		LT_InitStatus(D, LT_Status);

		LT_ACDecodeBit_Init(&AC, D->AData, D->ADataLen);
		LT_ACDecodeBit_Decode(&AC, &ACError, Reverse7LSBs(D->FrameHdr.ICoefA[0][0]), D->AData, D->ADataLen);

		dst_memset(MuxedDSD, 0, (NrOfBitsPerCh * NrOfChannels + 7) / 8); 
		for (BitNr = 0; BitNr < NrOfBitsPerCh; BitNr++) {
			for (ChNr = 0; ChNr < NrOfChannels; ChNr++) {
				int16_t Predict;
				uint8_t Residual;
				int16_t BitVal;
				const int FilterNr = GET_NIBBLE(D->FrameHdr.Filter4Bit[ChNr], BitNr);

				/* Calculate output value of the FIR filter */
				LT_RUN_FILTER_I(LT_ICoefI[FilterNr], LT_Status[ChNr]);
				//LT_RUN_FILTER_U(LT_ICoefU[FilterNr], LT_Status[ChNr]);
				//Predict = LT_RunFilterI(LT_ICoefI[FilterNr], LT_Status[ChNr]);
				//Predict = LT_RunFilterU(LT_ICoefU[FilterNr], LT_Status[ChNr]);

				/* Arithmetic decode the incoming bit */
				if ((D->FrameHdr.HalfProb[ChNr]/* == 1*/) && (BitNr < D->FrameHdr.NrOfHalfBits[ChNr])) {
					LT_ACDecodeBit_Decode(&AC, &Residual, AC_PROBS / 2, D->AData, D->ADataLen);
				}
				else {
					const int PtableNr = GET_NIBBLE(D->FrameHdr.Ptable4Bit[ChNr], BitNr);
					const int PtableIndex = LT_ACGetPtableIndex(Predict, D->FrameHdr.PtableLen[PtableNr]);
					LT_ACDecodeBit_Decode(&AC, &Residual, D->P_one[PtableNr][PtableIndex], D->AData, D->ADataLen);
				}

				/* Channel bit depends on the predicted bit and BitResidual[][] */
				BitVal = ((((uint16_t)Predict) >> 15) ^ Residual) & 1;

				/* Shift the result into the correct bit position */ \
				MuxedDSD[(BitNr >> 3) * NrOfChannels + ChNr] |= (uint8_t)(BitVal << (7 - (BitNr & 7)));

				/* Update filter */
				{
					uint32_t* const st = (uint32_t*)LT_Status[ChNr];
					st[3] = (st[3] << 1) | ((st[2] >> 31) & 1);
					st[2] = (st[2] << 1) | ((st[1] >> 31) & 1);
					st[1] = (st[1] << 1) | ((st[0] >> 31) & 1);
					st[0] = (st[0] << 1) | BitVal;
				}
			}
		}

		/* Flush the arithmetic decoder */
		LT_ACDecodeBit_Flush(&AC, &ACError, 0, D->AData, D->ADataLen);

		if (ACError != 1) {
			trc_printf("ERROR: Arithmetic decoding error!\n");
			retval = -1;
		}
	}

	return retval;
}
