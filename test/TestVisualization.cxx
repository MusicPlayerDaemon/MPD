/*
 * Copyright 2003-2022 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "output/plugins/visualization/SoundAnalysis.hxx"
#include "output/plugins/visualization/SoundInfoCache.hxx"
#include "output/plugins/visualization/Protocol.hxx"
#include "util/ByteOrder.hxx"

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <math.h>

using namespace Visualization;

// "Smoke test" for SoundInfoCache
TEST(VisualizationTest, SoundInfoCacheSmoke)
{
	using namespace std;
	using namespace std::chrono;

	// Validate a few assumptions I'm making about the API
	AudioFormat std_fmt(44100, SampleFormat::S16, 2);
	EXPECT_EQ(std_fmt.TimeToSize(seconds(1)), 44100 * 2 * 2);
	EXPECT_TRUE(std_fmt.IsFullyDefined());
	EXPECT_TRUE(std_fmt.IsValid());
	EXPECT_EQ(std_fmt.GetFrameSize(), 4);
	EXPECT_EQ(std_fmt.GetSampleRate(), 44100);

	// Whip-up an unrealistic, but easy-to-reason-about audio format for testing
	// purposes: 1Hz, mono, samples are signed bytes
	AudioFormat fmt(1, SampleFormat::S8, 1);
	EXPECT_TRUE(fmt.IsFullyDefined());
	EXPECT_TRUE(fmt.IsValid());

	{
		// Silly case-- a cache that can handle exactly three samples
		Visualization::SoundInfoCache cache(fmt, seconds(3));
		// Add 2 seconds' worth of data
		int8_t data[] = { 1, 2 };
		cache.Add(data, sizeof(data));

		// I now expect to have the following in my three-slot ring buffer:
		//
		// +---+---+---+
		// | 1 | 2 |   |
		// +---+---+---+
		//	 ^		 ^
		//	 p0		 p1

		EXPECT_EQ(cache.Size(), 2);

		int8_t buf[3];
		bool status = cache.GetFromBeginning(2, buf, sizeof(buf));
		EXPECT_TRUE(status);
		EXPECT_EQ(buf[0], 1);
		EXPECT_EQ(buf[1], 2);

		data[0] = 3; data[1] = 4;
		cache.Add(data, sizeof(data));

		// I now expect to have the following in my three-slot ring buffer:
		//
		// +---+---+---+
		// | 4 | 2 | 3 |
		// +---+---+---+
		//		 ^
		//		p0,p1

		EXPECT_EQ(cache.Size(), 3);

		status = cache.GetFromBeginning(3, buf, sizeof(buf));
		EXPECT_TRUE(status);
		EXPECT_EQ(buf[0], 2);
		EXPECT_EQ(buf[1], 3);
		EXPECT_EQ(buf[2], 4);

		data[0] = 5;
		cache.Add(data, 1);

		// I now expect to have the following in my three-slot ring buffer:
		//
		// +---+---+---+
		// | 4 | 5 | 3 |
		// +---+---+---+
		//			 ^
		//			 p0,p1

		EXPECT_EQ(cache.Size(), 3);

		status = cache.GetFromBeginning(3, buf, sizeof(buf));
		EXPECT_TRUE(status);
		EXPECT_EQ(buf[0], 3);
		EXPECT_EQ(buf[1], 4);
		EXPECT_EQ(buf[2], 5);

		int8_t data3[] = { 6, 7, 8 };
		cache.Add(data3, 3);

		// I now expect to have the following in my three-slot ring buffer:
		//
		// +---+---+---+
		// | 7 | 8 | 6 |
		// +---+---+---+
		//			 ^
		//			 p0,p1

		EXPECT_EQ(cache.Size(), 3);

		status = cache.GetFromBeginning(3, buf, sizeof(buf));
		EXPECT_TRUE(status);
		EXPECT_EQ(buf[0], 6);
		EXPECT_EQ(buf[1], 7);
		EXPECT_EQ(buf[2], 8);

		int8_t data4[] = { 9, 10, 11, 12 };
		cache.Add(data4, 4);

		// I now expect to have the following in my three-slot ring buffer:
		//
		// +----+----+----+
		// | 10 | 11 | 12 |
		// +----+----+----+
		//	 ^
		//	 p0,p1

		EXPECT_EQ(cache.Size(), 3);

		status = cache.GetFromBeginning(3, buf, sizeof(buf));
		EXPECT_TRUE(status);
		EXPECT_EQ(buf[0], 10);
		EXPECT_EQ(buf[1], 11);
		EXPECT_EQ(buf[2], 12);
	}
}

// Test SoundInfoCache WRT timing
TEST(VisualizationTest, SoundInfoCacheTiming)
{
	using namespace std;
	using namespace std::chrono;

	// Whip-up an unrealistic, but easy-to-reason-about audio format for testing purposes:
	// 1Hz, mono, samples are signed bytes (i.e. 1 byte per sample
	AudioFormat fmt(1, SampleFormat::S8, 1);
	EXPECT_TRUE(fmt.IsFullyDefined());

	// Silly case-- a cache that can handle exactly three samples
	Visualization::SoundInfoCache cache(fmt, seconds(3));
	// Add 2 seconds' worth of data
	int8_t data[] = { 1, 2 };
	cache.Add(data, sizeof(data));

	// I now expect to have the following in my three-slot ring buffer:
	//
	// +---+---+---+
	// | 1 | 2 |   |
	// +---+---+---+
	//   ^       ^
	//  p0       p1
	//  t0       t1 = t0 + 2 seconds
	//
	// I don't know what t0 is (it will be different every time this test is
	// run), but t1 should be two seconds later than t0.
	Visualization::SoundInfoCache::Time t0, t1;
	tie(t0, t1) = cache.Range();
	EXPECT_EQ(t1 - t0, seconds(2));

	int8_t buf[3];
	bool status = cache.GetByTime(2, t1, buf, sizeof(buf));
	EXPECT_TRUE(status);

	EXPECT_EQ(buf[0], 1);
	EXPECT_EQ(buf[1], 2);

	// Add 1 second's worth of data
	data[0] = 3;
	cache.Add(data, 1);

	// I now expect to have the following in my three-slot ring buffer:
	//
	// +---+---+---+
	// | 1 | 2 | 3 |
	// +---+---+---+
	//   ^
	//   p0, p1
	//   t0
	//   t1 = t0 + 3 seconds
	//
	// I don't know what t0 is (it will be different every time this test is
	// run), but t1 should be three seconds later than t0.
	tie(t0, t1) = cache.Range();
	EXPECT_EQ(t1 - t0, seconds(3));

	status = cache.GetByTime(3, t1, buf, sizeof(buf));
	EXPECT_TRUE(status);

	EXPECT_EQ(buf[0], 1);
	EXPECT_EQ(buf[1], 2);
	EXPECT_EQ(buf[2], 3);

	// Add 1 second's worth of data
	data[0] = 4;
	cache.Add(data, 1);

	// I now expect to have the following in my three-slot ring buffer:
	//
	// +---+---+---+
	// | 4 | 2 | 3 |
	// +---+---+---+
	//       ^
	//       p0, p1
	//       t0
	//       t1 = t0 + 3 seconds
	//
	// I don't know what t0 is (it will be different every time this test is
	// run), but t1 should be three seconds later than t0.
	tie(t0, t1) = cache.Range();
	EXPECT_EQ(t1 - t0, seconds(3));

	status = cache.GetByTime(3, t1, buf, sizeof(buf));
	EXPECT_TRUE(status);

	EXPECT_EQ(buf[0], 2);
	EXPECT_EQ(buf[1], 3);
	EXPECT_EQ(buf[2], 4);

	// Add another second's worth of data
	data[0] = 5;
	cache.Add(data, 1);

	// I now expect to have the following in my three-slot ring buffer:
	//
	// +---+---+---+
	// | 4 | 5 | 3 |
	// +---+---+---+
	//           ^
	//           p0, p1
	//           t0
	//           t1 = t0 + 3 seconds
	//
	// I don't know what t0 is (it will be different every time this test is
	// run), but t1 should be three seconds later than t0.
	tie(t0, t1) = cache.Range();
	EXPECT_EQ(t1 - t0, seconds(3));

	// Add 2 seconds' worth of data
	data[0] = 6; data[1] = 7;
	cache.Add(data, 2);

	// I now expect to have the following in my three-slot ring buffer:
	//
	// +---+---+---+
	// | 7 | 5 | 6 |
	// +---+---+---+
	//       ^
	//       p0, p1
	//       t0
	//       t1 = t0 + 3 seconds

	tie(t0, t1) = cache.Range();
	EXPECT_EQ(t1 - t0, seconds(3)); // 3 secs in the buffer

	// Ask for two samples, ending at `t1`
	status = cache.GetByTime(2, t1, buf, sizeof(buf));
	EXPECT_TRUE(status);

	EXPECT_EQ(buf[0], 6);
	EXPECT_EQ(buf[1], 7);

	// Let's try fractions-- at this point, we've got 3 second's worth of
	// data in the cache, from [t0, t1 = t0 + 3 seconds).

	// What happens if we ask for two samples, ending at t0 + 2500ms?
	auto d = milliseconds{2500}; // Should be 2500ms = 2.5sec
	auto t = t0 + d;

	status = cache.GetByTime(3, t, buf, sizeof(buf));
	EXPECT_TRUE(status);
	EXPECT_EQ(buf[0], 5);
	EXPECT_EQ(buf[1], 6);
	EXPECT_EQ(buf[2], 7);

	status = cache.GetByTime(2, t0 + milliseconds(1500), buf, sizeof(buf));
	EXPECT_TRUE(status);
	EXPECT_EQ(buf[0], 5);
	EXPECT_EQ(buf[1], 6);

	status = cache.GetByTime(1, t0 + milliseconds(500), buf, sizeof(buf));
	EXPECT_TRUE(status);
	EXPECT_EQ(buf[0], 5);

	// Negative tests-- what happens if I ask for _two_ samples at t0 + 500ms--
	// we can't satisfy that request
	status = cache.GetByTime(2, t0 + milliseconds(500), buf, sizeof(buf));
	EXPECT_FALSE(status);

	// What if I ask for even one sample at t1 + 1ms
	status = cache.GetByTime(1, t1 + milliseconds(1), buf, sizeof(buf));
	EXPECT_FALSE(status);
}

// Exercise SoundInfoCache on a more realistic waveform
TEST(VisualizationTest, Waveform)
{
	using namespace std;
	using namespace std::chrono;

	const double TWO_PI = 6.283185307179586476925286766559;

	// Let's generate a waveform for a 1Hz sine wave, sampled at 44100 samples
	// per second. Using format 44100:16:2, that's just over 172Kb (i.e. not too
	// bad).
	AudioFormat fmt(44100, SampleFormat::S16, 2);
	EXPECT_TRUE(fmt.IsFullyDefined());

	int16_t buf[44100 * 2];
	for (int i = 0; i < 44100; ++i) {
		double t = (double)i / 44100.0;
		int16_t v = (int16_t) (sin(TWO_PI * t) * 32767.0);
		buf[i * 2] = buf[i * 2 + 1] = v;
	}

	// Create a `SoundInfoCache` instance that can hold 5 seconds' worth of
	// such data...
	Visualization::SoundInfoCache cache(fmt, seconds(5));
	// and add 6 seconds' worth of data to it.
	cache.Add(buf, sizeof(buf));
	Visualization::SoundInfoCache::Time t0, t1;
	tie(t0, t1) = cache.Range();
	EXPECT_EQ(t1 - t0, seconds(1));
	cache.Add(buf, sizeof(buf));
	cache.Add(buf, sizeof(buf));
	cache.Add(buf, sizeof(buf));
	cache.Add(buf, sizeof(buf));
	cache.Add(buf, sizeof(buf));

	// I should now have five seconds' worth of data in the cache.
	Visualization::SoundInfoCache::Time t2, t3;
	tie(t2, t3) = cache.Range();
	EXPECT_EQ(t3 - t0, seconds(6));

	// But we're at "song time" = 6 seconds
	bool status = cache.GetByTime(100, t0 + seconds(6), buf, sizeof(buf));
	EXPECT_TRUE(status);

	// `buf[0:100]` should now contain the *last* 100 samples
	for (int i = 0; i < 100; ++i) {
		EXPECT_EQ(buf[2*i], buf[88000 + 2*i]);
	}
}

/**
 * \page vis_out_trivial_sample Trivial Waveforms for Testing Purposes
 *
 * \section vis_out_trivial_sample_intro Introduction
 *
 * Derivation of a trivial DFT for testing purposes.
 *
 * \section vis_out_trivial_sample_derivation Derivation
 *
 * Consider the waveform:
 *
 \code
                   1
   f(x) = sin(x) + - cos(2x)
                   2
 \endcode
 *
 * This function has a (continuous) Fourier transform of:
 *
 \code
  1                                               1
  - pi d(w - 2) - i pi d(w - 1) + i pi d(w + 1) + - pi d(w + 2)
  2                                               2
 \endcode
 *
 * where \c d denotes the dirac delta function and \c w represents the angular
 * momentum. This makes sense: the frequency domain has "spikes" at frequencies
 * of 1 & 2 (corresponding to the sin & cos arguments, respectively), and the
 * "burst" at a frequency of 1 is twice as strong as that at 2 (corresponding to
 * the sin & cos coefficients, resp.).
 *
 * Let's add a second waveform (so we can simulate stereo):
 *
 \code
                   1
  g(x) = sin(2x) + - cos(4x)
                   4
 \endcode
 *
 * The Fourier transform of \c g is:
 *
 \code
  1                                         1
  - pi d(w-4) - i pi d(w-2) + i pi d(w+2) + - pi d(w+4)
  4                                         4
 \endcode
 *
 * Similarly: we see spikes at 2 & 4, with the spike at 2 four times the size of
 * the spike at 4.
 *
 * \subsection vis_out_trivial_sample_derivation_octave Gnu Octave Code
 *
 \code

  octave:1> pkg load symbolic
  octave:2> syms x
  octave:3> f = sin (x) + 1/2 * cos (2*x)
  octave:4> fourier (f)
  ans = (sym)

    π⋅δ(w - 2)                                 π⋅δ(w + 2)
    ────────── - ⅈ⋅π⋅δ(w - 1) + ⅈ⋅π⋅δ(w + 1) + ──────────
        2                                          2
  octave:5> g = sin (2*x) + 1/4 * cos (4*x)
  octave:6> fourier (g)
  ans = (sym)
    π⋅δ(w - 4)                                 π⋅δ(w + 4)
    ────────── - ⅈ⋅π⋅δ(w - 2) + ⅈ⋅π⋅δ(w + 2) + ──────────
        4                                          4
 \endcode
 *
 * \subsection vis_out_trivial_sample_derivation_wolfram Wolfram Language
 *
 \code

 FourierTransform[Sin[x]+1/2 Cos[2x],x, \[Omega], FourierParameters -> {1,-1}]
 = 1/2 \[Pi] DiracDelta[-2+\[Omega]]-I \[Pi] DiracDelta[-1+\[Omega]]+I \[Pi] DiracDelta[1+\[Omega]]+1/2 \[Pi] DiracDelta[2+\[Omega]]

  FourierTransform[Sin[2x]+1/4 Cos[4x],x, \[Omega], FourierParameters -> {1,-1}]
  = 1/4 \[Pi] DiracDelta[-4 + \[Omega]] -
 I \[Pi] DiracDelta[-2 + \[Omega]] +
 I \[Pi] DiracDelta[2 + \[Omega]] + 1/4 \[Pi] DiracDelta[4 + \[Omega]]

 \endcode
 *
 * \subsection vis_out_trivial_sample_dfts Discrete Fourier Transforms
 *
 * Let's sample these waveforms at 5 points over the range 0 to 2Pi: that's far
 * too low a sampling rate to see much of anything, but it \em is simple enough
 * that we can compute the discrete Fourier tranform by hand for testing
 * purposes (we'll use a more realistic sampling rate later; right now we just
 * want to check our basic calculations).
 *
 * At the same time, for convenience, let's introduce a transformation so that
 * we can tell the codebase that we're sampling once per second (since 2*pi/5 is
 * around 1.2566 and AudioFormat only accepts integers for the sample rate).
 * Let x = pi * u /2, and we'll work in terms of u:
 *
 \code

  i   u      x = u * pi/2    f(y)  g(y)
  --  -      ------------    ----  ----
  0   0  sec 0               1/2   1/4
  1   1      Pi/2            1/2   1/4
  2   2      Pi              1/2   1/4
  3   3      3*Pi/2          -3/2  1/4
  4   4      2*Pi            1/2   1/4

 \endcode
 *
 * \subsubsection vis_out_trivial_sample_f
 *
 * Let's work out the Fourier coefficients "by hand". Let the k-th discrete
 * Fourier coefficient for f be Y(k) and let the summing index for each
 * coefficient be k:
 *
 \code

  k   j =>        0                  1                 2                 3                 4
  |
  v        1  -2pi*0*0*i/5    1  -2pi*1*0*i/5   1  -2pi*2*0*i/5   3  -2pi*3*0*i/5   1  -2pi*4*0*i/5
      Y  = - e              + - e             + - e             - - e             + - e
  0    0   2                  2                 2                 2                 2

           1  -2pi*0*1*i/5    1  -2pi*1*1*i/5   1  -2pi*2*1*i/5   3  -2pi*3*1*i/5   1  -2pi*4*1*i/5
  1   Y  = - e              + - e             + - e             - - e             + - e
       1   2                  2                 2                 2                 2

           1  -2pi*0*2*i/5    1  -2pi*1*2*i/5   1  -2pi*2*2*i/5   3  -2pi*3*2*i/5   1  -2pi*4*2*i/5
  2   Y  = - e              + - e             + - e             - - e             + - e
       2   2                  2                 2                 2                 2

           1  -2pi*0*3*i/5    1  -2pi*1*3*i/5   1  -2pi*2*3*i/5   3  -2pi*3*3*i/5   1  -2pi*4*3*i/5
  3   Y  = - e              + - e             + - e             - - e             + - e
       3   2                  2                 2                 2                 2

           1  -2pi*0*4*i/5    1  -2pi*1*4*i/5   1  -2pi*2*4*i/5   3  -2pi*3*4*i/5   1  -2pi*4*4*i/5
  4   Y  = - e              + - e             + - e             - - e             + - e
       4   2                  2                 2                 2                 2

 \endcode
 *
 * OK-- time to let Octave take over:
 *
 \code

  vpa(1/sym(2)*exp(-sym(2)*sym(pi)*0*    0 *i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*1*    0 *i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(2)*    0 *i/sym(5)) - sym(3)/sym(2)*exp(-sym(2)*sym(pi)*sym(3)*    0* i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(4)*    0 *i/sym(5)))
  vpa(1/sym(2)*exp(-sym(2)*sym(pi)*0*    1 *i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*1*    1 *i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(2)*    1 *i/sym(5)) - sym(3)/sym(2)*exp(-sym(2)*sym(pi)*sym(3)*    1* i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(4)*    1 *i/sym(5)))
  vpa(1/sym(2)*exp(-sym(2)*sym(pi)*0*sym(2)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*1*sym(2)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(2)*sym(2)*i/sym(5)) - sym(3)/sym(2)*exp(-sym(2)*sym(pi)*sym(3)*sym(2)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(4)*sym(2)*i/sym(5)))
  vpa(1/sym(2)*exp(-sym(2)*sym(pi)*0*sym(3)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*1*sym(3)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(2)*sym(3)*i/sym(5)) - sym(3)/sym(2)*exp(-sym(2)*sym(pi)*sym(3)*sym(3)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(4)*sym(3)*i/sym(5)))
  vpa(1/sym(2)*exp(-sym(2)*sym(pi)*0*sym(4)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*1*sym(4)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(2)*sym(4)*i/sym(5)) - sym(3)/sym(2)*exp(-sym(2)*sym(pi)*sym(3)*sym(4)*i/sym(5)) + 1/sym(2)*exp(-sym(2)*sym(pi)*sym(4)*sym(4)*i/sym(5)))

  ans = (sym)  0.5000000000000000000000000000000
  ans = (sym)  1.6180339887498948482045868343656  - 1.1755705045849462583374119092781⋅ⅈ
  ans = (sym) -0.61803398874989484820458683436564 + 1.9021130325903071442328786667588⋅ⅈ
  ans = (sym) -0.61803398874989484820458683436564 - 1.9021130325903071442328786667588⋅ⅈ
  ans = (sym)  1.6180339887498948482045868343656  + 1.1755705045849462583374119092781⋅ⅈ

 \endcode
 *
 * Let's confirm with Mathematica:
 *
 \code

 In[5]:= Fourier[{1/2,1/2,1/2,-3/2,1/2}, FourierParameters -> {1,-1}]
 Out[5]= {0.5 +0. I, 1.61803 -1.17557 I, -0.618034+1.90211 I, -0.618034-1.90211 I, 1.61803 +1.17557 I}

 \endcode
 *
 * \subsubsection vis_out_trivial_sample_g
 *
 \code

  k   j =>        0                  1                 2                 3                 4
  |
  v        1  -2pi*0*0*i/5    1  -2pi*1*0*i/5   1  -2pi*2*0*i/5   1  -2pi*3*0*i/5   1  -2pi*4*0*i/5
      Y  = - e              + - e             + - e             + - e             + - e
  0    0   4                  4                 4                 4                 4

           1  -2pi*0*1*i/5    1  -2pi*1*1*i/5   1  -2pi*2*1*i/5   1  -2pi*3*1*i/5   1  -2pi*4*1*i/5
  1   Y  = - e              + - e             + - e             + - e             + - e
       1   4                  4                 4                 4                 4

           1  -2pi*0*2*i/5    1  -2pi*1*2*i/5   1  -2pi*2*2*i/5   1  -2pi*3*2*i/5   1  -2pi*4*2*i/5
  2   Y  = - e              + - e             + - e             + - e             + - e
       2   4                  4                 4                 4                 4

           1  -2pi*0*3*i/5    1  -2pi*1*3*i/5   1  -2pi*2*3*i/5   1  -2pi*3*3*i/5   1  -2pi*4*3*i/5
  3   Y  = - e              + - e             + - e             + - e             + - e
       3   4                  4                 4                 4                 4

           1  -2pi*0*4*i/5    1  -2pi*1*4*i/5   1  -2pi*2*4*i/5   1  -2pi*3*4*i/5   1  -2pi*4*4*i/5
  4   Y  = - e              + - e             + - e             + - e             + - e
       4   4                  4                 4                 4                 4

 \endcode
 *
 \code

  vpa(1/sym(4)*exp(-sym(2)*sym(pi)*0*    0 *i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*1*    0 *i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(2)*    0 *i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(3)*    0* i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(4)*    0 *i/sym(5)))
  vpa(1/sym(4)*exp(-sym(2)*sym(pi)*0*    1 *i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*1*    1 *i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(2)*    1 *i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(3)*    1* i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(4)*    1 *i/sym(5)))
  vpa(1/sym(4)*exp(-sym(2)*sym(pi)*0*sym(2)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*1*sym(2)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(2)*sym(2)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(3)*sym(2)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(4)*sym(2)*i/sym(5)))
  vpa(1/sym(4)*exp(-sym(2)*sym(pi)*0*sym(3)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*1*sym(3)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(2)*sym(3)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(3)*sym(3)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(4)*sym(3)*i/sym(5)))
  vpa(1/sym(4)*exp(-sym(2)*sym(pi)*0*sym(4)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*1*sym(4)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(2)*sym(4)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(3)*sym(4)*i/sym(5)) + 1/sym(4)*exp(-sym(2)*sym(pi)*sym(4)*sym(4)*i/sym(5)))

  ans = (sym) 1.2500000000000000000000000000000
  ans = (sym) 0.e-142 + 0.e-142⋅ⅈ
  ans = (sym) 0.e-142 + 0.e-142⋅ⅈ
  ans = (sym) 0.e-142 + 0.e-142⋅ⅈ
  ans = (sym) 0.e-142 + 0.e-142⋅ⅈ

 \endcode
 *
 * Again, let's confirm with Mathematica:
 *
 \code

 In[6]:= Fourier[{1/4,1/4,1/4,1/4,1/4}, FourierParameters -> {1,-1}]
 Out[6]= {1.25, 5.55112*10^-17, 5.55112*10^-17, 5.55112*10^-17, 5.55112*10^-17}

 \endcode
 *
 *
 */

