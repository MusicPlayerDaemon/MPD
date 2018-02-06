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

#include "FrameReader.h"

/* Calculate the log2 of an integer and round the result up by using integer arithmetic */

int CFrameReader::log2RoundUp(long x) {
	int y = 0;
	while (x >= (1 << y)) {
		y++;
	}
	return y;
}
/* Read a Rice code from the DST file */

int CFrameReader::RiceDecode(CStrData& SD, int m) {
	int LSBs;
	int Nr;
	int RLBit;
	int RunLength;
	int Sign;

	/* Retrieve run length code */
	RunLength = 0;
	do {
		SD.getIntUnsigned(1, RLBit);
		RunLength += (1 - RLBit);
	} while (!RLBit);
	/* Retrieve least significant bits */
	SD.getIntUnsigned(m, LSBs);
	Nr = (RunLength << m) + LSBs;
	/* Retrieve optional sign bit */
	if (Nr != 0) {
		SD.getIntUnsigned(1, Sign);
		if (Sign) {
			Nr = -Nr;
		}
	}
	return Nr;
}

/* Read DSD signal of this frame from the DST input file */

void CFrameReader::readDSDFrame(CStrData& SD, long MaxFrameLen, int NrOfChannels, uint8_t* DSDFrame) {
	int ByteMax = MaxFrameLen * NrOfChannels;
	for (int ByteNr = 0; ByteNr < ByteMax; ByteNr++) {
		SD.getChrUnsigned(8, DSDFrame[ByteNr]);
	}
}

/* Read segmentation data for filters or Ptables */

void CFrameReader::readTableSegmentData(CStrData& SD, int NrOfChannels, int FrameLen, int MaxNrOfSegs, int MinSegLen, CSegment& S, int& SameSegAllCh) {
	int ChNr = 0;
	int DefinedBits = 0;
	bool ResolRead = false;
	int SegNr = 0;
	int MaxSegSize;
	int NrOfBits;
	int EndOfChannel;

	MaxSegSize = FrameLen - MinSegLen / 8;
	SD.getIntUnsigned(1, SameSegAllCh);
	if (SameSegAllCh) {
		SD.getIntUnsigned(1, EndOfChannel);
		while (!EndOfChannel) {
			if (SegNr >= MaxNrOfSegs) {
				log_printf("ERROR: Too many segments for this channel!");
				return;
			}
			if (!ResolRead) {
				NrOfBits = log2RoundUp(FrameLen - MinSegLen / 8);
				SD.getIntUnsigned(NrOfBits, S.Resolution);
				if ((S.Resolution == 0) || (S.Resolution > FrameLen - MinSegLen / 8)) {
					log_printf("ERROR: Invalid segment resolution!");
					return;
				}
				ResolRead = true;
			}
			NrOfBits = log2RoundUp(MaxSegSize / S.Resolution);
			SD.getIntUnsigned(NrOfBits, S.SegmentLen[0][SegNr]);

			if ((S.Resolution * 8 * S.SegmentLen[0][SegNr] < MinSegLen) || (S.Resolution * 8 * S.SegmentLen[0][SegNr] > FrameLen * 8 - DefinedBits - MinSegLen)) {
				log_printf("ERROR: Invalid segment length!");
				return;
			}
			DefinedBits += S.Resolution * 8 * S.SegmentLen[0][SegNr];
			MaxSegSize -= S.Resolution * S.SegmentLen[0][SegNr];
			SegNr++;
			SD.getIntUnsigned(1, EndOfChannel);
		}
		S.NrOfSegments[0] = SegNr + 1;
		S.SegmentLen[0][SegNr] = 0;

		for (ChNr = 1; ChNr < NrOfChannels; ChNr++) {
			S.NrOfSegments[ChNr] = S.NrOfSegments[0];
			for (SegNr = 0; SegNr < S.NrOfSegments[0]; SegNr++) {
				S.SegmentLen[ChNr][SegNr] = S.SegmentLen[0][SegNr];
			}
		}
	}
	else {
		while (ChNr < NrOfChannels) {
			if (SegNr >= MaxNrOfSegs) {
				log_printf("ERROR: Too many segments for this channel!");
				return;
			}
			SD.getIntUnsigned(1, EndOfChannel);
			if (!EndOfChannel) {
				if (!ResolRead) {
					NrOfBits = log2RoundUp(FrameLen - MinSegLen / 8);
					SD.getIntUnsigned(NrOfBits, S.Resolution);
					if ((S.Resolution == 0) || (S.Resolution > FrameLen - MinSegLen / 8)) {
						log_printf("ERROR: Invalid segment resolution!");
						return;
					}
					ResolRead = true;
				}
				NrOfBits = log2RoundUp(MaxSegSize / S.Resolution);
				SD.getIntUnsigned(NrOfBits, S.SegmentLen[ChNr][SegNr]);

				if ((S.Resolution * 8 * S.SegmentLen[ChNr][SegNr] < MinSegLen) || (S.Resolution * 8 * S.SegmentLen[ChNr][SegNr] > FrameLen * 8 - DefinedBits - MinSegLen)) {
					log_printf("ERROR: Invalid segment length!");
					return;
				}
				DefinedBits += S.Resolution * 8 * S.SegmentLen[ChNr][SegNr];
				MaxSegSize -= S.Resolution * S.SegmentLen[ChNr][SegNr];
				SegNr++;
			}
			else {
				S.NrOfSegments[ChNr] = SegNr + 1;
				S.SegmentLen[ChNr][SegNr] = 0;
				SegNr = 0;
				DefinedBits = 0;
				MaxSegSize = FrameLen - MinSegLen / 8;
				ChNr++;
			}
		}
	}
	if (!ResolRead)	{
		S.Resolution = 1;
	}
}

