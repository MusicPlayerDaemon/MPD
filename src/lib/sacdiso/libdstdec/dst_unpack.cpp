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
#include "dst_unpack.h"

__FUNCTION_ATTRIBUTES__ int ReadDSDframe(StrData* SD, long MaxFrameLen, int NrOfChannels, uint8_t* DSDFrame);
__FUNCTION_ATTRIBUTES__ int RiceDecode(StrData* SD, int m);
__FUNCTION_ATTRIBUTES__ int Log2RoundUp(long x);
__FUNCTION_ATTRIBUTES__ int ReadTableSegmentData(StrData* SD, int NrOfChannels, int FrameLen, int MaxNrOfSegs, int MinSegLen, Segment* S, int* SameSegAllCh);
__FUNCTION_ATTRIBUTES__ int CopySegmentData(FrameHeader* FH);
__FUNCTION_ATTRIBUTES__ int ReadSegmentData(StrData* SD, FrameHeader* FH);
__FUNCTION_ATTRIBUTES__ int ReadTableMappingData(StrData* SD, int NrOfChannels, int MaxNrOfTables, Segment* S, int* NrOfTables, int* SameMapAllCh);
__FUNCTION_ATTRIBUTES__ int CopyMappingData(FrameHeader* FH);
__FUNCTION_ATTRIBUTES__ int ReadMappingData(StrData* SD, FrameHeader* FH);
__FUNCTION_ATTRIBUTES__ int ReadFilterCoefSets(StrData* SD, int NrOfChannels, FrameHeader* FH, CodedTableF* CF);
__FUNCTION_ATTRIBUTES__ int ReadProbabilityTables(StrData* SD, FrameHeader* FH, CodedTableP* CP, int P_one[2 * MAX_CHANNELS][AC_HISMAX]);
__FUNCTION_ATTRIBUTES__ int ReadArithmeticCodedData(StrData* SD, int ADataLen, uint8_t* AData);