// Read the four bytes at \p as a float in the network protocol
inline float float_at(std::byte *p, size_t i) {
	uint32_t as_uint = FromBE32(*(uint32_t*)(p + 4*i));
	return *(float*)&as_uint;
}

// Test SoundAnalaysis against a trivial DFT
TEST(VisualizationTest, TrivialDft)
{
	using namespace std::chrono;

	// Let's represent our wave form as IEEE 754 single precisions floats,
	// sampled once per second, with two channels (i.e. stereo).
	AudioFormat fmt(1, SampleFormat::FLOAT, 2);

	// Sanity check-- 20 bytes is 5 samples, which should be five seconds'
	// worth. Double for the two channels
	Visualization::SoundInfoCache::Duration us = fmt.SizeToTime<Visualization::SoundInfoCache::Duration>(40);
	EXPECT_EQ(us, seconds(5));

	constexpr float samples[10] = { 0.5, 0.25, 0.5, 0.25, 0.5, 0.25, -1.5, 0.25, 0.5, 0.25 };
	std::shared_ptr<Visualization::SoundInfoCache> pcache =
		std::make_unique<Visualization::SoundInfoCache>(fmt, seconds(6)); // six seconds' capacity, just so we
										   // don't need to worry
	pcache->Add(samples, sizeof(samples));
	EXPECT_EQ(pcache->Size(), 40);

	Visualization::SoundInfoCache::Time t0, t1;
	std::tie(t0, t1) = pcache->Range();
	// `t0` is whatever time the first sample was added; what we know is that
	// `t1` should be five seconds later.
	auto d = t1 - t0;
	EXPECT_EQ(d, seconds(5));

	// For each channel, we'll get back five Fourier coefficients, corresponding
	// to the frequencies 0Hz, 1/5Hz, 2/5, 3/5 & 4/5. Let's pick cutoffs that
	// will discard the highest & the lowest, just for testing purposes.
	SoundAnalysisParameters params { 5, 0.25, 0.75 };
	SoundAnalysis analysis(params, pcache);

	EXPECT_EQ(2, analysis.NumChan());
	EXPECT_EQ(5, analysis.NumSamp());
	EXPECT_EQ(3, analysis.NumFreq());

	EXPECT_TRUE(analysis.Update(t1));

	// Three coefficients per channel, two channels
	fftwf_complex coeffs[6];
	analysis.GetCoeffs(coeffs, sizeof(coeffs));

	EXPECT_FLOAT_EQ(coeffs[0][0],  0.5);
	EXPECT_FLOAT_EQ(coeffs[0][1],  0.0);
	EXPECT_FLOAT_EQ(coeffs[1][0],  1.6180339887498948482045868343656);
	EXPECT_FLOAT_EQ(coeffs[1][1], -1.1755705045849462583374119092781);
	EXPECT_FLOAT_EQ(coeffs[2][0], -0.61803398874989484820458683436564);
	EXPECT_FLOAT_EQ(coeffs[2][1],  1.9021130325903071442328786667588);

	EXPECT_FLOAT_EQ(coeffs[3][0],  1.25);
	EXPECT_FLOAT_EQ(coeffs[3][1],  0.0);
	EXPECT_FLOAT_EQ(coeffs[4][0],  0.0);
	EXPECT_FLOAT_EQ(coeffs[4][0],  0.0);
	EXPECT_FLOAT_EQ(coeffs[5][0],  0.0);
	EXPECT_FLOAT_EQ(coeffs[5][0],  0.0);

	// bass/mids/trebs: 0/2/4 (left)
	// bass/mids/trebs: 0/0/0 (right)

	float bmt[6];
	EXPECT_TRUE(analysis.GetBassMidsTrebs(bmt, 6));

	EXPECT_FLOAT_EQ(bmt[0], 0.0);
	EXPECT_FLOAT_EQ(bmt[1], 2.0);
	EXPECT_FLOAT_EQ(bmt[2], 4.0);
	EXPECT_FLOAT_EQ(bmt[3], 0.0);
	EXPECT_FLOAT_EQ(bmt[4], 0.0);
	EXPECT_FLOAT_EQ(bmt[5], 0.0);

	// Serialization:
	//
	// +----------+----------+-------------+-----------+----------+---------+---------+----------+------------+---------------+-----------------+
	// | num_samp | num_chan | sample_rate | waveforms | num_freq | freq_lo | freq_hi | freq_off |   coeffs   | power_spectra | bass/mids/trebs |
	// | -------- | -------- | ----------- | --------- | -------- | ------- | ------- | -------- | ---------- | ------------- | --------------- |
	// | uint16_t |  uint8_t |  uint16_t   | see below | uint16_t |  float  |  float  | uint16_t | see below  |  see below    | see below       |
	// |     0005 |       02 |      0001   |           |      003 |    0.25 |    0.75 |     0001 |            |               |                 |
	// +----------+----------+-------------+-----------+----------+---------+---------+----------+------------+---------------+-----------------+
	//          2          1             2       40             2         4        4           2           48              24   24
	//	153 octets, total

	// waveforms:
	// chan 0: 0.5, 0.5 0.5 -1.5, 0.5
	// chan 1: 0.25 0.25 0.25 0.25, 0.25

	// coeffs:
	// chan 0: (1.6180339887498948482045868343656, -1.1755705045849462583374119092781), (-0.61803398874989484820458683436564, 1.9021130325903071442328786667588) (-0.61803398874989484820458683436564, 1.9021130325903071442328786667588)
	// chan 1: (0.0, 0.0) (0.0, 0.0) (0.0, 0.0)

	// spectra:
	// chan 0: 2, 2, 2
	// chan 1: 0, 0, 0

	std::byte buf[153 + 8]; // 4 bytes before & 4 bytes after
	std::fill(buf, buf + 153 + 8, std::byte{0xef});
	std::byte *p1 = analysis.SerializeSoundInfoFramePayload(buf + 4);
	std::byte *p0 = buf + 4;
	EXPECT_EQ(p1, p0 + 153);
	EXPECT_EQ(buf[0], std::byte{0xef});
	EXPECT_EQ(buf[1], std::byte{0xef});
	EXPECT_EQ(buf[2], std::byte{0xef});
	EXPECT_EQ(buf[3], std::byte{0xef});

	EXPECT_EQ(buf[157], std::byte{0xef});
	EXPECT_EQ(buf[158], std::byte{0xef});
	EXPECT_EQ(buf[159], std::byte{0xef});
	EXPECT_EQ(buf[160], std::byte{0xef});

	EXPECT_EQ(FromBE16(*(uint16_t*)p0), 5); p0 += 2; // num_samp := 5
	EXPECT_EQ(*p0, (std::byte)2);		p0 += 1; // num_chan := 2
	EXPECT_EQ(FromBE16(*(uint16_t*)p0), 1); p0 += 2; // sample_rate := 1

	// waveform, channel 0
	EXPECT_FLOAT_EQ(float_at(p0, 0),  0.5);
	EXPECT_FLOAT_EQ(float_at(p0, 1),  0.5);
	EXPECT_FLOAT_EQ(float_at(p0, 2),  0.5);
	EXPECT_FLOAT_EQ(float_at(p0, 3), -1.5);
	EXPECT_FLOAT_EQ(float_at(p0, 4),  0.5);
	p0 += 20;

	// waveform, channel 1
	EXPECT_FLOAT_EQ(float_at(p0, 0), 0.25);
	EXPECT_FLOAT_EQ(float_at(p0, 1), 0.25);
	EXPECT_FLOAT_EQ(float_at(p0, 2), 0.25);
	EXPECT_FLOAT_EQ(float_at(p0, 3), 0.25);
	EXPECT_FLOAT_EQ(float_at(p0, 4), 0.25);
	p0 += 20;

	EXPECT_EQ(FromBE16(*(uint16_t*)p0), 3); p0 += 2; // num_freq := 3

	EXPECT_FLOAT_EQ(float_at(p0, 0), 0.25); // freq_lo
	EXPECT_FLOAT_EQ(float_at(p0, 1), 0.75); // freq_hi
	p0 += 8;

	EXPECT_EQ(FromBE16(*(uint16_t*)p0), 1); p0 += 2; // freq_off

	// coefficients, channel 0
	EXPECT_FLOAT_EQ(float_at(p0, 0),  1.6180339887498948482045868343656);
	EXPECT_FLOAT_EQ(float_at(p0, 1), -1.1755705045849462583374119092781);
	EXPECT_FLOAT_EQ(float_at(p0, 2), -0.61803398874989484820458683436564);
	EXPECT_FLOAT_EQ(float_at(p0, 3),  1.9021130325903071442328786667588);
	EXPECT_FLOAT_EQ(float_at(p0, 4), -0.61803398874989484820458683436564);
	EXPECT_FLOAT_EQ(float_at(p0, 5), -1.9021130325903071442328786667588);
	p0 += 24;

	// For small quantities, absolute error is more reliable than relative.
	// The problem is choosing a threshold appropriately. On Linux (Arch &
	// Ubuntu, at least), the tests pass with a fairly tight threshold
	// (1.0e-43f).  However, to get the tests to pass on MacOS, we need to
	// loosen this considerably (different hardware on the Github action
	// runners, perhaps).
	const float ZERO_THRESH = 1.0e-9f;

	// coefficients, channel 1
	EXPECT_NEAR(float_at(p0, 0), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 1), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 2), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 3), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 4), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 5), 0.0, ZERO_THRESH);
	p0 += 24;

	// coefficients, channel 2
	EXPECT_FLOAT_EQ(float_at(p0, 0), 2.0);
	EXPECT_FLOAT_EQ(float_at(p0, 1), 2.0);
	EXPECT_FLOAT_EQ(float_at(p0, 2), 2.0);
	EXPECT_NEAR(float_at(p0, 3), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 4), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 5), 0.0, ZERO_THRESH);
	p0 += 24;

	// bass/mids/trebs

	EXPECT_NEAR(float_at(p0, 0), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 1), 2.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 2), 4.0, ZERO_THRESH);
	p0 += 12;

	EXPECT_NEAR(float_at(p0, 0), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 1), 0.0, ZERO_THRESH);
	EXPECT_NEAR(float_at(p0, 2), 0.0, ZERO_THRESH);
	p0 += 12;
}

