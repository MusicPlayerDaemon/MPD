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

#ifndef __TYPES_H_INCLUDED
#define __TYPES_H_INCLUDED

#include <stdint.h>
#include "config.h"
#include "conststr.h"

#define GET_BIT(BitBase, BitIndex) ((((uint8_t*)BitBase)[BitIndex >> 3] >> (7 - (BitIndex & 7))) & 1)
#define GET_NIBBLE(NibbleBase, NibbleIndex) ((((uint8_t*)NibbleBase)[NibbleIndex >> 1] >> ((NibbleIndex & 1) << 2)) & 0x0f)

enum TTable {FILTER, PTABLE};

typedef struct {
	int Resolution;                                /* Resolution for segments        */
	int SegmentLen[MAX_CHANNELS][MAXNROF_SEGS];    /* SegmentLen[ChNr][SegmentNr]    */
	int NrOfSegments[MAX_CHANNELS];                /* NrOfSegments[ChNr]             */
	int Table4Segment[MAX_CHANNELS][MAXNROF_SEGS]; /* Table4Segment[ChNr][SegmentNr] */
} Segment;

typedef struct {
	int     FrameNr;                                            /* Nr of frame that is currently processed    */
	int     NrOfChannels;                                       /* Number of channels in the recording        */
	int     NrOfFilters;                                        /* Number of filters used for this frame      */
	int     NrOfPtables;                                        /* Number of Ptables used for this frame      */
	int     Fsample44;                                          /* Sample frequency 64, 128, 256              */
	int     PredOrder[2 * MAX_CHANNELS];                        /* Prediction order used for this frame       */
	int     PtableLen[2 * MAX_CHANNELS];                        /* Nr of Ptable entries used for this frame   */
	int16_t ICoefA[2 * MAX_CHANNELS][1 << SIZE_CODEDPREDORDER]; /* Integer coefs for actual coding            */
	int     DSTCoded;                                           /* 1=DST coded is put in DST stream,          */
																															/* 0=DSD is put in DST stream                 */
	long    CalcNrOfBytes;                                      /* Contains number of bytes of the complete   */
	long    CalcNrOfBits;                                       /* Contains number of bits of the complete    */
																															/* channel stream after arithmetic encoding   */
																															/* (also containing bytestuff-,               */
																															/* ICoefA-bits, etc.)                         */
	int     HalfProb[MAX_CHANNELS];                             /* Defines per channel which probability is   */
																															/* applied for the first PredOrder[] bits of  */
																															/* a frame (0 = use Ptable entry, 1 = 128)    */
	int     NrOfHalfBits[MAX_CHANNELS];                         /* Defines per channel how many bits at the   */
																															/* start of each frame are optionally coded   */
																															/* with p=0.5                                 */
	Segment FSeg;                                               /* Contains segmentation data for filters     */
	uint8_t Filter4Bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME / 2];  /* Filter4Bit[ChNr][BitNr]                    */
	Segment PSeg;                                               /* Contains segmentation data for Ptables     */
	uint8_t Ptable4Bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME / 2];  /* Ptable4Bit[ChNr][BitNr]                    */
	int     PSameSegAsF;                                        /* 1 if segmentation is equal for F and P     */
	int     PSameMapAsF;                                        /* 1 if mapping is equal for F and P          */
	int     FSameSegAllCh;                                      /* 1 if all channels have same Filtersegm.    */
	int     FSameMapAllCh;                                      /* 1 if all channels have same Filtermap      */
	int     PSameSegAllCh;                                      /* 1 if all channels have same Ptablesegm.    */
	int     PSameMapAllCh;                                      /* 1 if all channels have same Ptablemap      */
	int     SegAndMapBits;                                      /* Number of bits in the stream for Seg&Map   */
	int     MaxNrOfFilters;                                     /* Max. nr. of filters allowed per frame      */
	int     MaxNrOfPtables;                                     /* Max. nr. of Ptables allowed per frame      */
	long    MaxFrameLen;                                        /* Max frame length of this file              */
	long    ByteStreamLen;                                      /* MaxFrameLen * NrOfChannels                 */
	long    BitStreamLen;                                       /* ByteStreamLen * RESOL                      */
	long    NrOfBitsPerCh;                                      /* MaxFrameLen * RESOL                        */
} FrameHeader;

