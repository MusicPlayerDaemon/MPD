/*

Copyright 2009, 2011 Sebastian Gesemann. All rights reserved.

Copyright 2020 Max Kellermann <max.kellermann@gmail.com>

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

#include "Dsd2Pcm.hxx"
#include "Traits.hxx"
#include "util/BitReverse.hxx"
#include "util/GenerateArray.hxx"

#include <cassert>

#include <stdlib.h>
#include <string.h>

/** number of FIR constants */
static constexpr size_t HTAPS = 48;

/** number of "8 MACs" lookup tables */
static constexpr size_t CTABLES = (HTAPS + 7) / 8;

static_assert(Dsd2Pcm::FIFOSIZE * 8 >= HTAPS * 2, "FIFOSIZE too small");

/*
 * Properties of this 96-tap lowpass filter when applied on a signal
 * with sampling rate of 44100*64 Hz:
 *
 * () has a delay of 17 microseconds.
 *
 * () flat response up to 48 kHz
 *
 * () if you downsample afterwards by a factor of 8, the
 *    spectrum below 70 kHz is practically alias-free.
 *
 * () stopband rejection is about 160 dB
 *
 * The coefficient tables ("ctables") take only 6 Kibi Bytes and
 * should fit into a modern processor's fast cache.
 */

/*
 * The 2nd half (48 coeffs) of a 96-tap symmetric lowpass filter
 */
static constexpr double htaps[HTAPS] = {
  0.09950731974056658,
  0.09562845727714668,
  0.08819647126516944,
  0.07782552527068175,
  0.06534876523171299,
  0.05172629311427257,
  0.0379429484910187,
  0.02490921351762261,
  0.0133774746265897,
  0.003883043418804416,
 -0.003284703416210726,
 -0.008080250212687497,
 -0.01067241812471033,
 -0.01139427235000863,
 -0.0106813877974587,
 -0.009007905078766049,
 -0.006828859761015335,
 -0.004535184322001496,
 -0.002425035959059578,
 -0.0006922187080790708,
  0.0005700762133516592,
  0.001353838005269448,
  0.001713709169690937,
  0.001742046839472948,
  0.001545601648013235,
  0.001226696225277855,
  0.0008704322683580222,
  0.0005381636200535649,
  0.000266446345425276,
  7.002968738383528e-05,
 -5.279407053811266e-05,
 -0.0001140625650874684,
 -0.0001304796361231895,
 -0.0001189970287491285,
 -9.396247155265073e-05,
 -6.577634378272832e-05,
 -4.07492895872535e-05,
 -2.17407957554587e-05,
 -9.163058931391722e-06,
 -2.017460145032201e-06,
  1.249721855219005e-06,
  2.166655190537392e-06,
  1.930520892991082e-06,
  1.319400334374195e-06,
  7.410039764949091e-07,
  3.423230509967409e-07,
  1.244182214744588e-07,
  3.130441005359396e-08
};

static constexpr float
CalculateCtableValue(size_t t, int k, int e) noexcept
{
	double acc = 0;
	for (int m = 0; m < k; ++m) {
		acc += (((e >> (7 - m)) & 1) * 2 - 1) * htaps[t * 8 + m];
	}

	return float(acc);
}

static constexpr auto
GenerateCtable(int t) noexcept
{
	int k = HTAPS - t*8;
	if (k>8) k=8;

	const size_t t_ = CTABLES - 1 - t;

	return GenerateArray<256>([t_, k](int e){
		return CalculateCtableValue(t_, k, e);
	});
}

static constexpr auto ctables = GenerateArray<CTABLES>(GenerateCtable);

template<typename Traits=SampleTraits<SampleFormat::S24_P32>>
static constexpr auto
CalculateCtableS24Value(size_t i, size_t j) noexcept
{
	return typename Traits::value_type(ctables[i][j] * Traits::MAX);
}

struct GenerateCtableS24Value {
	size_t i;

	constexpr auto operator()(size_t j) const noexcept {
		return CalculateCtableS24Value(i, j);
	}
};

static constexpr auto
GenerateCtableS24(size_t i) noexcept
{
	return GenerateArray<256>(GenerateCtableS24Value{i});
}

static constexpr auto ctables_s24 = GenerateArray<CTABLES>(GenerateCtableS24);

void
Dsd2Pcm::Reset() noexcept
{
	/* my favorite silence pattern */
	fifo.fill(0x69);

	fifopos = 0;
	/* 0x69 = 01101001
	 * This pattern "on repeat" makes a low energy 352.8 kHz tone
	 * and a high energy 1.0584 MHz tone which should be filtered
	 * out completely by any playback system --> silence
	 */
}

