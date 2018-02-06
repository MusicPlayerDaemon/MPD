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

#include <memory.h>

#include "ACData.h"
#include "FrameReader.h"
#include "DSTDecoder.h"

CDSTDecoder::CDSTDecoder() {
	::memset(this, 0, sizeof(*this));
}

CDSTDecoder::~CDSTDecoder() {
}

int CDSTDecoder::init(int channels, int fs44) {
	FrameHdr.NrOfChannels = channels;
	FrameHdr.MaxFrameLen = (588 * fs44 / 8);
	FrameHdr.ByteStreamLen = FrameHdr.MaxFrameLen * FrameHdr.NrOfChannels;
	FrameHdr.BitStreamLen = FrameHdr.ByteStreamLen * 8;
	FrameHdr.NrOfBitsPerCh = FrameHdr.MaxFrameLen * 8;
	FrameHdr.MaxNrOfFilters = 2 * FrameHdr.NrOfChannels;
	FrameHdr.MaxNrOfPtables = 2 * FrameHdr.NrOfChannels;

	FrameHdr.FrameNr = 0;
	StrFilter.TableType = T_FILTER;
	StrFilter.calcCCP();
	StrPtable.TableType = T_PTABLE;
	StrPtable.calcCCP();

	return 0;
}

int CDSTDecoder::close() {
	return 0;
}

/* DST decode a complete frame (all channels) */

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

