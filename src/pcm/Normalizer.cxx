// SPDX-License-Identifier: LGPL-2.1
// Copyright The Music Player Daemon Project
// Based on AudioCompress (c)2007 busybee (http://beesbuzz.biz/

#include "Normalizer.hxx"
#include "Clamp.hxx"
#include "SampleFormat.hxx"
#include "Traits.hxx"
#include "util/Compiler.h"

#include <algorithm> // for std::fill_n()

void
PcmNormalizer::Reset() noexcept
{
	prev_gain = 0;
	pos = 0;
	std::fill_n(peaks, bufsz, 0);
}

void
PcmNormalizer::ProcessS16(int16_t *gcc_restrict dest,
			  const std::span<const int16_t> src) noexcept
{
	constexpr SampleFormat format = SampleFormat::S16;
	using Traits = SampleTraits<format>;
	using long_type = Traits::long_type;

	constexpr unsigned SHIFT = 10;

	int peakVal = 1;
	std::size_t peakPos = 0;
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

        pos = (pos + 1) % bufsz;
	peaks[pos] = peakVal;

	for (std::size_t i = 0; i < bufsz; i++) {
		if (peaks[i] > peakVal)
		{
			peakVal = peaks[i];
			peakPos = 0;
		}
	}

	//! Determine target gain
	long_type newGain = (1 << SHIFT)*target/peakVal;

        //! Adjust the gain with inertia from the previous gain value
        long_type curGain = prev_gain;
        newGain = (curGain*((1 << smooth) - 1) + newGain) >> smooth;

        //! Make sure it's no more than the maximum gain value
        if (newGain > (maxgain << SHIFT))
                newGain = maxgain << SHIFT;

        //! Make sure it's no less than 1:1
	if (newGain < (1 << SHIFT))
		newGain = 1 << SHIFT;

        //! Make sure the adjusted gain won't cause clipping
        std::size_t ramp = src.size();
        if ((peakVal*newGain >> SHIFT) > Traits::MAX)
        {
                newGain = (Traits::MAX << SHIFT)/peakVal;
                //! Truncate the ramp time
                ramp = peakPos;
        }

        //! Record the new gain
        prev_gain = newGain;

        if (!ramp)
                ramp = 1;
        if (!curGain)
                curGain = 1 << SHIFT;
	const long_type delta = (newGain - curGain) / (long_type)ramp;

	for (const auto sample : src.first(ramp)) {
		//! Amplify the sample
		*dest++ = PcmClamp<format>(sample * curGain >> SHIFT);

                //! Adjust the gain
		curGain += delta;
	}

	curGain = newGain;

	for (const auto sample : src.subspan(ramp)) {
		//! Amplify the sample
		*dest++ = PcmClamp<format>(sample * curGain >> SHIFT);
	}
}