/* Copy segmentation data for filters and Ptables */

void CFrameReader::copySegmentData(CFrameHeader& FH) {
	FH.PSeg.Resolution = FH.FSeg.Resolution;
	FH.PSameSegAllCh = 1;
	for (int ChNr = 0; ChNr < FH.NrOfChannels; ChNr++) {
		FH.PSeg.NrOfSegments[ChNr] = FH.FSeg.NrOfSegments[ChNr];
		if (FH.PSeg.NrOfSegments[ChNr] > MAXNROF_PSEGS) {
			log_printf("ERROR: Too many segments!");
			return;
		}
		if (FH.PSeg.NrOfSegments[ChNr] != FH.PSeg.NrOfSegments[0]) {
			FH.PSameSegAllCh = 0;
		}
		for (int SegNr = 0; SegNr < FH.FSeg.NrOfSegments[ChNr]; SegNr++) {
			FH.PSeg.SegmentLen[ChNr][SegNr] = FH.FSeg.SegmentLen[ChNr][SegNr];
			if ((FH.PSeg.SegmentLen[ChNr][SegNr] != 0) &&	(FH.PSeg.Resolution * 8 * FH.PSeg.SegmentLen[ChNr][SegNr] < MIN_PSEG_LEN))	{
				log_printf("ERROR: Invalid segment length!");
				return;
			}
			if (FH.PSeg.SegmentLen[ChNr][SegNr] != FH.PSeg.SegmentLen[0][SegNr]) {
				FH.PSameSegAllCh = 0;
			}
		}
	}
}

/* Read segmentation data for filters and Ptables */

void CFrameReader::readSegmentData(CStrData& SD, CFrameHeader& FH) {
	SD.getIntUnsigned(1, FH.PSameSegAsF);
	readTableSegmentData(SD, FH.NrOfChannels, FH.MaxFrameLen, MAXNROF_FSEGS, MIN_FSEG_LEN, FH.FSeg, FH.FSameSegAllCh);
	if (FH.PSameSegAsF == 1) {
		copySegmentData(FH);
	}
	else {
		readTableSegmentData(SD, FH.NrOfChannels, FH.MaxFrameLen, MAXNROF_PSEGS, MIN_PSEG_LEN, FH.PSeg, FH.PSameSegAllCh);
	}
}

/* Read mapping data for filters or Ptables */

void CFrameReader::readTableMappingData(CStrData& SD, int NrOfChannels, int MaxNrOfTables, CSegment& S, int& NrOfTables, int& SameMapAllCh) {
	int CountTables = 1;
	int NrOfBits = 1;

	S.Table4Segment[0][0] = 0;
	SD.getIntUnsigned(1, SameMapAllCh);
	if (SameMapAllCh) {
		for (int SegNr = 1; SegNr < S.NrOfSegments[0]; SegNr++)	{
			NrOfBits = log2RoundUp(CountTables);
			SD.getIntUnsigned(NrOfBits, S.Table4Segment[0][SegNr]);
			if (S.Table4Segment[0][SegNr] == CountTables)	{
				CountTables++;
			}
			else if (S.Table4Segment[0][SegNr] > CountTables) {
				log_printf("ERROR: Invalid table number for segment!");
				return;
			}
		}
		for (int ChNr = 1; ChNr < NrOfChannels; ChNr++) {
			if (S.NrOfSegments[ChNr] != S.NrOfSegments[0]) {
				log_printf("ERROR: Mapping can't be the same for all channels!");
				return;
			}
			for (int SegNr = 0; SegNr < S.NrOfSegments[0]; SegNr++) {
				S.Table4Segment[ChNr][SegNr] = S.Table4Segment[0][SegNr];
			}
		}
	}
	else {
		for (int ChNr = 0; ChNr < NrOfChannels; ChNr++) {
			for (int SegNr = 0; SegNr < S.NrOfSegments[ChNr]; SegNr++) {
				if ((ChNr != 0) || (SegNr != 0)) {
					NrOfBits = log2RoundUp(CountTables);
					SD.getIntUnsigned(NrOfBits, S.Table4Segment[ChNr][SegNr]);
					if (S.Table4Segment[ChNr][SegNr] == CountTables) {
						CountTables++;
					}
					else if (S.Table4Segment[ChNr][SegNr] > CountTables) {
						log_printf("ERROR: Invalid table number for segment!");
						return;
					}
				}
			}
		}
	}
	if (CountTables > MaxNrOfTables) {
		log_printf("ERROR: Too many tables for this frame!");
		return;
	}
	NrOfTables = CountTables;
}

