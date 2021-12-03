/*
 * Copyright 2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * This library is based on af_replaygain.c from FFmpeg.  Original
 * copyright header:
 *
 * Copyright (c) 1998 - 2009 Conifer Software
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ReplayGainAnalyzer.hxx"
#include "util/Compiler.h"
#include "util/ConstBuffer.hxx"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <numeric>

ReplayGainAnalyzer::ReplayGainAnalyzer() noexcept
{
}

/*
 * Find the largest absolute sample value.
 */
[[gnu::pure]] [[gnu::hot]]
static float
FindPeak(const float *samples, std::size_t n) noexcept
{
	float peak = 0.0;

	while (n-- > 0) {
		float value = std::fabs(*samples++);
		if (value > peak)
			peak = value;
	}

	return peak;
}

static constexpr double
Square(double x) noexcept
{
	return x * x;
}

[[gnu::const]]
static double
SquareHypot(ReplayGainAnalyzer::Frame f) noexcept
{
#if GCC_OLDER_THAN(10,0)
	/* GCC 8 doesn't have std::transform_reduce() */
	double sum = 0;
	for (const auto &i : f)
		sum += Square(i);
	return sum;
#else
	/* proper C++17 */
	return std::transform_reduce(f.begin(), f.end(), 0.,
				     std::plus<double>{}, Square);
#endif
}

/*
 * Calculate stereo RMS level. Minimum value is about -100 dB for
 * digital silence. The 90 dB offset is to compensate for the
 * normalized float range and 3 dB is for stereo samples.
 */
[[gnu::hot]]
static double
CalcStereoRMS(ConstBuffer<ReplayGainAnalyzer::Frame> src) noexcept
{
#if GCC_OLDER_THAN(10,0)
	/* GCC 8 doesn't have std::transform_reduce() */
	double sum = 0;
	for (const auto &i : src)
		sum += SquareHypot(i);
#else
	/* proper C++17 */
	double sum = std::transform_reduce(src.begin(), src.end(),
					   1e-16,
					   std::plus<double>{},
					   SquareHypot);
#endif

	return 10 * std::log10(sum / src.size) + 90.0 - 3.0;
}

static constexpr bool
IsSilentSample(float value) noexcept
{
	return std::fabs(value) <= 1e-10f;
}

[[gnu::const]]
static bool
IsSilentFrame(ReplayGainAnalyzer::Frame frame) noexcept
{
	return std::all_of(frame.begin(), frame.end(), IsSilentSample);
}

[[gnu::pure]]
static bool
IsSilentBuffer(ConstBuffer<ReplayGainAnalyzer::Frame> buffer) noexcept
{
	return std::all_of(buffer.begin(), buffer.end(), IsSilentFrame);
}

static constexpr ReplayGainAnalyzer::DoubleFrame
operator*(ReplayGainAnalyzer::Frame f, float x) noexcept
{
	ReplayGainAnalyzer::DoubleFrame result{};
	for (std::size_t i = 0; i < result.size(); ++i)
		result[i] = (double)f[i] * (double)x;
	return result;
}

static constexpr auto &
operator+=(ReplayGainAnalyzer::DoubleFrame &dest, ReplayGainAnalyzer::DoubleFrame src) noexcept
{
	for (std::size_t i = 0; i < dest.size(); ++i)
		dest[i] += src[i];
	return dest;
}

static constexpr auto &
operator-=(ReplayGainAnalyzer::DoubleFrame &dest, ReplayGainAnalyzer::DoubleFrame src) noexcept
{
	for (std::size_t i = 0; i < dest.size(); ++i)
		dest[i] -= src[i];
	return dest;
}

[[maybe_unused]]
static constexpr auto
operator+(ReplayGainAnalyzer::DoubleFrame a, ReplayGainAnalyzer::DoubleFrame b) noexcept
{
	a += b;
	return a;
}

static constexpr auto
operator-(ReplayGainAnalyzer::DoubleFrame a, ReplayGainAnalyzer::DoubleFrame b) noexcept
{
	a -= b;
	return a;
}

static constexpr auto
ToSingle(ReplayGainAnalyzer::DoubleFrame src) noexcept
{
	ReplayGainAnalyzer::Frame dest{};
	for (std::size_t i = 0; i < dest.size(); ++i)
		dest[i] = src[i];
	return dest;
}

template<std::size_t ORDER>
static constexpr ReplayGainAnalyzer::Frame
ApplyFilter(ReplayGainAnalyzer::Frame src, std::size_t i,
	    std::array<ReplayGainAnalyzer::Frame, 128> &hist_a,
	    std::array<ReplayGainAnalyzer::Frame, 128> &hist_b,
	    const std::array<double, ORDER + 1> &coeff_a,
	    const std::array<double, ORDER + 1> &coeff_b) noexcept
{
	ReplayGainAnalyzer::DoubleFrame frame = (hist_b[i] = src) * coeff_b[0];

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 100
#endif
	for (std::size_t j = 1; j <= ORDER; ++j)
		frame += hist_b[i - j] * coeff_b[j] - hist_a[i - j] * coeff_a[j];

	return hist_a[i] = ToSingle(frame);
}

