// SPDX-License-Identifier: LGPL-2.1
// Copyright The Music Player Daemon Project
// Based on AudioCompress (c)2007 busybee (http://beesbuzz.biz/

#include "Normalizer.hxx"

struct Compressor {
	///! Target level (on a scale of 0-32767)
	static constexpr int target = 16384;

	//! The maximum amount to amplify by
	static constexpr int maxgain = 32;

	//! How much inertia ramping has
	static constexpr int smooth = 8;

        //! History of the peak values
        int *const peaks;

        //! History of the gain values
        int *const gain;

        //! History of clip amounts
        int *const clipped;

        unsigned int pos = 0;
        const unsigned int bufsz;

	Compressor(unsigned history) noexcept
		:peaks(new int[history]{}),
		 gain(new int[history]{}),
		 clipped(new int[history]{}),
		 bufsz(history)
	{
	}

	~Compressor() noexcept {
		delete[] peaks;
		delete[] gain;
		delete[] clipped;
	}
};

struct Compressor *
Compressor_new(unsigned int history) noexcept
{
	return new Compressor(history);
}

void
Compressor_delete(struct Compressor *obj) noexcept
{
	delete obj;
}

void
Compressor_Process_int16(struct Compressor *obj, int16_t *audio,
			 unsigned int count) noexcept
{
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
	newGain = (1 << 10)*obj->target/peakVal;

        //! Adjust the gain with inertia from the previous gain value
        newGain = (curGain*((1 << obj->smooth) - 1) + newGain)
                >> obj->smooth;

        //! Make sure it's no more than the maximum gain value
        if (newGain > (obj->maxgain << 10))
                newGain = obj->maxgain << 10;

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

