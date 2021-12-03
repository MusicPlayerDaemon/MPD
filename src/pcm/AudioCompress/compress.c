/* compress.c
 * Compressor logic
 *
 * (c)2007 busybee (http://beesbuzz.biz/
 * Licensed under the terms of the LGPL. See the file COPYING for details.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "compress.h"

struct Compressor {
        //! The compressor's preferences
        struct CompressorConfig prefs;

        //! History of the peak values
        int *peaks;

        //! History of the gain values
        int *gain;

        //! History of clip amounts
        int *clipped;

        unsigned int pos;
        unsigned int bufsz;
};

struct Compressor *Compressor_new(unsigned int history)
{
	struct Compressor *obj = malloc(sizeof(struct Compressor));
	if (obj == NULL)
		/* out of memory, not much we can do */
		abort();

	obj->prefs.target = TARGET;
	obj->prefs.maxgain = GAINMAX;
	obj->prefs.smooth = GAINSMOOTH;

        obj->peaks = obj->gain = obj->clipped = NULL;
	obj->bufsz = 0;
        obj->pos = 0;

        Compressor_setHistory(obj, history);

        return obj;
}

void Compressor_delete(struct Compressor *obj)
{
	if (obj->peaks)
		free(obj->peaks);
	if (obj->gain)
		free(obj->gain);
	if (obj->clipped)
		free(obj->clipped);
	free(obj);
}

static int *resizeArray(int *data, int newsz, int oldsz)
{
        data = realloc(data, newsz*sizeof(int));
	if (data == NULL)
		/* out of memory, not much we can do */
		abort();

        if (newsz > oldsz)
                memset(data + oldsz, 0, sizeof(int)*(newsz - oldsz));
        return data;
}

void Compressor_setHistory(struct Compressor *obj, unsigned int history)
{
	if (!history)
                history = BUCKETS;

        obj->peaks = resizeArray(obj->peaks, history, obj->bufsz);
        obj->gain = resizeArray(obj->gain, history, obj->bufsz);
        obj->clipped = resizeArray(obj->clipped, history, obj->bufsz);
        obj->bufsz = history;
}

struct CompressorConfig *Compressor_getConfig(struct Compressor *obj)
{
        return &obj->prefs;
}

void Compressor_Process_int16(struct Compressor *obj, int16_t *audio,
                              unsigned int count)
{
        struct CompressorConfig *prefs = Compressor_getConfig(obj);
	int16_t *ap;
	unsigned int i;
        int *peaks = obj->peaks;
        int curGain = obj->gain[obj->pos];
        int newGain;
        int peakVal = 1;
        int peakPos = 0;
        int slot = (obj->pos + 1) % obj->bufsz;
        int *clipped = obj->clipped + slot;
        unsigned int ramp = count;
        int delta;

	ap = audio;
	for (i = 0; i < count; i++)
	{
		int val = *ap++;
                if (val < 0)
                        val = -val;
		if (val > peakVal)
                {
			peakVal = val;
                        peakPos = i;
                }
	}
	peaks[slot] = peakVal;


	for (i = 0; i < obj->bufsz; i++)
	{
		if (peaks[i] > peakVal)
		{
			peakVal = peaks[i];
			peakPos = 0;
		}
	}

	//! Determine target gain
	newGain = (1 << 10)*prefs->target/peakVal;

        //! Adjust the gain with inertia from the previous gain value
        newGain = (curGain*((1 << prefs->smooth) - 1) + newGain)
                >> prefs->smooth;

        //! Make sure it's no more than the maximum gain value
        if (newGain > (prefs->maxgain << 10))
                newGain = prefs->maxgain << 10;

        //! Make sure it's no less than 1:1
	if (newGain < (1 << 10))
		newGain = 1 << 10;

        //! Make sure the adjusted gain won't cause clipping
        if ((peakVal*newGain >> 10) > 32767)
        {
                newGain = (32767 << 10)/peakVal;
                //! Truncate the ramp time
                ramp = peakPos;
        }

        //! Record the new gain
        obj->gain[slot] = newGain;

        if (!ramp)
                ramp = 1;
        if (!curGain)
                curGain = 1 << 10;
	delta = (newGain - curGain) / (int)ramp;

	ap = audio;
        *clipped = 0;
	for (i = 0; i < count; i++)
	{
		int sample;

		//! Amplify the sample
		sample = *ap*curGain >> 10;
		if (sample < -32768)
		{
			*clipped += -32768 - sample;
			sample = -32768;
		} else if (sample > 32767)
		{
			*clipped += sample - 32767;
			sample = 32767;
		}
		*ap++ = sample;

                //! Adjust the gain
                if (i < ramp)
                        curGain += delta;
                else
                        curGain = newGain;
	}

        obj->pos = slot;
}