/***************************************************************************/
/*                                                                         */
/* name     : ReadDSDframe                                                 */
/*                                                                         */
/* function : Read DSD signal of this frame from the DST input file.       */
/*                                                                         */
/* pre      : a file must be opened by using getbits_init(),               */
/*            MaxFrameLen, NrOfChannels                                    */
/*                                                                         */
/* post     : BS11[][]                                                     */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadDSDframe(StrData* S, long MaxFrameLen, int NrOfChannels, uint8_t* DSDFrame) {
	int ByteNr;
	int ByteMax = MaxFrameLen * NrOfChannels;
	int return_value;
  
	for (ByteNr = 0; ByteNr < ByteMax; ByteNr++) {
		return_value = FIO_BitGetChrUnsigned(S, 8, &DSDFrame[ByteNr]);
		if (return_value == -1) {
			return -1;
		}
	}
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : RiceDecode                                                   */
/*                                                                         */
/* function : Read a Rice code from the DST file                           */
/*                                                                         */
/* pre      : a file must be opened by using putbits_init(), m             */
/*                                                                         */
/* post     : Returns the Rice decoded number                              */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int RiceDecode(StrData* S, int m) {
	int LSBs;
	int Nr;
	int RLBit;
	int RunLength;
	int Sign;
	int return_value;
	/* Retrieve run length code */
	RunLength = 0;
	do {
		return_value = FIO_BitGetIntUnsigned(S, 1, &RLBit);
		if (return_value == -1) {
			return 0;
		}
		RunLength += (1 - RLBit);
	} while (RLBit == 0);
	/* Retrieve least significant bits */
	return_value = FIO_BitGetIntUnsigned(S, m, &LSBs);
	if (return_value == -1) {
		return 0;
	}
	Nr = (RunLength << m) + LSBs;
	/* Retrieve optional sign bit */
	if (Nr != 0) {
		return_value = FIO_BitGetIntUnsigned(S, 1, &Sign);
		if (return_value == -1) {
			return 0;
		}
		if (Sign == 1) {
			Nr = -Nr;
		}
	}
	return Nr;
}

/***************************************************************************/
/*                                                                         */
/* name     : Log2RoundUp                                                  */
/*                                                                         */
/* function : Calculate the log2 of an integer and round the result up,    */
/*            by using integer arithmetic.                                 */
/*                                                                         */
/* pre      : x                                                            */
/*                                                                         */
/* post     : Returns the rounded up log2 of x.                            */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int Log2RoundUp(long x) {
  int y = 0;
  while (x >= (1 << y)) {
    y++;
  }
  return y;
}

/***************************************************************************/
/*                                                                         */
/* name     : ReadTableSegmentData                                         */
/*                                                                         */
/* function : Read segmentation data for filters or Ptables.               */
/*                                                                         */
/* pre      : NrOfChannels, FrameLen, MaxNrOfSegs, MinSegLen               */
/*                                                                         */
/* post     : S->Resolution, S->SegmentLen[][], S->NrOfSegments[]          */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadTableSegmentData(StrData* SD, int NrOfChannels, int FrameLen, int MaxNrOfSegs, int MinSegLen, Segment* S, int* SameSegAllCh) {
	int ChNr         = 0;
	int DefinedBits  = 0;
	int ResolRead    = 0;
	int SegNr        = 0;
	int MaxSegSize;
	int NrOfBits;
	int EndOfChannel;
	int return_value;

	MaxSegSize = FrameLen - MinSegLen/8;

	return_value = FIO_BitGetIntUnsigned(SD, 1, SameSegAllCh);
	if (return_value == -1) {
		return -1;
	}
	if (*SameSegAllCh == 1)
	{
		return_value = FIO_BitGetIntUnsigned(SD, 1, &EndOfChannel);
		if (return_value == -1) {
			return -1;
		}
		while (EndOfChannel == 0)
		{
			if (SegNr >= MaxNrOfSegs)
			{
				trc_printf("ERROR: Too many segments for this channel!\n");
				return -1;
			}
			if (ResolRead == 0)
			{
				NrOfBits = Log2RoundUp(FrameLen - MinSegLen / 8);
				return_value = FIO_BitGetIntUnsigned(SD, NrOfBits, &S->Resolution);
				if (return_value == -1) {
					return -1;
				}
				if ((S->Resolution == 0) || (S->Resolution > FrameLen - MinSegLen/8))
				{
					trc_printf("ERROR: Invalid segment resolution!\n");
					return -1;
				}
				ResolRead = 1;
			}
			NrOfBits = Log2RoundUp(MaxSegSize / S->Resolution);
			return_value = FIO_BitGetIntUnsigned(SD, NrOfBits, &S->SegmentLen[0][SegNr]);
			if (return_value == -1) {
				return -1;
			}

			if ((S->Resolution * 8 * S->SegmentLen[0][SegNr] < MinSegLen) ||
					(S->Resolution * 8 * S->SegmentLen[0][SegNr] > FrameLen * 8 - DefinedBits - MinSegLen))
			{
				trc_printf("ERROR: Invalid segment length!\n");
				return -1;
			}
			DefinedBits += S->Resolution * 8 * S->SegmentLen[0][SegNr];
			MaxSegSize  -= S->Resolution * S->SegmentLen[0][SegNr];
			SegNr++;
			return_value = FIO_BitGetIntUnsigned(SD, 1, &EndOfChannel);
			if (return_value == -1) {
				return -1;
			}
		}
		S->NrOfSegments[0]      = SegNr + 1;
		S->SegmentLen[0][SegNr] = 0;

		for (ChNr = 1; ChNr < NrOfChannels; ChNr++)
		{
			S->NrOfSegments[ChNr] = S->NrOfSegments[0];
			for (SegNr = 0; SegNr < S->NrOfSegments[0]; SegNr++)
			{
				S->SegmentLen[ChNr][SegNr] = S->SegmentLen[0][SegNr];
			}
		}
	}
	else
	{
		while (ChNr < NrOfChannels)
		{
			if (SegNr >= MaxNrOfSegs)
			{
				trc_printf("ERROR: Too many segments for this channel!\n");
				return -1;
			}
			return_value = FIO_BitGetIntUnsigned(SD, 1, &EndOfChannel);
			if (return_value == -1) {
				return -1;
			}
			if (EndOfChannel == 0)
			{
				if (ResolRead == 0)
				{
					NrOfBits = Log2RoundUp(FrameLen - MinSegLen / 8);
					return_value = FIO_BitGetIntUnsigned(SD, NrOfBits, &S->Resolution);
					if (return_value == -1) {
						return -1;
					}
					if ((S->Resolution == 0) || (S->Resolution > FrameLen - MinSegLen/8))
					{
						trc_printf("ERROR: Invalid segment resolution!\n");
						return -1;
					}
					ResolRead = 1;
				}
				NrOfBits = Log2RoundUp(MaxSegSize / S->Resolution);
				return_value = FIO_BitGetIntUnsigned(SD, NrOfBits, &S->SegmentLen[ChNr][SegNr]);
				if (return_value == -1) {
					return -1;
				}

				if ((S->Resolution * 8 * S->SegmentLen[ChNr][SegNr] < MinSegLen) ||
						(S->Resolution * 8 * S->SegmentLen[ChNr][SegNr] > FrameLen * 8 - DefinedBits - MinSegLen))
				{
					trc_printf("ERROR: Invalid segment length!\n");
					return -1;
				}
				DefinedBits += S->Resolution * 8 * S->SegmentLen[ChNr][SegNr];
				MaxSegSize  -= S->Resolution * S->SegmentLen[ChNr][SegNr];
				SegNr++;
			}
			else
			{
				S->NrOfSegments[ChNr]      = SegNr + 1;
				S->SegmentLen[ChNr][SegNr] = 0;
				SegNr                      = 0;
				DefinedBits                = 0;
				MaxSegSize                 = FrameLen - MinSegLen/8;
				ChNr++;
			}
		}
	}
	if (ResolRead == 0)
	{
		S->Resolution = 1;
	}
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : CopySegmentData                                              */
/*                                                                         */
/* function : Read segmentation data for filters and Ptables.              */
/*                                                                         */
/* pre      : FH->NrOfChannels, FH->FSeg.Resolution,                       */
/*            FH->FSeg.NrOfSegments[], FH->FSeg.SegmentLen[][]             */
/*                                                                         */
/* post     : FH-> : PSeg : .Resolution, .NrOfSegments[], .SegmentLen[][], */
/*                   PSameSegAllCh                                         */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int CopySegmentData(FrameHeader* FH)
{
  int ChNr;
  int SegNr;

  int *dst = FH->PSeg.NrOfSegments, *src = FH->FSeg.NrOfSegments;

  FH->PSeg.Resolution = FH->FSeg.Resolution;
  FH->PSameSegAllCh   = 1;
  for (ChNr = 0; ChNr < FH->NrOfChannels; ChNr++)
  {
    dst[ChNr] = src[ChNr];
    if (dst[ChNr] > MAXNROF_PSEGS)
    {
      trc_printf("ERROR: Too many segments!\n");
			return -1;
    }
    if (dst[ChNr] != dst[0])
    {
      FH->PSameSegAllCh = 0;
    }
    for (SegNr = 0; SegNr < dst[ChNr]; SegNr++)
    {
      int *lendst = FH->PSeg.SegmentLen[ChNr], *lensrc = FH->FSeg.SegmentLen[ChNr];

      lendst[SegNr] = lensrc[SegNr];
      if ((lendst[SegNr] != 0) && (FH->PSeg.Resolution*8*lendst[SegNr]<MIN_PSEG_LEN))
      {
        trc_printf("ERROR: Invalid segment length!\n");
				return -1;
      }
      if (lendst[SegNr] != FH->PSeg.SegmentLen[0][SegNr])
      {
        FH->PSameSegAllCh = 0;
      }
    }
  }
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : ReadSegmentData                                              */
/*                                                                         */
/* function : Read segmentation data for filters and Ptables.              */
/*                                                                         */
/* pre      : FH->NrOfChannels, CO->MaxFrameLen                            */
/*                                                                         */
/* post     : FH-> : FSeg : .Resolution, .SegmentLen[][], .NrOfSegments[], */
/*                   PSeg : .Resolution, .SegmentLen[][], .NrOfSegments[], */
/*                   PSameSegAsF, FSameSegAllCh, PSameSegAllCh             */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadSegmentData(StrData* SD, FrameHeader* FH)
{
	int return_value;
  return_value = FIO_BitGetIntUnsigned(SD, 1, &FH->PSameSegAsF);
	if (return_value == -1) {
		return -1;
	}
  return_value = ReadTableSegmentData(SD, FH->NrOfChannels, FH->MaxFrameLen, MAXNROF_FSEGS, MIN_FSEG_LEN, &FH->FSeg, &FH->FSameSegAllCh);
	if (return_value == -1) {
		return -1;
	}
  if (FH->PSameSegAsF == 1)
  {
		return_value = CopySegmentData(FH);
  }
  else
  {
		return_value = ReadTableSegmentData(SD, FH->NrOfChannels, FH->MaxFrameLen, MAXNROF_PSEGS, MIN_PSEG_LEN, &FH->PSeg, &FH->PSameSegAllCh);
  }
	return return_value;
}

/***************************************************************************/
/*                                                                         */
/* name     : ReadTableMappingData                                         */
/*                                                                         */
/* function : Read mapping data for filters or Ptables.                    */
/*                                                                         */
/* pre      : NrOfChannels, MaxNrOfTables, S->NrOfSegments[]               */
/*                                                                         */
/* post     : S->Table4Segment[][], NrOfTables, SameMapAllCh               */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadTableMappingData(StrData* SD, int NrOfChannels, int MaxNrOfTables, Segment* S, int* NrOfTables, int* SameMapAllCh)
{
  int ChNr;
  int CountTables = 1;
  int NrOfBits    = 1;
  int SegNr;

  S->Table4Segment[0][0] = 0;

  FIO_BitGetIntUnsigned(SD, 1, SameMapAllCh);
  if (*SameMapAllCh == 1)
  {
    for (SegNr = 1; SegNr < S->NrOfSegments[0]; SegNr++)
    {
			NrOfBits = Log2RoundUp(CountTables);
			FIO_BitGetIntUnsigned(SD, NrOfBits, &S->Table4Segment[0][SegNr]);

      if (S->Table4Segment[0][SegNr] == CountTables)
      {
        CountTables++;
      }
      else if (S->Table4Segment[0][SegNr] > CountTables)
      {
        trc_printf("ERROR: Invalid table number for segment!\n");
				return -1;
      }
    }
    for(ChNr = 1; ChNr < NrOfChannels; ChNr++)
    {
      if (S->NrOfSegments[ChNr] != S->NrOfSegments[0])
      {
        trc_printf("ERROR: Mapping can't be the same for all channels!\n");
				return -1;
      }
      for (SegNr = 0; SegNr < S->NrOfSegments[0]; SegNr++)
      {
        S->Table4Segment[ChNr][SegNr] = S->Table4Segment[0][SegNr];
      }
    }
  }
  else
  {
    for(ChNr = 0; ChNr < NrOfChannels; ChNr++)
    {
      for (SegNr = 0; SegNr < S->NrOfSegments[ChNr]; SegNr++)
      {
        if ((ChNr != 0) || (SegNr != 0))
        {
					NrOfBits = Log2RoundUp(CountTables);
					FIO_BitGetIntUnsigned(SD, NrOfBits, &S->Table4Segment[ChNr][SegNr]);

          if (S->Table4Segment[ChNr][SegNr] == CountTables)
          {
            CountTables++;
          }
          else if (S->Table4Segment[ChNr][SegNr] > CountTables)
          {
            trc_printf("ERROR: Invalid table number for segment!\n");
						return -1;
          }
        }
      }
    }
  }
  if (CountTables > MaxNrOfTables)
  {
    trc_printf("ERROR: Too many tables for this frame!\n");
		return -1;
  }
  *NrOfTables = CountTables;
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : CopyMappingData                                              */
/*                                                                         */
/* function : Copy mapping data for Ptables from the filter mapping.       */
/*                                                                         */
/* pre      : CO-> : NrOfChannels, MaxNrOfPtables                          */
/*            FH-> : FSeg.NrOfSegments[], FSeg.Table4Segment[][],          */
/*                   NrOfFilters, PSeg.NrOfSegments[]                      */
/*                                                                         */
/* post     : FH-> : PSeg.Table4Segment[][], NrOfPtables, PSameMapAllCh    */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int CopyMappingData(FrameHeader* FH)
{
  int ChNr;
  int SegNr;

  FH->PSameMapAllCh = 1;
  for (ChNr = 0; ChNr < FH->NrOfChannels; ChNr++)
  {
    if (FH->PSeg.NrOfSegments[ChNr] == FH->FSeg.NrOfSegments[ChNr])
    {
      for (SegNr = 0; SegNr < FH->FSeg.NrOfSegments[ChNr]; SegNr++)
      {
        FH->PSeg.Table4Segment[ChNr][SegNr]=FH->FSeg.Table4Segment[ChNr][SegNr];
        if (FH->PSeg.Table4Segment[ChNr][SegNr] != FH->PSeg.Table4Segment[0][SegNr])
        {
          FH->PSameMapAllCh = 0;
        }
      }
    }
    else
    {
      trc_printf("ERROR: Not same number of segments for filters and Ptables!\n");
			return -1;
    }
  }
  FH->NrOfPtables = FH->NrOfFilters;
  if (FH->NrOfPtables > FH->MaxNrOfPtables)
  {
    trc_printf("ERROR: Too many tables for this frame!\n");
		return -1;
  }
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : ReadMappingData                                              */
/*                                                                         */
/* function : Read mapping data (which channel uses which filter/Ptable).  */
/*                                                                         */
/* pre      : CO-> : NrOfChannels, MaxNrOfFilters, MaxNrOfPtables          */
/*            FH-> : FSeg.NrOfSegments[], PSeg.NrOfSegments[]              */
/*                                                                         */
/* post     : FH-> : FSeg.Table4Segment[][], .NrOfFilters,                 */
/*                   PSeg.Table4Segment[][], .NrOfPtables,                 */
/*                   PSameMapAsF, FSameMapAllCh, PSameMapAllCh, HalfProb[] */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadMappingData(StrData *SD, FrameHeader* FH)
{
  int j;
	int return_value;
	return_value = FIO_BitGetIntUnsigned(SD, 1, &FH->PSameMapAsF);
	if (return_value == -1) {
		return -1;
	}
	return_value = ReadTableMappingData(SD, FH->NrOfChannels, FH->MaxNrOfFilters, &FH->FSeg, &FH->NrOfFilters, &FH->FSameMapAllCh);
	if (return_value == -1) {
		return -1;
	}
  if (FH->PSameMapAsF == 1)
  {
		return_value = CopyMappingData(FH);
		if (return_value == -1) {
			return -1;
		}
  }
  else
  {
		return_value = ReadTableMappingData(SD, FH->NrOfChannels, FH->MaxNrOfPtables, &FH->PSeg, &FH->NrOfPtables, &FH->PSameMapAllCh);
		if (return_value == -1) {
			return -1;
		}
  }

  for (j = 0; j < FH->NrOfChannels; j++)
  {
		return_value = FIO_BitGetIntUnsigned(SD, 1, &FH->HalfProb[j]);
		if (return_value == -1) {
			return -1;
		}
  }
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : ReadFilterCoefSets                                           */
/*                                                                         */
/* function : Read all filter data from the DST file, which contains:      */
/*            - which channel uses which filter                            */
/*            - for each filter:                                           */
/*              ~ prediction order                                         */
/*              ~ all coefficients                                         */
/*                                                                         */
/* pre      : a file must be opened by using getbits_init(), NrOfChannels  */
/*            FH->NrOfFilters, CF->CPredOrder[], CF->CPredCoef[][],        */
/*            FH->FSeg.Table4Segment[][0]                                  */
/*                                                                         */
/* post     : FH->PredOrder[], FH->ICoefA[][], FH->NrOfHalfBits[],         */
/*            CF->Coded[], CF->BestMethod[], CF->m[][],                    */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadFilterCoefSets(StrData* SD, int NrOfChannels, FrameHeader* FH, CodedTableF* CF)
{
  int c;
  int ChNr;
  int CoefNr;
  int FilterNr;
  int TapNr;
  int x;

  /* Read the filter parameters */
  for(FilterNr = 0; FilterNr < FH->NrOfFilters; FilterNr++)
  {
		FIO_BitGetIntUnsigned(SD, SIZE_CODEDPREDORDER, &FH->PredOrder[FilterNr]);
    FH->PredOrder[FilterNr]++;

		FIO_BitGetIntUnsigned(SD, 1, &CF->Coded[FilterNr]);
    if (CF->Coded[FilterNr] == 0)
    {
      CF->BestMethod[FilterNr] = -1;
      for(CoefNr = 0; CoefNr < FH->PredOrder[FilterNr]; CoefNr++)
      {
				FIO_BitGetShortSigned(SD, SIZE_PREDCOEF, &FH->ICoefA[FilterNr][CoefNr]);
      }
    }
    else
    {
      int bestmethod;

			FIO_BitGetIntUnsigned(SD, SIZE_RICEMETHOD, &CF->BestMethod[FilterNr]);
      bestmethod = CF->BestMethod[FilterNr];
      if (CF->CPredOrder[bestmethod] >= FH->PredOrder[FilterNr])
      {
        trc_printf("ERROR: Invalid coefficient coding method!\n");
				return -1;
      }

      for(CoefNr = 0; CoefNr < CF->CPredOrder[bestmethod]; CoefNr++)
      {
				FIO_BitGetShortSigned(SD, SIZE_PREDCOEF, &FH->ICoefA[FilterNr][CoefNr]);
      }

			FIO_BitGetIntUnsigned(SD, SIZE_RICEM, &CF->m[FilterNr][bestmethod]);
      
      for(CoefNr = CF->CPredOrder[bestmethod]; CoefNr < FH->PredOrder[FilterNr]; CoefNr++)
      {
        for (TapNr = 0, x = 0; TapNr < CF->CPredOrder[bestmethod]; TapNr++)
        {
          x += CF->CPredCoef[bestmethod][TapNr] * FH->ICoefA[FilterNr][CoefNr - TapNr - 1];
        }

        if (x >= 0)
        {
					c = RiceDecode(SD, CF->m[FilterNr][bestmethod]) - (x + 4) / 8;
        }
        else
        {
					c = RiceDecode(SD, CF->m[FilterNr][bestmethod]) + (-x + 3) / 8;
        }

        if ((c < -(1<<(SIZE_PREDCOEF-1))) || (c >= (1<<(SIZE_PREDCOEF-1))))
        {
          trc_printf("ERROR: filter coefficient out of range!\n");
					return -1;
        }
        else
        {
          FH->ICoefA[FilterNr][CoefNr] = (int16_t) c;
        }
      }
    }

    /* Clear out remaining coeffs, as the SSE2 code uses them all. */
    dst_memset(&FH->ICoefA[FilterNr][CoefNr], 0, ((1<<SIZE_CODEDPREDORDER) - CoefNr) * sizeof(**FH->ICoefA));
  }

  for (ChNr = 0; ChNr < NrOfChannels; ChNr++)
  {
    FH->NrOfHalfBits[ChNr] = FH->PredOrder[FH->FSeg.Table4Segment[ChNr][0]];
  }

	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : ReadProbabilityTables                                        */
/*                                                                         */
/* function : Read all Ptable data from the DST file, which contains:      */
/*            - which channel uses which Ptable                            */
/*            - for each Ptable all entries                                */
/*                                                                         */
/* pre      : a file must be opened by using getbits_init(),               */
/*            FH->NrOfPtables, CP->CPredOrder[], CP->CPredCoef[][]         */
/*                                                                         */
/* post     : FH->PtableLen[], CP->Coded[], CP->BestMethod[], CP->m[][],   */
/*            P_one[][]                                                    */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadProbabilityTables(StrData* SD, FrameHeader* FH, CodedTableP* CP, int P_one[2 * MAX_CHANNELS][AC_HISMAX])
{
  int c;
  int EntryNr;
  int PtableNr;
  int TapNr;
  int x;

  /* Read the data of all probability tables (table entries) */
  for(PtableNr = 0; PtableNr < FH->NrOfPtables; PtableNr++)
  {
		FIO_BitGetIntUnsigned(SD, AC_HISBITS, &FH->PtableLen[PtableNr]);
    FH->PtableLen[PtableNr]++;

    if (FH->PtableLen[PtableNr] > 1)
    {
			FIO_BitGetIntUnsigned(SD, 1, &CP->Coded[PtableNr]);
      
      if (CP->Coded[PtableNr] == 0)
      {
        CP->BestMethod[PtableNr] = -1;
        for(EntryNr = 0; EntryNr < FH->PtableLen[PtableNr]; EntryNr++)
        {
					FIO_BitGetIntUnsigned(SD, AC_BITS - 1, &P_one[PtableNr][EntryNr]);
          P_one[PtableNr][EntryNr]++;
        }
      }
      else
      {
        int bestmethod;
				FIO_BitGetIntUnsigned(SD, SIZE_RICEMETHOD, &CP->BestMethod[PtableNr]);
        bestmethod = CP->BestMethod[PtableNr];
        if (CP->CPredOrder[bestmethod] >= FH->PtableLen[PtableNr])
        {
          trc_printf("ERROR: Invalid Ptable coding method!\n");
					return -1;
        }

        for(EntryNr = 0; EntryNr < CP->CPredOrder[bestmethod]; EntryNr++)
        {
					FIO_BitGetIntUnsigned(SD, AC_BITS - 1, &P_one[PtableNr][EntryNr]);
          P_one[PtableNr][EntryNr]++;
        }

				FIO_BitGetIntUnsigned(SD, SIZE_RICEM, &CP->m[PtableNr][bestmethod]);

        for(EntryNr = CP->CPredOrder[bestmethod]; EntryNr < FH->PtableLen[PtableNr]; EntryNr++)
        {
          for (TapNr = 0, x = 0; TapNr < CP->CPredOrder[bestmethod]; TapNr++)
          {
            x += CP->CPredCoef[bestmethod][TapNr] * P_one[PtableNr][EntryNr - TapNr - 1];
          }

          if (x >= 0)
          {
						c = RiceDecode(SD, CP->m[PtableNr][bestmethod]) - (x + 4) / 8;
          }
          else
          {
						c = RiceDecode(SD, CP->m[PtableNr][bestmethod]) + (-x + 3) / 8;
          }

          if ((c < 1) || (c > (1 << (AC_BITS - 1))))
          {
            trc_printf("ERROR: Ptable entry out of range!\n");
						return -1;
          }
          else
          {
            P_one[PtableNr][EntryNr] = c;
          }
        }
      }
    }
    else
    {
      P_one[PtableNr][0]       = 128;
      CP->BestMethod[PtableNr] = -1;
    }
  }
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : ReadArithmeticCodeData                                       */
/*                                                                         */
/* function : Read arithmetic coded data from the DST file, which contains:*/
/*            - length of the arithmetic code                              */
/*            - all bits of the arithmetic code                            */
/*                                                                         */
/* pre      : a file must be opened by using getbits_init(), ADataLen      */
/*                                                                         */
/* post     : AData[]                                                      */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int ReadArithmeticCodedData(StrData* SD, int ADataLen, ADataByte* AData) {
	int j;
	int return_value;
	uint8_t Val;
	for (j = 0; j < (ADataLen >> 3); j++) {
		uint8_t v;
		return_value = FIO_BitGetChrUnsigned(SD, 8, &v);
		if (return_value == -1) {
			return -1;
		}
		AData[j] = v;
	}
	Val = 0;
	for (j = ADataLen & ~7; j < ADataLen; j++) {
		uint8_t v;
		return_value = FIO_BitGetChrUnsigned(SD, 1, &v);
		if (return_value == -1) {
			return -1;
		}
		Val |= v << (7 - (j & 7));
		if (j == ADataLen - 1) {
			AData[j >> 3] = Val;
			Val = 0;
		}
	}
	return 0;
}

/***************************************************************************/
/*                                                                         */
/* name     : UnpackDSTframe                                               */
/*                                                                         */
/* function : Read a complete frame from the DST input file                */
/*                                                                         */
/* pre      : a file must be opened by using getbits_init()                */
/*                                                                         */
/* post     : Complete D-structure                                         */
/*                                                                         */
/* uses     : types.h, fio_bit.h, stdio.h, stdlib.h, constopt.h,           */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int UnpackDSTframe(DstDec* D, uint8_t* DSTdataframe, uint8_t* DSDdataframe) {
	int   Dummy;
	int   Ready = 0;
	int   return_value;

	/* fill internal buffer with DSTframe */
	FillBuffer(&D->S, DSTdataframe, D->FrameHdr.CalcNrOfBytes);

	/* interpret DST header byte */
	return_value = FIO_BitGetIntUnsigned(&D->S, 1, &D->FrameHdr.DSTCoded);
	if (return_value == -1) {
		return -1;
	}

	if (D->FrameHdr.DSTCoded == 0)
	{
		return_value = FIO_BitGetIntUnsigned(&D->S, 1, &Dummy);	/* Was &D->DstXbits.Bit, but it was never used */
		if (return_value == -1) {
			return -1;
		}
		return_value = FIO_BitGetIntUnsigned(&D->S, 6, &Dummy);
		if (return_value == -1) {
			return -1;
		}
		if (Dummy != 0) {
			trc_printf("ERROR: Illegal stuffing pattern in frame %d!\n", D->FrameHdr.FrameNr);
			return -1;
		}

		/* Read DSD data and put in output stream */
		return_value = ReadDSDframe(&D->S, D->FrameHdr.MaxFrameLen, D->FrameHdr.NrOfChannels, DSDdataframe);
		if (return_value == -1) {
			return -1;
		}
	}
	else
	{
		return_value = ReadSegmentData(&D->S, &D->FrameHdr);
		if (return_value == -1) {
			return -1;
		}

		return_value = ReadMappingData(&D->S, &D->FrameHdr);
		if (return_value == -1) {
			return -1;
		}

		return_value = ReadFilterCoefSets(&D->S, D->FrameHdr.NrOfChannels, &D->FrameHdr, &D->StrFilter);
		if (return_value == -1) {
			return -1;
		}

		return_value = ReadProbabilityTables(&D->S, &D->FrameHdr, &D->StrPtable, D->P_one);
		if (return_value == -1) {
			return -1;
		}

		D->ADataLen = D->FrameHdr.CalcNrOfBits - get_in_bitcount(&D->S);
		return_value = ReadArithmeticCodedData(&D->S, D->ADataLen, D->AData);
		if (return_value == -1) {
			return -1;
		}

		if (D->ADataLen > 0 && GET_BIT(D->AData, 0) != 0) {
			trc_printf("ERROR: Illegal arithmetic code in frame %d!\n", D->FrameHdr.FrameNr);
			return -1;
		}
  }

  return Ready;
}
