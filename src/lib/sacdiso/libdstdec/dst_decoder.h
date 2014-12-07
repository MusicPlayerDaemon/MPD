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

#ifndef __DSTDECODER_H_INCLUDED
#define __DSTDECODER_H_INCLUDED

#include "types.h"
#include "dst_unpack.h"

__FUNCTION_ATTRIBUTES__ int Init(DstDec* D, int NrChannels, int Fs44);
__FUNCTION_ATTRIBUTES__ int Close(DstDec* D);
__FUNCTION_ATTRIBUTES__ int Decode(DstDec* D, uint8_t* DSTFrame, uint8_t* DSDMuxedChannelData, int FrameCnt, int* FrameSize);

#endif
