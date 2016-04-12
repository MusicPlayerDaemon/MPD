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

#ifndef DSTCONSTS_H
#define DSTCONSTS_H

#define RESOL               8
#define SIZE_MAXFRAMELEN    4  /* Number of bits for representing framelength in the frame header */
#define SIZE_NROFCHANNELS   4  /* Number of bits for representing NrOfChannels in the frame header */
#define SIZE_DSTFRAMELEN    16 /* Number of bits for representing the DST	framelength in bytes in the frameheader */

                               /* PREDICTION */
#define SIZE_CODEDPREDORDER 7  /* Number of bits in the stream for representing	the "CodedPredOrder" in each frame */
#define SIZE_PREDCOEF       9  /* Number of bits in the stream for representing each filter coefficient in each frame */

                               /* ARITHMETIC CODING */
#define AC_BITS             8  /* Number of bits and maximum level for coding the probability */
#define AC_PROBS            (1 << AC_BITS)
#define AC_HISBITS          6  /* Number of entries in the histogram */
#define AC_HISMAX           (1 << AC_HISBITS)
#define AC_QSTEP            (SIZE_PREDCOEF - AC_HISBITS) /* Quantization step for histogram */

                               /* RICE CODING OF PREDICTION COEFFICIENTS AND PTABLES */
#define NROFFRICEMETHODS    3  /* Number of different Pred. Methods for filters	used in combination with Rice coding */
#define NROFPRICEMETHODS    3  /* Number of different Pred. Methods for Ptables	used in combination with Rice coding */
#define MAXCPREDORDER       3  /* max pred.order for prediction of filter coefs / Ptables entries */
#define SIZE_RICEMETHOD     2  /* nr of bits in stream for indicating method */
#define SIZE_RICEM          3  /* nr of bits in stream for indicating m */
#define MAX_RICE_M_F        6  /* Max. value of m for filters */
#define MAX_RICE_M_P        4  /* Max. value of m for Ptables */


                                 /* SEGMENTATION */
#define MAXNROF_FSEGS       4    /* max nr of segments per channel for filters */
#define MAXNROF_PSEGS       8    /* max nr of segments per channel for Ptables */
#define MIN_FSEG_LEN        1024 /* min segment length in bits of filters      */
#define MIN_PSEG_LEN        32   /* min segment length in bits of Ptables      */

#define MAX_DSTXBITS_SIZE   256  /* DSTXBITS */

/*  64FS44 =>  4704 */
/* 128FS44 =>  9408 */
/* 256FS44 => 18816 */
#define MAX_DSDBYTES_INFRAME	18816

#define MAX_CHANNELS 6
#define MAX_DSDBITS_INFRAME (8 * MAX_DSDBYTES_INFRAME)

#define MAXNROF_SEGS 8 /* max nr of segments per channel for filters or Ptables */

/***** GENERAL *****/

#define BYTESIZE            8  /* Number of bits per byte */

#define MAXCH              64  /* Maximum number of channels */

/* maximum no of bits per channel within a frame */
/* (#bits/channel)/frame at 64 x fs              */
/* for  64FS DSD   37632                         */
/* for 128FS DSD   75264                         */
/* for 256FS DSD  150528                         */

#define MAXCHBITS           (150528)
#define DSDFRAMESIZE        (MAXCHBITS/8)

#define DSDFRAMESIZE_ALLCH  (DSDFRAMESIZE*MAXCH)

/***** CHOLESKY ALGORITHM *****/
#define TRESHOLD            0.334 /* treshold in function Chol() */


/***** PREDICTION FILTER *****/

#define SIZECODEDPREDORDER  7   /* Number of bits for representing the prediction */
/* order (for each channel) in each frame         */
/* SIZECODEDPREDORDER = 7 (128)                   */

#define MAXPREDORDER        (1<<(SIZECODEDPREDORDER)) 
/* Maximum prediction filter order */

#define SIZEPREDCOEF        9   /* Number of bits for representing each filter
coefficient in each frame */

#define PFCOEFSCALER        ((1 << ((SIZEPREDCOEF) - 1)) - 1)
/* Scaler for coefficient normalization */


/***** P-TABLES *****/

#define SIZECODEDPTABLELEN  6  /* Number bits for p-table length */

#define MAXPTABLELEN        (1<<(SIZECODEDPTABLELEN))
/* Maximum length of p-tables */

/* P-table indices */

#define MAXPTIND            ((MAXPTABLELEN)-1)
/* Maximum level for p-table indices */

#define ACQSTEP             ((SIZEPREDCOEF)-(SIZECODEDPTABLELEN)) 
/* Quantization step for histogram */


/* P-table values */

#define SIZEPROBS           8  /* Number of bits for coding probabilities */   

#define PROBLEVELS          (1<<(SIZEPROBS)) /* Number of probability levels 
Maximum level: ((PROBLEVELS)-1) */


/***** ARITHMETIC CODE *****/

#define SIZEADATALENGTH     18 /* Number of bits for representing the length of
the arithmetic coded sequence */

#define MAXADATALEN         (1<<(SIZEADATALENGTH))
/* Maximum length of Arithmetic encoded data  */

/***** DIMENSIONS *****/

#define MAXAUTOLEN          ((MAXPREDORDER)+1)
/* Length of autocorrelation vectors */

#define FRAMEHEADERLEN      8     /* Maximum length of the encoded frame header in bits */

#define ENCWORDLEN          8     /* Length of output words in encoded stream */

#define MAXENCFRAMELEN      ( (MAXCH)*(MAXCHBITS)+(FRAMEHEADERLEN))
/* Maximum length of encoded frame in bits
number of output words in encoded stream =
MAXENCFRAMELEN / ENCWORDLEN */

/*============================================================================*/
/*       STATUS MESSAGES                                                      */
/*============================================================================*/

namespace DST {

	typedef enum {
		DST_NOERROR = 0,
		DST_ERROR = -1
	} ENCODING_STATUS;

	typedef enum {
		NOFRAMEERROR,
		NOMOREFRAMES,
		FRAMEINCOMPLETE
	} RDFRAME_STATUS;

}

#endif