typedef struct {
	int TableType;                                                                                    /* FILTER or PTABLE: indicates contents       */
	int StreamBits;                                                                                   /* nr of bits all filters use in the stream   */
	int CPredOrder[NROFFRICEMETHODS];                                                                 /* Code_PredOrder[Method]                     */
	int CPredCoef[NROFPRICEMETHODS][MAXCPREDORDER];                                                   /* Code_PredCoef[Method][CoefNr]              */
	int Coded[2 * MAX_CHANNELS];                                                                      /* DST encode coefs/entries of Fir/PtabNr     */
	int BestMethod[2 * MAX_CHANNELS];                                                                 /* BestMethod[Fir/PtabNr]                     */
	int m[2 * MAX_CHANNELS][NROFFRICEMETHODS];                                                        /* m[Fir/PtabNr][Method]                      */
	int DataLenData[2 * MAX_CHANNELS];                                                                /* Fir/PtabDataLength[Fir/PtabNr]             */
	int Data[2 * MAX_CHANNELS][MAX((1 << SIZE_CODEDPREDORDER) * SIZE_PREDCOEF, AC_BITS * AC_HISMAX)]; /* Fir/PtabData[Fir/PtabNr][Index]            */
} CodedTable;

typedef struct {
	int TableType;                                                                                    /* FILTER or PTABLE: indicates contents       */
	int StreamBits;                                                                                   /* nr of bits all filters use in the stream   */
	int CPredOrder[NROFFRICEMETHODS];                                                                 /* Code_PredOrder[Method]                     */
	int CPredCoef[NROFPRICEMETHODS][MAXCPREDORDER];                                                   /* Code_PredCoef[Method][CoefNr]              */
	int Coded[2 * MAX_CHANNELS];                                                                      /* DST encode coefs/entries of Fir/PtabNr     */
	int BestMethod[2 * MAX_CHANNELS];                                                                 /* BestMethod[Fir/PtabNr]                     */
	int m[2 * MAX_CHANNELS][NROFFRICEMETHODS];                                                        /* m[Fir/PtabNr][Method]                      */
	int DataLenData[2 * MAX_CHANNELS];                                                                /* Fir/PtabDataLength[Fir/PtabNr]             */
	int Data[2 * MAX_CHANNELS][(1 << SIZE_CODEDPREDORDER) * SIZE_PREDCOEF];                           /* FirData[FirNr][Index]                      */
} CodedTableF;

typedef struct {
	int TableType;                                                                                    /* FILTER or PTABLE: indicates contents       */
	int StreamBits;                                                                                   /* nr of bits all filters use in the stream   */
	int CPredOrder[NROFFRICEMETHODS];                                                                 /* Code_PredOrder[Method]                     */
	int CPredCoef[NROFPRICEMETHODS][MAXCPREDORDER];                                                   /* Code_PredCoef[Method][CoefNr]              */
	int Coded[2 * MAX_CHANNELS];                                                                      /* DST encode coefs/entries of Fir/PtabNr     */
	int BestMethod[2 * MAX_CHANNELS];                                                                 /* BestMethod[Fir/PtabNr]                     */
	int m[2 * MAX_CHANNELS][NROFFRICEMETHODS];                                                        /* m[Fir/PtabNr][Method]                      */
	int DataLenData[2 * MAX_CHANNELS];                                                                /* Fir/PtabDataLength[Fir/PtabNr]             */
	int Data[2 * MAX_CHANNELS][AC_BITS * AC_HISMAX];                                                  /* PtabData[PtabNr][Index]                    */
} CodedTableP;

typedef struct {
	uint8_t DSTdata[MAX_CHANNELS * MAX_DSDBYTES_INFRAME];
	int32_t TotalBytes;
	int32_t ByteCounter;
	int     BitPosition;
	uint8_t DataByte;
} StrData;

typedef struct {
	unsigned int Init;
	unsigned int C;
	unsigned int A;
	int          cbptr;
} ACData;

typedef uint8_t ADataByte;

typedef struct {
	FrameHeader  FrameHdr;                                       /* Contains frame based header information     */

	CodedTableF  StrFilter;                                      /* Contains FIR-coef. compression data         */
	CodedTableP  StrPtable;                                      /* Contains Ptable-entry compression data      */
																															 /* input stream.                               */
	int          P_one[2 * MAX_CHANNELS][AC_HISMAX];             /* Probability table for arithmetic coder      */
	ADataByte    AData[MAX_DSDBYTES_INFRAME * MAX_CHANNELS];     /* Contains the arithmetic coded bit stream    */
																															 /* of a complete frame                         */
	int          ADataLen;                                       /* Number of code bits contained in AData[]    */
	StrData      S;                                              /* DST data stream */
} DstDec;

#endif