/* Copy mapping data for Ptables from the filter mapping */

void CFrameReader::copyMappingData(CFrameHeader& FH) {
	FH.PSameMapAllCh = 1;
	for (int ChNr = 0; ChNr < FH.NrOfChannels; ChNr++) {
		if (FH.PSeg.NrOfSegments[ChNr] == FH.FSeg.NrOfSegments[ChNr]) {
			for (int SegNr = 0; SegNr < FH.FSeg.NrOfSegments[ChNr]; SegNr++) {
				FH.PSeg.Table4Segment[ChNr][SegNr] = FH.FSeg.Table4Segment[ChNr][SegNr];
				if (FH.PSeg.Table4Segment[ChNr][SegNr] != FH.PSeg.Table4Segment[0][SegNr]) {
					FH.PSameMapAllCh = 0;
				}
			}
		}
		else {
			log_printf("ERROR: Not the same number of segments for filters and Ptables!");
			return;
		}
	}
	FH.NrOfPtables = FH.NrOfFilters;
	if (FH.NrOfPtables > FH.MaxNrOfPtables) {
		log_printf("ERROR: Too many tables for this frame!");
		return;
	}
}

/* Read mapping data (which channel uses which filter/Ptable) */

void CFrameReader::readMappingData(CStrData& SD, CFrameHeader& FH) {
	SD.getIntUnsigned(1, FH.PSameMapAsF);
	readTableMappingData(SD, FH.NrOfChannels, FH.MaxNrOfFilters, FH.FSeg, FH.NrOfFilters, FH.FSameMapAllCh);
	if (FH.PSameMapAsF == 1) {
		copyMappingData(FH);
	}
	else {
		readTableMappingData(SD, FH.NrOfChannels, FH.MaxNrOfPtables, FH.PSeg, FH.NrOfPtables, FH.PSameMapAllCh);
	}
	for (int i = 0; i < FH.NrOfChannels; i++) {
		SD.getIntUnsigned(1, FH.HalfProb[i]);
	}
}

/* function : Read all filter data from the DST file, which contains:      */
/*            - which channel uses which filter                            */
/*            - for each filter:                                           */
/*              ~ prediction order                                         */
/*              ~ all coefficients                                         */

void CFrameReader::readFilterCoefSets(CStrData& SD, int NrOfChannels, CFrameHeader& FH, CCodedTableF& CF) {
	/* Read the filter parameters */
	for (int FilterNr = 0; FilterNr < FH.NrOfFilters; FilterNr++) {
		SD.getIntUnsigned(SIZE_CODEDPREDORDER, FH.PredOrder[FilterNr]);
		FH.PredOrder[FilterNr]++;
		SD.getIntUnsigned(1, CF.Coded[FilterNr]);
		if (!CF.Coded[FilterNr]) {
			CF.BestMethod[FilterNr] = -1;
			for (int CoefNr = 0; CoefNr < FH.PredOrder[FilterNr]; CoefNr++) {
				SD.getShortSigned(SIZE_PREDCOEF, FH.ICoefA[FilterNr][CoefNr]);
			}
		}
		else {
			SD.getIntUnsigned(SIZE_RICEMETHOD, CF.BestMethod[FilterNr]);
			int bestmethod = CF.BestMethod[FilterNr];
			if (CF.CPredOrder[bestmethod] >= FH.PredOrder[FilterNr]) {
				log_printf("ERROR: Invalid coefficient coding method!");
				return;
			}
			for (int CoefNr = 0; CoefNr < CF.CPredOrder[bestmethod]; CoefNr++) {
				SD.getShortSigned(SIZE_PREDCOEF, FH.ICoefA[FilterNr][CoefNr]);
			}
			SD.getIntUnsigned(SIZE_RICEM, CF.m[FilterNr][bestmethod]);
			for (int CoefNr = CF.CPredOrder[bestmethod]; CoefNr < FH.PredOrder[FilterNr]; CoefNr++) {
				int x = 0;
				int c;
				for (int TapNr = 0; TapNr < CF.CPredOrder[bestmethod]; TapNr++) {
					x += CF.CPredCoef[bestmethod][TapNr] * FH.ICoefA[FilterNr][CoefNr - TapNr - 1];
				}
				if (x >= 0) {
					c = RiceDecode(SD, CF.m[FilterNr][bestmethod]) - (x + 4) / 8;
				}
				else {
					c = RiceDecode(SD, CF.m[FilterNr][bestmethod]) + (-x + 3) / 8;
				}
				if ((c < -(1 << (SIZE_PREDCOEF - 1))) || (c >= (1 << (SIZE_PREDCOEF - 1)))) {
					log_printf("ERROR: filter coefficient out of range!");
					return;
				}
				else {
					FH.ICoefA[FilterNr][CoefNr] = (int16_t)c;
				}
			}
		}
	}
	for (int ChNr = 0; ChNr < NrOfChannels; ChNr++) {
		FH.NrOfHalfBits[ChNr] = FH.PredOrder[FH.FSeg.Table4Segment[ChNr][0]];
	}
}