// Now let's try a more realistic sampling rate
TEST(VisualizationTest, SinesAndCosines)
{
	using namespace std::chrono;

	const float TWO = 2.f;
	const float FOUR = 4.f;

	// Everything below is driven off `NUM_SAMP`-- the higher this number is,
	// the closer we'll get to a dirac delta function at these functions'
	// frequencies.
	const size_t NUM_SAMP = /*101*/ /*513*/ 1025;

	const size_t NUM_COEFF = (NUM_SAMP / 2) + 1;
	const size_t SAMPLE_RATE_HZ = size_t((float)NUM_SAMP / 6.28318531f) + 1;

	// Just for fun (and better test coverage) we'll represent our waveforms as
	// signed 16-bit integers, sampled at ceil(num_samp/2/Pi,) with two channels
	// (i.e. stereo).
	AudioFormat fmt(SAMPLE_RATE_HZ, SampleFormat::S16, 2);

	// Let's sample over the entire period of these functions (2Pi =~ 6.28)
	std::shared_ptr<Visualization::SoundInfoCache> pcache =
		std::make_shared<Visualization::SoundInfoCache>(fmt, seconds(7));

	// Sample the functions over all of [0, 2*Pi), so the DFT has a chance
	// to "see" all the frequencies in one period of each function.
	int16_t samples[SAMPLE_RATE_HZ * 2];
	// We sample the waveforms one second at a time, filling-up the cache as we
	// go:
	for (size_t i = 0; i < 7; ++i) {
		for (size_t j = 0; j < SAMPLE_RATE_HZ; ++j) {
			float x = (float)i + float(j) / (float)SAMPLE_RATE_HZ;
			float f = sin(x) + cos(TWO * x) / TWO;
			float g = sin(TWO * x) + cos(FOUR * x) / FOUR;

			// -1.5 <= f <= 0.75 (approx), & -1.25 <= g <= 0.75 (approx), so
			// -let's scale f & g.
			samples[2 * j	 ] = (int16_t)(f * 1024.f);
			samples[2 * j + 1] = (int16_t)(g * 1024.f);
		}
		pcache->Add(samples, sizeof(samples));
	}

	Visualization::SoundInfoCache::Time t0, t1;
	std::tie(t0, t1) = pcache->Range();

	// Quick sanity check-- `t0` is whatever time the first sample was added;
	// what we *do* know is that `t1` should be seven seconds later.
	auto d = t1 - t0;
	EXPECT_EQ(d, seconds(7));

	// OK-- compute the DFT:
	SoundAnalysisParameters params(NUM_SAMP, 0.f, 20000.f);
	SoundAnalysis analysis(params, pcache);

	EXPECT_TRUE(analysis.Update(t1));
	fftwf_complex coeffs[2 * NUM_COEFF];
	EXPECT_TRUE(analysis.GetCoeffs(coeffs, sizeof(coeffs)));

	float spectra[2 * NUM_COEFF];
	for (size_t i = 0; i < NUM_COEFF; ++i) {
		float mag_left = sqrt(coeffs[i][0] * coeffs[i][0] + coeffs[i][1] * coeffs[i][1]);
		spectra[i] = mag_left > 1.0f ? mag_left : 0.f; // threshold

		float mag_right = sqrt(coeffs[NUM_COEFF + i][0] * coeffs[NUM_COEFF + i][0] +
							   coeffs[NUM_COEFF + i][1] * coeffs[NUM_COEFF + i][1]);
		spectra[NUM_COEFF + i] = mag_right > 1.0f ? mag_right : 0.f; // threshold
	}

	// left: should see frequency at coeff 1 & coeff 2 (half as big as one)
	float abs_err = spectra[1] / 50.f;
	EXPECT_NEAR(spectra[1], TWO * spectra[2], abs_err);

	float thresh = spectra[1] / 50.f;
	for (size_t i = 0; i < NUM_COEFF; ++i) {
		if (i != 1 && i != 2) {
			EXPECT_TRUE(spectra[i] < thresh)
				<< "i is " << i << ", threshold is " << thresh <<
				", spectra[i] is " << spectra[i];
		}
	}

	// right: should see 'em at 2 & 4 (the one at 4 being one-quarter the size)
	abs_err = spectra[NUM_COEFF + 2] / 50.f;
	EXPECT_NEAR(spectra[NUM_COEFF + 2], FOUR * spectra[NUM_COEFF + 4], abs_err);
	thresh = spectra[NUM_COEFF + 2] /50.f;
	for (size_t i = 0; i < NUM_COEFF; ++i) {
		if (i != 2 && i != 4) {
			EXPECT_TRUE(spectra[NUM_COEFF + i] < thresh)
				<< "i is " << i << ", threshold is " << thresh <<
				", spectra[NUM_COEFF + i] is " << spectra[NUM_COEFF + i];
		}
	}

}