int CDSTDecoder::decode(uint8_t* DSTFrame, int frameSize, uint8_t* DSDFrame) {
	int      rv = 0;
	int      ChNr;
	int      BitNr;
	uint8_t  ACError;
	int      NrOfBitsPerCh = FrameHdr.NrOfBitsPerCh;
	int      NrOfChannels = FrameHdr.NrOfChannels;

	FrameHdr.FrameNr++;
	FrameHdr.CalcNrOfBytes = frameSize / 8;
	FrameHdr.CalcNrOfBits = FrameHdr.CalcNrOfBytes * 8;

	/* unpack DST frame: segmentation, mapping, arithmetic data */
	rv = unpack(DSTFrame, DSDFrame);
	if (rv == -1) {
		return -1;
	}

	if (FrameHdr.DSTCoded == 1) {
		CACData AC;
		int16_t  LT_ICoefI[2 * MAX_CHANNELS][16][256];
		//uint16_t LT_ICoefU[2 * MAX_CHANNELS][16][256];
		uint8_t  LT_Status[MAX_CHANNELS][16];

		fillTable4Bit(FrameHdr.FSeg, FrameHdr.Filter4Bit);
		fillTable4Bit(FrameHdr.PSeg, FrameHdr.Ptable4Bit);

		LT_InitCoefTablesI(LT_ICoefI);
		//LT_InitCoefTablesU(LT_ICoefU);
		LT_InitStatus(LT_Status);

		AC.decodeBit_Init(AData, ADataLen);
		AC.decodeBit_Decode(&ACError, reverse7LSBs(FrameHdr.ICoefA[0][0]), AData, ADataLen);

		dst_memset(DSDFrame, 0, (NrOfBitsPerCh * NrOfChannels + 7) / 8);
		for (BitNr = 0; BitNr < NrOfBitsPerCh; BitNr++) {
			for (ChNr = 0; ChNr < NrOfChannels; ChNr++) {
				int16_t Predict;
				uint8_t Residual;
				int16_t BitVal;
				const int FilterNr = GET_NIBBLE(FrameHdr.Filter4Bit[ChNr], BitNr);

				/* Calculate output value of the FIR filter */
				LT_RUN_FILTER_I(LT_ICoefI[FilterNr], LT_Status[ChNr]);
				//LT_RUN_FILTER_U(LT_ICoefU[FilterNr], LT_Status[ChNr]);
				//Predict = LT_RunFilterI(LT_ICoefI[FilterNr], LT_Status[ChNr]);
				//Predict = LT_RunFilterU(LT_ICoefU[FilterNr], LT_Status[ChNr]);

				/* Arithmetic decode the incoming bit */
				if ((FrameHdr.HalfProb[ChNr]/* == 1*/) && (BitNr < FrameHdr.NrOfHalfBits[ChNr])) {
					AC.decodeBit_Decode(&Residual, AC_PROBS / 2, AData, ADataLen);
				}
				else {
					int PtableNr = GET_NIBBLE(FrameHdr.Ptable4Bit[ChNr], BitNr);
					int PtableIndex = AC.getPtableIndex(Predict, FrameHdr.PtableLen[PtableNr]);
					AC.decodeBit_Decode(&Residual, P_one[PtableNr][PtableIndex], AData, ADataLen);
				}

				/* Channel bit depends on the predicted bit and BitResidual[][] */
				BitVal = ((((uint16_t)Predict) >> 15) ^ Residual) & 1;

				/* Shift the result into the correct bit position */ \
				DSDFrame[(BitNr >> 3) * NrOfChannels + ChNr] |= (uint8_t)(BitVal << (7 - (BitNr & 7)));

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
		AC.decodeBit_Flush(&ACError, 0, AData, ADataLen);

		if (ACError != 1) {
			log_printf("ERROR: Arithmetic decoding error!");
			rv = -1;
		}
	}

	return rv;
}

/* Read a complete frame from the DST input stream */

int CDSTDecoder::unpack(uint8_t* DSTFrame, uint8_t* DSDFrame) {
	int Dummy;
	int Ready = 0;

	/* fill internal buffer with DSTframe */
	SD.fillBuffer(DSTFrame, FrameHdr.CalcNrOfBytes);

	/* interpret DST header byte */
	SD.getIntUnsigned(1, FrameHdr.DSTCoded);
	if (FrameHdr.DSTCoded == 0)	{
		SD.getIntUnsigned(1, Dummy);	/* Was &D->DstXbits.Bit, but it was never used */
		SD.getIntUnsigned(6, Dummy);
		if (Dummy != 0) {
			log_printf("ERROR: Illegal stuffing pattern in frame %d!\n", FrameHdr.FrameNr);
			return -1;
		}

		/* Read DSD data and put in output stream */
		CFrameReader::readDSDFrame(SD, FrameHdr.MaxFrameLen, FrameHdr.NrOfChannels, DSDFrame);
	}
	else {
		CFrameReader::readSegmentData(SD, FrameHdr);
		CFrameReader::readMappingData(SD, FrameHdr);
		CFrameReader::readFilterCoefSets(SD, FrameHdr.NrOfChannels, FrameHdr, StrFilter);
		CFrameReader::readProbabilityTables(SD, FrameHdr, StrPtable, P_one);
		ADataLen = FrameHdr.CalcNrOfBits - SD.get_in_bitcount();
		CFrameReader::readArithmeticCodedData(SD, ADataLen, AData);
		if (ADataLen > 0 && GET_BIT(AData, 0) != 0) {
			log_printf("ERROR: Illegal arithmetic code in frame %d!", FrameHdr.FrameNr);
			return -1;
		}
	}
	return Ready;
}

/* Take the 7 LSBs of a number consisting of SIZE_PREDCOEF bits */
/* (2's complement), reverse the bit order and add 1 to it.     */

int16_t CDSTDecoder::reverse7LSBs(int16_t c) {
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

/* Fill an array that indicates for each bit of each channel which table number must be used */

void CDSTDecoder::fillTable4Bit(CSegment& S, uint8_t Table4Bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME / 2]) {
	int SegNr;
	int Start;
	int End;
	int8_t Val;
	for (int ChNr = 0; ChNr < FrameHdr.NrOfChannels; ChNr++) {
		for (SegNr = 0, Start = 0; SegNr < S.NrOfSegments[ChNr] - 1; SegNr++) {
			Val = (int8_t)S.Table4Segment[ChNr][SegNr];
			End = Start + S.Resolution * 8 * S.SegmentLen[ChNr][SegNr];
			for (int BitNr = Start; BitNr < End; BitNr++) {
				uint8_t* p = (uint8_t*)&Table4Bit[ChNr][BitNr / 2];
				int s = (BitNr & 1) << 2;
				*p = ((uint8_t)Val << s) | (*p & (0xf0 >> s));
			}
			Start += S.Resolution * 8 * S.SegmentLen[ChNr][SegNr];
		}
		Val = (int8_t)S.Table4Segment[ChNr][SegNr];
		for (int BitNr = Start; BitNr < FrameHdr.NrOfBitsPerCh; BitNr++) {
			uint8_t* p = (uint8_t*)&Table4Bit[ChNr][BitNr / 2];
			int s = (BitNr & 1) << 2;
			*p = ((uint8_t)Val << s) | (*p & (0xf0 >> s));
		}
	}
}

void CDSTDecoder::LT_InitCoefTablesI(int16_t ICoefI[2 * MAX_CHANNELS][16][256]) {
	int FilterNr, FilterLength, TableNr, k, i, j;
	for (FilterNr = 0; FilterNr < FrameHdr.NrOfFilters; FilterNr++) {
		FilterLength = FrameHdr.PredOrder[FilterNr];
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
					cvalue += (((i >> j) & 1) * 2 - 1) * FrameHdr.ICoefA[FilterNr][TableNr * 8 + j];
				}
				ICoefI[FilterNr][TableNr][i] = (int16_t)cvalue;
			}
		}
	}
}

