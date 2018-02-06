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

#ifndef FRAMEREADER_H
#define FRAMEREADER_H

#include "DSTFramework.h"
#include "CodedTable.h"
#include "StrData.h"

class CFrameReader {
public:
	static int log2RoundUp(long x);
	static int RiceDecode(CStrData& SD, int m);
	static void readDSDFrame(CStrData& SD, long MaxFrameLen, int NrOfChannels, uint8_t* DSDFrame);
	static void readTableSegmentData(CStrData& SD, int NrOfChannels, int FrameLen, int MaxNrOfSegs, int MinSegLen, CSegment& S, int& SameSegAllCh);
	static void copySegmentData(CFrameHeader& FH);
	static void readSegmentData(CStrData& SD, CFrameHeader& FH);
	static void readTableMappingData(CStrData& SD, int NrOfChannels, int MaxNrOfTables, CSegment& S, int& NrOfTables, int& SameMapAllCh);
	static void copyMappingData(CFrameHeader& FH);
	static void readMappingData(CStrData& SD, CFrameHeader& FH);
	static void readFilterCoefSets(CStrData& SD, int NrOfChannels, CFrameHeader& FH, CCodedTableF& CF);
	static void readProbabilityTables(CStrData& SD, CFrameHeader& FH, CCodedTableP& CP, int P_one[2 * MAX_CHANNELS][AC_HISMAX]);
	static void readArithmeticCodedData(CStrData& SD, int ADataLen, ADataByte* AData);
};

#endif