// Network protocol -- deserialization
TEST(VisualizationTest, TestDeCliHlo)
{
	ClientHello clihlo;
	uint8_t incomplete_buf_0[] = { 0x00 };
	EXPECT_EQ(ParseResult::NEED_MORE_DATA,
		  ParseClihlo(incomplete_buf_0, sizeof(incomplete_buf_0), clihlo));

	// Correct message type, length is zero
	uint8_t incomplete_buf_1[] = { 0x00, 0x00, 0x00, 0x00 };
	EXPECT_EQ(ParseResult::NEED_MORE_DATA,
		  ParseClihlo(incomplete_buf_1, sizeof(incomplete_buf_1), clihlo));

	// Correct message type, length is correct, payload is incomplete
	uint8_t incomplete_buf_2[] = { 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x20 };
	EXPECT_EQ(ParseResult::NEED_MORE_DATA,
		  ParseClihlo(incomplete_buf_2, sizeof(incomplete_buf_2), clihlo));

	// Correct message type, length is correct, missing "check byte"
	uint8_t incomplete_buf_3[] = {
		0x00, 0x00,
		0x00, 0x06,
		0x00, 0x01,
		0x00, 0x20,
		0x00, 0xff
	};
	EXPECT_EQ(ParseResult::NEED_MORE_DATA,
		  ParseClihlo(incomplete_buf_3, sizeof(incomplete_buf_3), clihlo));

	// Correct message, except the length is incorrect
	uint8_t incomplete_buf_4[] = {
		0x00, 0x00,
		0x00, 0x05,
		0x00, 0x01,
		0x00, 0x20,
		0x00, 0xff
	};
	EXPECT_EQ(ParseResult::NEED_MORE_DATA,
		  ParseClihlo(incomplete_buf_4, sizeof(incomplete_buf_4), clihlo));

	// Finally correct
	uint8_t complete_buf_0[] = {
		0x00, 0x00,
		0x00, 0x06,
		0x00, 0x01,
		0x00, 0x20,
		0x00, 0xff,
		0x00
	};
	EXPECT_EQ(ParseResult::OK,
		  ParseClihlo(complete_buf_0, sizeof(complete_buf_0), clihlo));

	EXPECT_EQ(clihlo.major_version, 0);
	EXPECT_EQ(clihlo.minor_version, 1);
	EXPECT_EQ(clihlo.requested_fps, 32);
	EXPECT_EQ(clihlo.tau, 255);
}