void CDSTDecoder::LT_InitCoefTablesU(uint16_t ICoefU[2 * MAX_CHANNELS][16][256]) {
	int FilterNr, FilterLength, TableNr, k, i, j;
	for (FilterNr = 0; FilterNr < FrameHdr.NrOfFilters; FilterNr++) {
		FilterLength = FrameHdr.PredOrder[FilterNr];
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
					cvalue += (int16_t)(((i >> j) & 1) * 2 - 1) * FrameHdr.ICoefA[FilterNr][TableNr * 8 + j];
				}
				ICoefU[FilterNr][TableNr][i] = (uint16_t)(cvalue + (1 << SIZE_PREDCOEF) * 8);
			}
		}
	}
}

void CDSTDecoder::LT_InitStatus(uint8_t Status[MAX_CHANNELS][16]) {
	int ChNr, TableNr;
	for (ChNr = 0; ChNr < FrameHdr.NrOfChannels; ChNr++) {
		for (TableNr = 0; TableNr < 16; TableNr++) {
			Status[ChNr][TableNr] = 0xaa;
		}
	}
}

int16_t CDSTDecoder::LT_RunFilterI(int16_t FilterTable[16][256], uint8_t ChannelStatus[16]) {
	int Predict;
	Predict = FilterTable[0][ChannelStatus[0]];
	Predict += FilterTable[1][ChannelStatus[1]];
	Predict += FilterTable[2][ChannelStatus[2]];
	Predict += FilterTable[3][ChannelStatus[3]];
	Predict += FilterTable[4][ChannelStatus[4]];
	Predict += FilterTable[5][ChannelStatus[5]];
	Predict += FilterTable[6][ChannelStatus[6]];
	Predict += FilterTable[7][ChannelStatus[7]];
	Predict += FilterTable[8][ChannelStatus[8]];
	Predict += FilterTable[9][ChannelStatus[9]];
	Predict += FilterTable[10][ChannelStatus[10]];
	Predict += FilterTable[11][ChannelStatus[11]];
	Predict += FilterTable[12][ChannelStatus[12]];
	Predict += FilterTable[13][ChannelStatus[13]];
	Predict += FilterTable[14][ChannelStatus[14]];
	Predict += FilterTable[15][ChannelStatus[15]];
	return (int16_t)Predict;
}

int16_t CDSTDecoder::LT_RunFilterU(uint16_t FilterTable[16][256], uint8_t ChannelStatus[16]) {
	uint32_t Predict32;
	int      Predict;
	Predict32 = FilterTable[0][ChannelStatus[0]] | (FilterTable[1][ChannelStatus[1]] << 16);
	Predict32 += FilterTable[2][ChannelStatus[2]] | (FilterTable[3][ChannelStatus[3]] << 16);
	Predict32 += FilterTable[4][ChannelStatus[4]] | (FilterTable[5][ChannelStatus[5]] << 16);
	Predict32 += FilterTable[6][ChannelStatus[6]] | (FilterTable[7][ChannelStatus[7]] << 16);
	Predict32 += FilterTable[8][ChannelStatus[8]] | (FilterTable[9][ChannelStatus[9]] << 16);
	Predict32 += FilterTable[10][ChannelStatus[10]] | (FilterTable[11][ChannelStatus[11]] << 16);
	Predict32 += FilterTable[12][ChannelStatus[12]] | (FilterTable[13][ChannelStatus[13]] << 16);
	Predict32 += FilterTable[14][ChannelStatus[14]] | (FilterTable[15][ChannelStatus[15]] << 16);
	Predict = (Predict32 >> 16) + (Predict32 & 0xffff);
	return (int16_t)Predict;
}
