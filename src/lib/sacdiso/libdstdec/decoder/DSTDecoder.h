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

#ifndef DSTDECODER_H
#define DSTDECODER_H

#include "DSTFramework.h"
#include "CodedTable.h"
#include "StrData.h"

class CDSTDecoder : public CDSTFramework {
public:
	CFrameHeader FrameHdr;                                        /* Contains frame based header information     */

	CCodedTableF StrFilter;                                       /* Contains FIR-coef. compression data         */
	CCodedTableP StrPtable;                                       /* Contains Ptable-entry compression data      */
                                                                /* input stream.                               */
	int          P_one[2 * MAX_CHANNELS][AC_HISMAX];              /* Probability table for arithmetic coder      */
	ADataByte    AData[MAX_DSDBYTES_INFRAME * MAX_CHANNELS];      /* Contains the arithmetic coded bit stream    */
                                                                /* of a complete frame                         */
	int          ADataLen;                                        /* Number of code bits contained in AData[]    */
	CStrData     SD;                                              /* DST data stream */

public:
	CDSTDecoder();
	~CDSTDecoder();
	int init(int channels, int fs44);
	int close();
	int decode(uint8_t* DSTFrame, int frameSize, uint8_t* DSDFrame);
	int unpack(uint8_t* DSTFrame, uint8_t* DSDFrame);
private:
	int16_t reverse7LSBs(int16_t c);
	void fillTable4Bit(CSegment& S, uint8_t Table4Bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME / 2]);
	void LT_InitCoefTablesI(int16_t ICoefI[2 * MAX_CHANNELS][16][256]);
	void LT_InitCoefTablesU(uint16_t ICoefU[2 * MAX_CHANNELS][16][256]);
	void LT_InitStatus(uint8_t Status[MAX_CHANNELS][16]);
	int16_t LT_RunFilterI(int16_t FilterTable[16][256], uint8_t ChannelStatus[16]);
	int16_t LT_RunFilterU(uint16_t FilterTable[16][256], uint8_t ChannelStatus[16]);
};

#endif