// Network protocol -- serialization
TEST(VisualizationTest, TestSerSrvHlo)
{
	using std::byte;

	byte buf[] = {
		(byte)0x00, (byte)0x00, // type
		(byte)0x00, (byte)0x00, // length
		(byte)0x00, (byte)0x00, // payload
		(byte)0x00,				// check
		(byte)0xaa				// tombstone
	};

	SerializeSrvhlo((byte)3, (byte)2, buf);

	ASSERT_EQ(buf[0], (byte)0x00);
	ASSERT_EQ(buf[1], (byte)0x01);
	ASSERT_EQ(buf[2], (byte)0x00);
	ASSERT_EQ(buf[3], (byte)0x02);
	ASSERT_EQ(buf[4], (byte)0x03);
	ASSERT_EQ(buf[5], (byte)0x02);
	ASSERT_EQ(buf[6], (byte)0x00);
	ASSERT_EQ(buf[7], (byte)0xaa);
}

/**
 * \page vis_out_indexing_torture_test Torture-test the indexing of Fourier coefficients
 *
 * \section vis_out_indexing_torture_test_intro Introduction
 *
 * Between the Fast Fourier Transform library's use of the Hermitian property to
 * only return the first n/2 + 1 Fourier coefficients and the visualization
 * plugin's options to clamp frequencies to a certain range for analysis
 * purposes, the indexing logic is complex. This page derives test data for a
 * parameterized test suite designed to "torture" that stretch of code.
 *
 * \section vis_out_indexing_torture_test_data The Data
 *
 * We'll assume we have three channels, and define one (continuous) function for
 * each:
 *
 \code
		  1
  f(x) = sin(x) + - cos(2x)
		  2

		   1
  g(x) = sin(2x) + - cos(4x)
		   4

	     x
  h(x) = sin(-) + 2 cos(2x)
	     2

 \endcode
 *
 * These have continuous Fourier transforms of:
 *
 \code
  1                                               1
  - pi d(w - 2) - i pi d(w - 1) + i pi d(w + 1) + - pi d(w + 2)
  2                                               2

  1                                         1
  - pi d(w-4) - i pi d(w-2) + i pi d(w+2) + - pi d(w+4)
  4                                         4

  -2 pi i d(2w-1) + 2 pi d(w-2) + 2 pi d(w+2) + 2 pi i d(2w+1)
 \endcode
 *
 * Mathematica code that produced these:
 *
 \code

  FourierTransform[Sin[x]+1/2Cos[2x],x,w,FourierParameters->{1,-1}]

  FourierTransform[Sin[2x]+1/4Cos[4x],x,w,FourierParameters->{1, -1}]

  FourierTransform[Sin[x/2]+2Cos[2x],x,w,FourierParameters->{1, -1}]

 \endcode
 *
 * Now let's sample each waveform at seventeen points over the range [0,4Pi].
 * Seventeen was chosen not because it's enough to derive any meaningful
 * information about the waveforms but because it's enough to run a suite of
 * test cases while small enough to be computationally tractable. Four Pi was
 * chosen because \c f, \c g, and \c h are mutually periodic over that interval.
 *
 * However, we introduce a transformation so that we can tell the codebase that
 * we're sampling once per second, since 2Pi/16 is about 0.785 and class
 * AudioFormat only accepts integers for the sample rate. Let x = pi/4 * u:
 *
 \code

   u   x = pi/4 * u f(x)      	  g(x)      h(x)
   -   ------------ ----      	  ----      ----
   0   0       	    1/2       	  1/4       2
   1   Pi/4    	    1/Sqrt[2] 	  3/4       Sin[Pi/8]
   2   Pi/2    	    1/2       	  1/4       -2+1/Sqrt[2]
   3   (3 Pi)/4     1/Sqrt[2] 	  -(5/4)    Cos[Pi/8]
   4   Pi           1/2       	  1/4       3
   5   (5 Pi)/4     -(1/Sqrt[2])  3/4       Cos[Pi/8]
   6   (3 Pi)/2     -(3/2)        1/4       -2+1/Sqrt[2]
   7   (7 Pi)/4     -(1/Sqrt[2])  -(5/4)    Sin[Pi/8]
   8   2 Pi         1/2           1/4       2
   9   (9 Pi)/4     1/Sqrt[2]     3/4       -Sin[Pi/8]
   10  (5 Pi)/2     1/2           1/4       -2-1/Sqrt[2]
   11  (11 Pi)/4    1/Sqrt[2]     -(5/4)    -Cos[Pi/8]
   12  3 Pi         1/2           1/4       1
   13  (13 Pi)/4    -(1/Sqrt[2])  3/4       -Cos[Pi/8]
   14  (7 Pi)/2     -(3/2)        1/4       -2-1/Sqrt[2]
   15  (15 Pi)/4    -(1/Sqrt[2])  -(5/4)    -Sin[Pi/8]
   16  4 Pi         1/2           1/4       2

   t=Table[{u,u Pi/4,Sin[u Pi/4]+1/2Cos[2u Pi/4],Sin[2u Pi/4]+1/4Cos[4u Pi/4],Sin[u Pi/8]+2Cos[2u Pi/4]}, {u,0,16}]
 \endcode
 *
 *
 */

