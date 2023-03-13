// SPDX-License-Identifier: LGPL-2.1
// Copyright The Music Player Daemon Project
// Based on AudioCompress (c)2007 busybee (http://beesbuzz.biz/

#include "Normalizer.hxx"
#include "Clamp.hxx"
#include "SampleFormat.hxx"
#include "Traits.hxx"
#include "util/Compiler.h"

void
PcmNormalizer::ProcessS16(int16_t *gcc_restrict dest,
			  const std::span<const int16_t> src) noexcept
{
	constexpr SampleFormat format = SampleFormat::S16;
	using Traits = SampleTraits<format>;

        const int slot = (pos + 1) % bufsz;

        int peakVal = 1, peakPos = 0;
	for (std::size_t i = 0; i < src.size(); i++) {
		int val = src[i];
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
        std::size_t ramp = src.size();
        if ((peakVal*newGain >> 10) > Traits::MAX)
        {
                newGain = (Traits::MAX << 10)/peakVal;
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

	for (std::size_t i = 0; i < src.size(); i++) {
		//! Amplify the sample
		*dest++ = PcmClamp<format>(src[i] * curGain >> 10);

                //! Adjust the gain
                if (i < ramp)
                        curGain += delta;
                else
                        curGain = newGain;
	}

        pos = slot;
}