[[gnu::hot]]
inline void
ReplayGainAnalyzer::Yule::Filter(const Frame *gcc_restrict src,
				 Frame *gcc_restrict dst,
				 std::size_t n_frames) noexcept
{
	std::size_t i = hist_i;

	// If filter history is very small magnitude, clear it completely to
	// prevent denormals from rattling around in there forever
	// (slowing us down).

	if (IsSilentBuffer({&hist_a[i - ORDER], ORDER}) ||
	    IsSilentBuffer({&hist_b[i - ORDER], ORDER}))
		hist_a = hist_b = {};

	while (n_frames--) {
		*dst++ = ApplyFilter<ORDER>(*src++, i++,
					    hist_a, hist_b,
					    coeff_a, coeff_b);

		if (i == hist_a.size()) {
			constexpr std::size_t n = ORDER;
			std::copy(std::prev(hist_a.end(), n), hist_a.end(),
				  hist_a.begin());
			std::copy(std::prev(hist_b.end(), n), hist_b.end(),
				  hist_b.begin());
			i = n;
		}
	}

	hist_i = i;
}

[[gnu::hot]]
inline void
ReplayGainAnalyzer::Butter::Filter(Frame *gcc_restrict samples,
				   std::size_t n_frames) noexcept
{
	std::size_t i = hist_i;

	// If filter history is very small magnitude, clear it completely
	// to prevent denormals from rattling around in there forever
	// (slowing us down).

	if (IsSilentBuffer({&hist_a[i - ORDER], ORDER}) ||
	    IsSilentBuffer({&hist_b[i - ORDER], ORDER}))
		hist_a = hist_b = {};

	while (n_frames--) {
		*samples = ApplyFilter<ORDER>(*samples, i,
					      hist_a, hist_b,
					      coeff_a, coeff_b);
		++samples;
		++i;

		if (i == hist_a.size()) {
			constexpr std::size_t n = ORDER;
			std::copy(std::prev(hist_a.end(), n), hist_a.end(),
				  hist_a.begin());
			std::copy(std::prev(hist_b.end(), n), hist_b.end(),
				  hist_b.begin());
			i = n;
		}
	}

	hist_i = i;
}

void
ReplayGainAnalyzer::Process(ConstBuffer<Frame> src) noexcept
{
	assert(!src.empty());

	float new_peak = FindPeak(src.front().data(),
				  src.size * src.front().size());
	if (new_peak > peak)
		peak = new_peak;

	Frame *tmp = buffer.GetT<Frame>(src.size);
	yule.Filter(src.data, tmp, src.size);
	butter.Filter(tmp, src.size);

	const long level = std::lrint(std::floor(STEPS_PER_DB * CalcStereoRMS({tmp, src.size})));
	const std::size_t level_index = std::clamp(level, 0L, (long)histogram.size() - 1L);
	histogram[level_index]++;
}

/*
 * Calculate the ReplayGain value from the specified loudness histogram;
 * clip to -24 / +64 dB.
 */
template<std::size_t size>
[[gnu::pure]] [[gnu::hot]]
static float
FindHistogramPercentile95(const std::array<uint_least32_t, size> &histogram) noexcept
{
#if GCC_OLDER_THAN(10,0)
	/* GCC 8 doesn't have std::reduce() */
	uint_fast32_t total_windows = 0;
	for (const auto &i : histogram)
		total_windows += i;
#else
	/* proper C++17 */
	const uint_fast32_t total_windows =
		std::reduce(histogram.begin(), histogram.end());
#endif

	uint_fast32_t loud_count = 0;
	std::size_t i = histogram.size();
	while (i--)
		if ((loud_count += histogram[i]) * 20 >= total_windows)
			break;

	return i;
}

float
ReplayGainAnalyzer::GetGain() const noexcept
{
	std::size_t i = FindHistogramPercentile95(histogram);
	float gain = (float)(64.54f - float(i) / STEPS_PER_DB);

	return std::clamp(gain, -24.0f, 64.0f);
}

void
WindowReplayGainAnalyzer::CopyToBuffer(ConstBuffer<Frame> src) noexcept
{
	std::copy(src.begin(), src.end(),
		  window_buffer.data() + window_fill);
	window_fill += src.size;
}

void
WindowReplayGainAnalyzer::Process(ConstBuffer<Frame> src) noexcept
{
	assert(window_fill < WINDOW_FRAMES);

	if (window_fill > 0) {
		std::size_t window_space = WINDOW_FRAMES - window_fill;

		if (src.size < window_space) {
			CopyToBuffer(src);
			return;
		}

		CopyToBuffer({src.data, window_space});
		Flush();

		src.skip_front(window_space);
		if (src.empty())
			return;
	}

	while (src.size >= WINDOW_FRAMES) {
		ReplayGainAnalyzer::Process({src.data, WINDOW_FRAMES});
		src.skip_front(WINDOW_FRAMES);
	}

	CopyToBuffer(src);
}

void
WindowReplayGainAnalyzer::Flush() noexcept
{
	if (window_fill > 0)
		ReplayGainAnalyzer::Process({window_buffer.data(), window_fill});
	window_fill = 0;
}