/* Define each test case by the low & high frequency cutoffs (in Hertz), along
 * with the expected lo & hi indicies in [0,17). */
class IdxTortureTestCase {
public:
	IdxTortureTestCase(float lo_cutoff_hz, float hi_cutoff_hz,
			   size_t idx_lo, size_t idx_hi) :
		lo_cutoff_hz_(lo_cutoff_hz), hi_cutoff_hz_(hi_cutoff_hz),
		idx_lo_(idx_lo), idx_hi_(idx_hi)
		{}
	std::tuple<float, float> cutoffs() const {
		return std::make_tuple(lo_cutoff_hz_, hi_cutoff_hz_);
	}
	std::tuple<size_t, size_t> idxs() const {
		return std::make_tuple(idx_lo_, idx_hi_);
	}

	// Let GTest pretty-print instances of this class
	friend void PrintTo(const IdxTortureTestCase &x, std::ostream* os) {
		*os << "((" << x.lo_cutoff_hz_ << "," << x.hi_cutoff_hz_ <<
			"), (" << x.idx_lo_ << "," << x.idx_hi_ << "))";
	}

private:
	float lo_cutoff_hz_;
	float hi_cutoff_hz_;
	size_t idx_lo_;
	size_t idx_hi_;

};

// Divide two floats while avoiding under- or overflow
static float safe_divide(float num, float div) {

	// Avoid overflow
	if ((div < 1.f) && (num > div * std::numeric_limits<float>::max())) {
		return std::numeric_limits<float>::max();
	}

	// Avoid underflow.
	if( (fabsf(num) <= std::numeric_limits<float>::min()) ||
	    ((div > 1.0f) && (num < div*std::numeric_limits<float>::min())) ) {
		return 0.f;
	}

	return num / div;
}

