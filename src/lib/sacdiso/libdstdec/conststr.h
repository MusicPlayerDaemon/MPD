#ifndef __CONSTSTR_H_INCLUDED
#define __CONSTSTR_H_INCLUDED

#define  MIN(x,y)    (((x) < (y)) ? (x) : (y))
#define  MAX(x,y)    (((x) > (y)) ? (x) : (y))

#define RESOL               8
#define SIZE_MAXFRAMELEN    4  /* Number of bits for representing framelength 
                                  in the frame header */
#define SIZE_NROFCHANNELS   4  /* Number of bits for representing NrOfChannels
                                  in the frame header */
#define SIZE_DSTFRAMELEN    16 /* Number of bits for representing the DST
                                  framelength in bytes in the frameheader */

/* PREDICTION */
#define SIZE_CODEDPREDORDER 7  /* Number of bits in the stream for representing
                                  the "CodedPredOrder" in each frame */
#define SIZE_PREDCOEF       9  /* Number of bits in the stream for representing
                                  each filter coefficient in each frame */

/* ARITHMETIC CODING */
#define AC_BITS             8  /* Number of bits and maximum level for coding
                                  the probability */
#define AC_PROBS            (1 << AC_BITS)
#define AC_HISBITS          6  /* Number of entries in the histogram */
#define AC_HISMAX           (1 << AC_HISBITS)
#define AC_QSTEP            (SIZE_PREDCOEF - AC_HISBITS)  /* Quantization step 
                                                             for histogram */

/* RICE CODING OF PREDICTION COEFFICIENTS AND PTABLES */
#define NROFFRICEMETHODS    3   /* Number of different Pred. Methods for filters
                                   used in combination with Rice coding       */
#define NROFPRICEMETHODS    3   /* Number of different Pred. Methods for Ptables
                                   used in combination with Rice coding       */
#define MAXCPREDORDER       3   /* max pred.order for prediction of
                                   filter coefs / Ptables entries             */
#define SIZE_RICEMETHOD     2   /* nr of bits in stream for indicating method */
#define SIZE_RICEM          3   /* nr of bits in stream for indicating m      */
#define MAX_RICE_M_F        6   /* Max. value of m for filters                */
#define MAX_RICE_M_P        4   /* Max. value of m for Ptables                */


/* SEGMENTATION */
#define MAXNROF_FSEGS       4     /* max nr of segments per channel for filters */
#define MAXNROF_PSEGS       8     /* max nr of segments per channel for Ptables */
#define MIN_FSEG_LEN        1024  /* min segment length in bits of filters      */
#define MIN_PSEG_LEN        32    /* min segment length in bits of Ptables      */

/* DSTXBITS */
#define MAX_DSTXBITS_SIZE   256

/*  64FS44 =>  4704 */
/* 128FS44 =>  9408 */
/* 256FS44 => 18816 */
#define MAX_DSDBYTES_INFRAME 4704

#define MAX_CHANNELS 6
#define MAX_DSDBITS_INFRAME (8 * MAX_DSDBYTES_INFRAME)

#define MAXNROF_SEGS 8 /* max nr of segments per channel for filters or Ptables */

#endif
