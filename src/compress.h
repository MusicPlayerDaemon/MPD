/* compress.h
 * interface to audio compression
 *
 * (c)2003-6 fluffy@beesbuzz.biz
 */

#ifndef COMPRESS_H
#define COMPRESS_H

/* These are copied from the AudioCompress config.h, mainly because CompressDo
 * needs GAINSHIFT defined.  The rest are here so they can be used as defaults
 * to pass to CompressCfg(). -- jat */
#define ANTICLIP 0		/* Strict clipping protection */
#define TARGET 25000		/* Target level */
#define GAINMAX 32		/* The maximum amount to amplify by */
#define GAINSHIFT 10		/* How fine-grained the gain is */
#define GAINSMOOTH 8		/* How much inertia ramping has*/
#define BUCKETS 400		/* How long of a history to store */

void CompressCfg(int monitor,
		 int anticlip,
		 int target,
		 int maxgain,
		 int smooth,
		 int buckets);

void CompressDo(void *data, unsigned int numSamples);

void CompressFree(void);

#endif