// Return true if the relative error between `lhs` & `rhs` is less than `tol`
static bool are_close(float lhs, float rhs, float tol) {

	float diff = fabsf(lhs - rhs);
	float frac_of_lhs = safe_divide(diff, fabs(lhs));
	float frac_of_rhs = safe_divide(diff, fabs(rhs));
	float max_rel_diff = std::max(frac_of_lhs, frac_of_rhs);

	return max_rel_diff <= tol;
}

// Return true if `lhs` & `rhs` agree to five significant digits
static bool five_digits(float lhs, float rhs) {
	return are_close(lhs, rhs, 1.e-05f);
}

class IdxTortureTest : public testing::TestWithParam<IdxTortureTestCase> {
public:
	// Fourier[SetPrecision[t[[;;,3]], 16], FourierParameters->{1,-1}]
	constexpr static const fftwf_complex f_coeffs[17] = {
		{ 0.50000000000000f,  0.f},
		{ 0.62404208822347f, -0.46986553084959f},
		{ 3.48070796230114f, -7.23828394653827f},
		{ 0.06111673918962f,  1.71295616696190f},
		{ 2.43751072458536f,  3.27051130807801f},
		{-1.12266955199846f, -0.53193455468083f},
		{-0.59746244376299f, -0.11563563964270f},
		{-0.46409036904584f, -0.03527953287599f},
		{-0.41915514949231f, -0.00829393093989f},
		{-0.41915514949231f,  0.00829393093989f},
		{-0.46409036904584f,  0.03527953287599f},
		{-0.59746244376299f,  0.11563563964270f},
		{-1.12266955199846f,  0.53193455468083f},
		{ 2.43751072458536f, -3.27051130807801f},
		{ 0.06111673918962f, -1.71295616696190f},
		{ 3.48070796230114f,  7.23828394653827f},
		{ 0.62404208822347f,  0.46986553084959f}
	};
	// Fourier[SetPrecision[t[[;;,4]], 16], FourierParameters->{1,-1}]
	constexpr static const fftwf_complex g_coeffs[17] = {
		{ 0.25000000000000f,  0.f},
		{ 0.28620899822335f, -0.14696793266575f},
		{ 0.42658182242300f, -0.35895971036545f},
		{ 0.87173429439100f, -0.84934191648538f},
		{ 5.16897572372971f, -5.16795421546238f},
		{-2.07706474071741f,  2.08837122139244f},
		{-1.07969011373806f,  1.16417468556352f},
		{-0.83808506555320f,  1.18824575269449f},
		{-0.75866091875839f,  2.79139586286938f},
		{-0.75866091875839f, -2.79139586286938f},
		{-0.83808506555320f, -1.18824575269449f},
		{-1.07969011373806f, -1.16417468556352f},
		{-2.07706474071741f, -2.08837122139244f},
		{ 5.16897572372971f,  5.16795421546238f},
		{ 0.87173429439100f,  0.84934191648538f},
		{ 0.42658182242300f,  0.35895971036545f},
		{ 0.28620899822335f,  0.146967932665f}
	};
	// Fourier[SetPrecision[t[[;;,5]], 16], FourierParameters->{1,-1}]
	constexpr static const fftwf_complex h_coeffs[17] = {
		{ 2.00000000000000f,  0.f},
		{ 3.5761227909419f,  -7.6567080944334f},
		{ 2.0830369856750f,   1.6088977373161f},
		{ 3.0216651144523f,   2.3664956368817f},
		{ 11.6290955114612f, 11.02082270531760f},
		{-2.8576336224259f,  -3.36095624506707f},
		{-0.8602633034329f,  -1.2241861217752f},
		{-0.3757216709962f,  -0.5623964777234f},
		{-0.2163018056754f,  -0.1684941877293f},
		{-0.2163018056754f,   0.1684941877293f},
		{-0.3757216709962f,   0.5623964777234f},
		{-0.8602633034329f,   1.2241861217752f},
		{-2.8576336224259f,   3.36095624506707f},
		{ 11.6290955114612,-11.02082270531760f},
		{ 3.0216651144523f,  -2.3664956368817f},
		{ 2.0830369856750f,  -1.6088977373161f},
		{ 3.5761227909419f,   7.65670809443f},
	};
};

