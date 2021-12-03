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

#pragma once

#include "Buffer.hxx"

#include <array>
#include <cstdint>

template<typename T> struct ConstBuffer;

/**
 * Analyze a 44.1 kHz / stereo / float32 audio stream and calculate
 * peak and ReplayGain values.
 */
class ReplayGainAnalyzer {
public:
	static constexpr unsigned CHANNELS = 2;
	static constexpr unsigned SAMPLE_RATE = 44100;
	using sample_type = float;
	using Frame = std::array<sample_type, CHANNELS>;
	using DoubleFrame = std::array<double, CHANNELS>;

private:
	/*
	 * Optimized implementation of 10th-order IIR stereo filter.
	 */
	struct Yule {
		static constexpr std::size_t ORDER = 10;

		static constexpr std::array coeff_a{
			1.00000000000000, -3.47845948550071, 6.36317777566148,
			-8.54751527471874, 9.47693607801280, -8.81498681370155,
			6.85401540936998, -4.39470996079559, 2.19611684890774,
			-0.75104302451432, 0.13149317958808,
		};

		static constexpr std::array coeff_b{
			0.05418656406430, -0.02911007808948, -0.00848709379851,
			-0.00851165645469, -0.00834990904936, 0.02245293253339,
			-0.02596338512915, 0.01624864962975, -0.00240879051584,
			0.00674613682247, -0.00187763777362
		};

		static_assert(coeff_a.size() == ORDER + 1);
		static_assert(coeff_b.size() == ORDER + 1);

		unsigned hist_i = ORDER;
		std::array<Frame, 128> hist_a{}, hist_b{};

		void Filter(const Frame *src, Frame *dst,
			    std::size_t n_frames) noexcept;
	};

	/*
	 * Optimized implementation of 2nd-order IIR stereo filter.
	 */
	struct Butter {
		static constexpr std::size_t ORDER = 2;

		static constexpr std::array coeff_a{
			1.00000000000000, -1.96977855582618,  0.97022847566350,
		};

		static constexpr std::array coeff_b{
			0.98500175787242, -1.97000351574484,  0.98500175787242,
		};

		static_assert(coeff_a.size() == ORDER + 1);
		static_assert(coeff_b.size() == ORDER + 1);

		unsigned hist_i = ORDER;
		std::array<Frame, 128> hist_a{}, hist_b{};

		void Filter(Frame *samples, std::size_t n_frames) noexcept;
	};

	static constexpr std::size_t STEPS_PER_DB = 100;
	static constexpr unsigned MAX_DB = 120;

	std::array<uint_least32_t, STEPS_PER_DB * MAX_DB> histogram{};
	float peak = 0;

	Yule yule;
	Butter butter;

	PcmBuffer buffer;

public:
	ReplayGainAnalyzer() noexcept;

	void Process(ConstBuffer<Frame> src) noexcept;

	float GetPeak() const noexcept {
		return peak;
	}

	[[gnu::pure]]
	float GetGain() const noexcept;
};

/**
 * A variant of #ReplayGainAnalyzer which automatically calls
 * Process() with windows of 50ms.
 */
class WindowReplayGainAnalyzer : public ReplayGainAnalyzer {
	static constexpr std::size_t WINDOW_FRAMES = SAMPLE_RATE / 20;

	std::array<Frame, WINDOW_FRAMES> window_buffer;
	std::size_t window_fill = 0;

public:
	void Process(ConstBuffer<Frame> src) noexcept;

	void Flush() noexcept;

private:
	void CopyToBuffer(ConstBuffer<Frame> src) noexcept;
};