inline void
Dsd2Pcm::ApplySample(size_t ffp, uint8_t src) noexcept
{
	fifo[ffp] = src;
	uint8_t *p = fifo.data() + ((ffp-CTABLES) & FIFOMASK);
	*p = bit_reverse(*p);
}

inline float
Dsd2Pcm::CalcOutputSample(size_t ffp) const noexcept
{
	double acc = 0;
	for (size_t i = 0; i < CTABLES; ++i) {
		uint8_t bite1 = fifo[(ffp              -i) & FIFOMASK];
		uint8_t bite2 = fifo[(ffp-(CTABLES*2-1)+i) & FIFOMASK];
		acc += double(ctables[i][bite1] + ctables[i][bite2]);
	}
	return float(acc);
}

inline float
Dsd2Pcm::TranslateSample(size_t ffp, uint8_t src) noexcept
{
	ApplySample(ffp, src);
	return CalcOutputSample(ffp);
}

inline int32_t
Dsd2Pcm::CalcOutputSampleS24(size_t ffp) const noexcept
{
	int32_t acc = 0;
	for (size_t i = 0; i < CTABLES; ++i) {
		uint8_t bite1 = fifo[(ffp              -i) & FIFOMASK];
		uint8_t bite2 = fifo[(ffp-(CTABLES*2-1)+i) & FIFOMASK];
		acc += ctables_s24[i][bite1] + ctables_s24[i][bite2];
	}
	return acc;
}

inline int32_t
Dsd2Pcm::TranslateSampleS24(size_t ffp, uint8_t src) noexcept
{
	ApplySample(ffp, src);
	return CalcOutputSampleS24(ffp);
}

void
Dsd2Pcm::Translate(size_t samples,
		   const uint8_t *gcc_restrict src, ptrdiff_t src_stride,
		   float *dst, ptrdiff_t dst_stride) noexcept
{
	size_t ffp = fifopos;
	while (samples-- > 0) {
		uint8_t bite1 = *src;
		src += src_stride;
		*dst = TranslateSample(ffp, bite1);
		dst += dst_stride;
		ffp = (ffp + 1) & FIFOMASK;
	}
	fifopos = ffp;
}

void
Dsd2Pcm::TranslateS24(size_t samples,
		      const uint8_t *gcc_restrict src, ptrdiff_t src_stride,
		      int32_t *dst, ptrdiff_t dst_stride) noexcept
{
	size_t ffp = fifopos;
	while (samples-- > 0) {
		uint8_t bite1 = *src;
		src += src_stride;
		*dst = TranslateSampleS24(ffp, bite1);
		dst += dst_stride;
		ffp = (ffp + 1) & FIFOMASK;
	}
	fifopos = ffp;
}

void
MultiDsd2Pcm::Translate(unsigned channels, size_t n_frames,
			const uint8_t *src, float *dest) noexcept
{
	assert(channels <= per_channel.max_size());

	if (channels == 2) {
		TranslateStereo(n_frames, src, dest);
		return;
	}

	for (unsigned i = 0; i < channels; ++i) {
		per_channel[i].Translate(n_frames,
					 src++, channels,
					 dest++, channels);
	}
}

inline void
MultiDsd2Pcm::TranslateStereo(size_t n_frames,
			      const uint8_t *src, float *dest) noexcept
{
	size_t ffp = fifopos;
	while (n_frames-- > 0) {
		*dest++ = per_channel[0].TranslateSample(ffp, *src++);
		*dest++ = per_channel[1].TranslateSample(ffp, *src++);
		ffp = (ffp + 1) & Dsd2Pcm::FIFOMASK;
	}
	fifopos = ffp;
}

void
MultiDsd2Pcm::TranslateS24(unsigned channels, size_t n_frames,
			   const uint8_t *src, int32_t *dest) noexcept
{
	assert(channels <= per_channel.max_size());

	if (channels == 2) {
		TranslateStereoS24(n_frames, src, dest);
		return;
	}

	for (unsigned i = 0; i < channels; ++i) {
		per_channel[i].TranslateS24(n_frames,
					    src++, channels,
					    dest++, channels);
	}
}

inline void
MultiDsd2Pcm::TranslateStereoS24(size_t n_frames,
				 const uint8_t *src, int32_t *dest) noexcept
{
	size_t ffp = fifopos;
	while (n_frames-- > 0) {
		*dest++ = per_channel[0].TranslateSampleS24(ffp, *src++);
		*dest++ = per_channel[1].TranslateSampleS24(ffp, *src++);
		ffp = (ffp + 1) & Dsd2Pcm::FIFOMASK;
	}
	fifopos = ffp;
}
