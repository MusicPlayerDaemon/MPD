// SPDX-License-Identifier: LGPL-2.1
// Copyright The Music Player Daemon Project
// Based on AudioCompress (c)2007 busybee (http://beesbuzz.biz/

#include "Normalizer.hxx"

void
PcmNormalizer::ProcessS16(int16_t *audio, std::size_t count) noexcept
{
        const int slot = (pos + 1) % bufsz;

        int peakVal = 1, peakPos = 0;
	for (std::size_t i = 0; i < count; i++) {
		int val = audio[i];
                if (val < 0)
                        val = -val;
		if (val > peakVal)
                {
			peakVal = val;
                        peakPos = i;
                }
	}
	peaks[slot] = peakVal;

	for (std::size_t i = 0; i < bufsz; i++) {
		if (peaks[i] > peakVal)
		{
			peakVal = peaks[i];
			peakPos = 0;
		}
	}

	//! Determine target gain
	int newGain = (1 << 10)*target/peakVal;

        //! Adjust the gain with inertia from the previous gain value
        int curGain = gain[pos];
        newGain = (curGain*((1 << smooth) - 1) + newGain) >> smooth;

        //! Make sure it's no more than the maximum gain value
        if (newGain > (maxgain << 10))
                newGain = maxgain << 10;

        //! Make sure it's no less than 1:1
	if (newGain < (1 << 10))
		newGain = 1 << 10;

        //! Make sure the adjusted gain won't cause clipping
        std::size_t ramp = count;
        if ((peakVal*newGain >> 10) > 32767)
        {
                newGain = (32767 << 10)/peakVal;
                //! Truncate the ramp time
                ramp = peakPos;
        }

        //! Record the new gain
        gain[slot] = newGain;

        if (!ramp)
                ramp = 1;
        if (!curGain)
                curGain = 1 << 10;
	const int delta = (newGain - curGain) / (int)ramp;

        auto &clipped_ = clipped[slot];
        clipped_ = 0;
	for (std::size_t i = 0; i < count; i++) {
		int sample;

		//! Amplify the sample
		sample = audio[i] * curGain >> 10;
		if (sample < -32768)
		{
			clipped_ += -32768 - sample;
			sample = -32768;
		} else if (sample > 32767)
		{
			clipped_ += sample - 32767;
			sample = 32767;
		}
		audio[i] = sample;

                //! Adjust the gain
                if (i < ramp)
                        curGain += delta;
                else
                        curGain = newGain;
	}

        pos = slot;
}
