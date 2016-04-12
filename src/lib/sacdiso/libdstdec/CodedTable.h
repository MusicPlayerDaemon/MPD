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

#ifndef CODEDTABLE_H
#define CODEDTABLE_H

#include "DSTDefs.h"

class CCodedTableBase {
public:
	int TableType;                                                                                    /* FILTER or PTABLE: indicates contents       */
	int StreamBits;                                                                                   /* nr of bits all filters use in the stream   */
	int CPredOrder[NROFFRICEMETHODS];                                                                 /* Code_PredOrder[Method]                     */
	int CPredCoef[NROFPRICEMETHODS][MAXCPREDORDER];                                                   /* Code_PredCoef[Method][CoefNr]              */
	int Coded[2 * MAX_CHANNELS];                                                                      /* DST encode coefs/entries of Fir/PtabNr     */
	int BestMethod[2 * MAX_CHANNELS];                                                                 /* BestMethod[Fir/PtabNr]                     */
	int m[2 * MAX_CHANNELS][NROFFRICEMETHODS];                                                        /* m[Fir/PtabNr][Method]                      */
	int DataLenData[2 * MAX_CHANNELS];                                                                /* Fir/PtabDataLength[Fir/PtabNr]             */
public:
	void calcCCP();
};

class CCodedTable : public CCodedTableBase {
	int Data[2 * MAX_CHANNELS][MAX((1 << SIZE_CODEDPREDORDER) * SIZE_PREDCOEF, AC_BITS * AC_HISMAX)]; /* Fir/PtabData[Fir/PtabNr][Index] */
};

class CCodedTableF : public CCodedTableBase {
	int Data[2 * MAX_CHANNELS][(1 << SIZE_CODEDPREDORDER) * SIZE_PREDCOEF];                           /* FirData[FirNr][Index]                      */
};

class CCodedTableP : public CCodedTableBase {
	int Data[2 * MAX_CHANNELS][AC_BITS * AC_HISMAX];                                                  /* PtabData[PtabNr][Index]                    */
};

#endif
