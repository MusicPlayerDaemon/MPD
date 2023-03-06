// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NULL_FILTER_HXX
#define MPD_NULL_FILTER_HXX

#include "filter/Filter.hxx"

class NullFilter final : public Filter {
public:
	explicit NullFilter(const AudioFormat &af):Filter(af) {}

	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override {
		return src;
	}
};

#endif