TEST_P(IdxTortureTest, Torture) {
	using namespace std::chrono;

	IdxTortureTestCase test_case = GetParam();

	// Let's represent our wave form as IEEE 754 single precisions floats,
	// sampled once per second, with three channels.e. stereo).
	AudioFormat fmt(1, SampleFormat::FLOAT, 3);

	// Seventeen samples from f, g, h (above), interleaved (i.e. we have
	// f(t_0), g(t_0),h(t_0,f(t_1)...).
	constexpr float samples[51] = {
		 0.5000000000000000f,  0.2500000000000000f,  2.000000000000000f,
		 0.7071067811865475f,  0.7500000000000000f,  0.3826834323650898f,
		 0.5000000000000000f,  0.2500000000000000f, -1.292893218813452f,
		 0.7071067811865475f, -1.250000000000000f,   0.9238795325112868f,
		 0.5000000000000000f,  0.2500000000000000f,  3.000000000000000f,
		-0.7071067811865475f,  0.7500000000000000f,  0.9238795325112868f,
		-1.500000000000000f,   0.2500000000000000f, -1.292893218813452f,
		-0.7071067811865475f, -1.250000000000000f,   0.3826834323650898f,
		 0.5000000000000000f,  0.2500000000000000f,  2.000000000000000f,
		 0.7071067811865475f,  0.7500000000000000f, -0.3826834323650898f,
		 0.5000000000000000f,  0.2500000000000000f, -2.707106781186548f,
		 0.7071067811865475f, -1.250000000000000f,  -0.9238795325112868f,
		 0.5000000000000000f,  0.2500000000000000f,  1.000000000000000f,
		-0.7071067811865475f,  0.7500000000000000f, -0.9238795325112868f,
		-1.500000000000000f,   0.2500000000000000f, -2.707106781186548f,
		-0.7071067811865475f, -1.250000000000000f,  -0.3826834323650898f,
		 0.5000000000000000f,  0.2500000000000000f,  2.000000000000000f
	};
	std::shared_ptr<Visualization::SoundInfoCache> pcache =
		std::make_unique<Visualization::SoundInfoCache>(fmt, seconds(18)); // eighteen seconds' capacity, just so we
										   // don't need to worry
	pcache->Add(samples, sizeof(samples));

	Visualization::SoundInfoCache::Time t0, t1;
	std::tie(t0, t1) = pcache->Range();
	// `t0` is whatever time the first sample was added; what we know is that
	// `t1` should be five seconds later.
	auto d = t1 - t0;
	EXPECT_EQ(d, seconds(17));

	float lo_cutoff, hi_cutoff;
	std::tie(lo_cutoff, hi_cutoff) = test_case.cutoffs();

	size_t idx_lo, idx_hi;
	std::tie(idx_lo, idx_hi) = test_case.idxs();

	// For each channel, FFTW will compute 17 Fourier coefficients,
	// corresponding to the frequencies 0Hz, 1/17Hz, 2/17Hz, ...,
	// 16/17Hz. This test is parameterized by the cutoff frequencies.
	SoundAnalysisParameters params { 17, lo_cutoff, hi_cutoff};
	SoundAnalysis analysis(params, pcache);

	// Any smoke?
	EXPECT_EQ(3, analysis.NumChan());
	EXPECT_EQ(17, analysis.NumSamp());
	EXPECT_EQ(idx_hi - idx_lo, analysis.NumFreq());

	EXPECT_TRUE(analysis.Update(t1));

	// On to the meat of the test-- we could get up to 17 complex-valued
	// Fourier coefficients back, expressed in single precision, per
	// channel. `SerializeCoefficients()` is designed for use with
	// serialization, so it works in terms of octets in network byte order.
	std::byte buf[17 * 8 * 3];
	std::byte *pout = analysis.SerializeCoefficients(buf);
	EXPECT_EQ(pout - buf, 3*8*(idx_hi - idx_lo));

	// For each channel, we expect F_{idx_lo}..F{idx_hi-1}, where `F`
	// denotes the pre-computed Fourier coefficients in class
	// `IdxTortureTest`
	size_t i = 0; // Let `i` run over `buf`, counting by float
	for (size_t j = idx_lo; j < idx_hi; ++j, i += 2) {

		/* Nb. that the GTest macro for comparing floating-point values,
		   `EXPECT_FLOAT_EQ` is hard coded to a tolerance of 4ULP (units
		   in last place). These tests can't handle that level of
		   precision (I suspect because FFTW is using optimzed
		   algorithms that sacrifice prceision for speed). I've written
		   my own comparison routine, leaning rather heavily on the Boost
		   implementation: <https://www.boost.org/doc/libs/1_84_0/libs/test/doc/html/boost_test/testing_tools/extended_comparison/floating_point/floating_points_comparison_theory.html>*/

		EXPECT_PRED2(five_digits, float_at(buf, i),   f_coeffs[j][0]) <<
			"(i: " << i << ", j: " << j << ")";
		EXPECT_PRED2(five_digits, float_at(buf, i+1), f_coeffs[j][1]) <<
			"(i: " << i << ", j: " << j << ")";
	}
	for (size_t j = idx_lo; j < idx_hi; ++j, i += 2) {
		EXPECT_PRED2(five_digits, float_at(buf, i),   g_coeffs[j][0]) <<
			"(i: " << i << ", j: " << j << ")";
		EXPECT_PRED2(five_digits, float_at(buf, i+1), g_coeffs[j][1]) <<
			"(i: " << i << ", j: " << j << ")";
	}
	for (size_t j = idx_lo; j < idx_hi; ++j, i += 2) {
		EXPECT_PRED2(five_digits, float_at(buf, i),   h_coeffs[j][0]) <<
			"(i: " << i << ", j: " << j << ")";
		EXPECT_PRED2(five_digits, float_at(buf, i+1), h_coeffs[j][1]) <<
			"(i: " << i << ", j: " << j << ")";
	}
}

INSTANTIATE_TEST_SUITE_P(
	IndexTortureTesting,
	IdxTortureTest,
	testing::Values(
	/* 0*/	IdxTortureTestCase(0.0,  0.5,   0,  9), // Entire first half
	/* 1*/	IdxTortureTestCase(0.0,  1.0,   0, 17), // Entire spectrum
	/* 2*/	IdxTortureTestCase(0.06, 0.5,   1,  9), // "In" one lhs, first half
	/* 3*/	IdxTortureTestCase(0.12, 0.5,   2,  9), // "In" two lhs, first half
	/* 4*/	IdxTortureTestCase(0.0,  0.47,  0,  8), // "In" one rhs, first half
	/* 5*/	IdxTortureTestCase(0.0,  0.4,   0,  7), // "In" two rhs, first half
	/* 6*/	IdxTortureTestCase(0.06, 0.47,  1,  8), // "In" one on each side, first half
	/* 7*/	IdxTortureTestCase(0.12, 0.4,   2,  7), // "In" two on each side, first half
	/* 8*/	IdxTortureTestCase(0.0,  0.53,  0, 10), // First half + 1
	/* 9*/	IdxTortureTestCase(0.0,  0.59,  0, 11), // First half + 2
	/*10*/	IdxTortureTestCase(0.42, 0.59,  7, 11), // 2 in first half, 2 in second
	/*11*/	IdxTortureTestCase(0.48, 0.59,  8, 11), // 1 in first half, 2 in second
	/*12*/	IdxTortureTestCase(0.48, 0.65,  8, 12), // 1 in first half, 3 in second
	/*13*/	IdxTortureTestCase(0.53, 1.0,   9, 17), // entire second half
        /*14*/  IdxTortureTestCase(0.59, 0.89, 10, 16), // "In" one on either side, 2nd half
	/*15*/  IdxTortureTestCase(0.65, 0.89, 11, 16)  // "In" two on lhs, one on rhs, 2nd half
		));