/* Read all Ptable data from the DST file, which contains:      */
/* - which channel uses which Ptable                            */
/* - for each Ptable all entries                                */

void CFrameReader::readProbabilityTables(CStrData& SD, CFrameHeader& FH, CCodedTableP& CP, int P_one[2 * MAX_CHANNELS][AC_HISMAX]) {
	/* Read the data of all probability tables (table entries) */
	for (int PtableNr = 0; PtableNr < FH.NrOfPtables; PtableNr++)	{
		SD.getIntUnsigned(AC_HISBITS, FH.PtableLen[PtableNr]);
		FH.PtableLen[PtableNr]++;
		if (FH.PtableLen[PtableNr] > 1) {
			SD.getIntUnsigned(1, CP.Coded[PtableNr]);
			if (!CP.Coded[PtableNr]) {
				CP.BestMethod[PtableNr] = -1;
				for (int EntryNr = 0; EntryNr < FH.PtableLen[PtableNr]; EntryNr++) {
					SD.getIntUnsigned(AC_BITS - 1, P_one[PtableNr][EntryNr]);
					P_one[PtableNr][EntryNr]++;
				}
			}
			else {
				SD.getIntUnsigned(SIZE_RICEMETHOD, CP.BestMethod[PtableNr]);
				int bestmethod = CP.BestMethod[PtableNr];
				if (CP.CPredOrder[bestmethod] >= FH.PtableLen[PtableNr]) {
					log_printf("ERROR: Invalid Ptable coding method!");
					return;
				}
				for (int EntryNr = 0; EntryNr < CP.CPredOrder[bestmethod]; EntryNr++) {
					SD.getIntUnsigned(AC_BITS - 1, P_one[PtableNr][EntryNr]);
					P_one[PtableNr][EntryNr]++;
				}
				SD.getIntUnsigned(SIZE_RICEM, CP.m[PtableNr][bestmethod]);
				for (int EntryNr = CP.CPredOrder[bestmethod]; EntryNr < FH.PtableLen[PtableNr]; EntryNr++) {
					int x = 0;
					int c;
					for (int TapNr = 0; TapNr < CP.CPredOrder[bestmethod]; TapNr++) {
						x += CP.CPredCoef[bestmethod][TapNr] * P_one[PtableNr][EntryNr - TapNr - 1];
					}
					if (x >= 0) {
						c = RiceDecode(SD, CP.m[PtableNr][bestmethod]) - (x + 4) / 8;
					}
					else {
						c = RiceDecode(SD, CP.m[PtableNr][bestmethod]) + (-x + 3) / 8;
					}
					if ((c < 1) || (c > (1 << (AC_BITS - 1)))) {
						log_printf("ERROR: Ptable entry out of range!");
						return;
					}
					else {
						P_one[PtableNr][EntryNr] = c;
					}
				}
			}
		}
		else {
			P_one[PtableNr][0] = 128;
			CP.BestMethod[PtableNr] = -1;
		}
	}
}

/* Read arithmetic coded data from the DST file, which contains: */
/* - length of the arithmetic code                               */
/* - all bits of the arithmetic code                             */

void CFrameReader::readArithmeticCodedData(CStrData& SD, int ADataLen, ADataByte* AData) {
	for (int j = 0; j < (ADataLen >> 3); j++) {
		uint8_t v;
		SD.getChrUnsigned(8, v);
		AData[j] = v;
	}
	uint8_t Val = 0;
	for (int j = ADataLen & ~7; j < ADataLen; j++) {
		uint8_t v;
		SD.getChrUnsigned(1, v);
		Val |= v << (7 - (j & 7));
		if (j == ADataLen - 1) {
			AData[j >> 3] = Val;
			Val = 0;
		}
	}
}
