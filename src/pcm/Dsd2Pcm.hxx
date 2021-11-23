/*

Copyright 2009, 2011 Sebastian Gesemann. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY SEBASTIAN GESEMANN ''AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEBASTIAN GESEMANN OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Sebastian Gesemann.

 */

#ifndef DSD2PCM_H_INCLUDED
#define DSD2PCM_H_INCLUDED

#include "ChannelDefs.hxx"

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * A "dsd2pcm engine" for one channel.
 */
class Dsd2Pcm {
	friend class MultiDsd2Pcm;

public:
	/* must be a power of two */
	static constexpr size_t FIFOSIZE = 16;

private:
	/** bit mask for FIFO offsets */
	static constexpr size_t FIFOMASK = FIFOSIZE - 1;

	std::array<uint8_t, FIFOSIZE> fifo;
	size_t fifopos;

public:
	Dsd2Pcm() noexcept {
		Reset();
	}

	/**
	 * resets the internal state for a fresh new stream
	 */
	void Reset() noexcept;

	/**
	 * "translates" a stream of octets to a stream of floats
	 * (8:1 decimation)
	 * @param ctx -- pointer to abstract context (buffers)
	 * @param samples -- number of octets/samples to "translate"
	 * @param src -- pointer to first octet (input)
	 * @param src_stride -- src pointer increment
	 * @param dst -- pointer to first float (output)
	 * @param dst_stride -- dst pointer increment
	 */
	void Translate(size_t samples,
		       const uint8_t *src, ptrdiff_t src_stride,
		       float *dst, ptrdiff_t dst_stride) noexcept;

	void TranslateS24(size_t samples,
			  const uint8_t *src, ptrdiff_t src_stride,
			  int32_t *dst, ptrdiff_t dst_stride) noexcept;

private:
	void ApplySample(size_t ffp, uint8_t src) noexcept;
	float CalcOutputSample(size_t ffp) const noexcept;
	float TranslateSample(size_t ffp, uint8_t src) noexcept;

	int32_t CalcOutputSampleS24(size_t ffp) const noexcept;
	int32_t TranslateSampleS24(size_t ffp, uint8_t src) noexcept;
};

class MultiDsd2Pcm {
	std::array<Dsd2Pcm, MAX_CHANNELS> per_channel;

	size_t fifopos = 0;

public:
	void Reset() noexcept {
		for (auto &i : per_channel)
			i.Reset();
		fifopos = 0;
	}

	void Translate(unsigned channels, size_t n_frames,
		       const uint8_t *src, float *dest) noexcept;

	void TranslateS24(unsigned channels, size_t n_frames,
			  const uint8_t *src, int32_t *dest) noexcept;

private:
	/**
	 * Optimized implementation for the common case.
	 */
	void TranslateStereo(size_t n_frames,
			     const uint8_t *src, float *dest) noexcept;

	void TranslateStereoS24(size_t n_frames,
				const uint8_t *src, int32_t *dest) noexcept;
};

#endif /* include guard DSD2PCM_H_INCLUDED */

