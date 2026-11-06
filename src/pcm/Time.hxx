// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <cstddef>
#include <cstdint>

template<std::size_t CAPACITY> class StringBuffer;

template<typename D>
constexpr auto
PcmTimeToFrames(D t, unsigned sample_rate) noexcept
{
	using Period = typename D::period;
	return ((t.count() * sample_rate) / Period::den) * Period::num;
}

template<typename D>
constexpr std::size_t
PcmTimeToSize(D t, unsigned sample_rate, unsigned frame_size) noexcept
{
	return std::size_t(std::size_t(PcmTimeToFrames(t, sample_rate)) * frame_size);
}

template<typename D>
constexpr D
PcmFramesToTime(std::uintmax_t n_frames, unsigned sample_rate) noexcept
{
	using Rep = typename D::rep;
	using Period = typename D::period;
	return D(((Rep(1) * n_frames / Period::num) * Period::den) / sample_rate);
}

template<typename D>
constexpr D
PcmSizeToTime(std::uintmax_t size, unsigned sample_rate, unsigned frame_size) noexcept
{
	return PcmFramesToTime<D>(size / frame_size, sample_rate);
}
