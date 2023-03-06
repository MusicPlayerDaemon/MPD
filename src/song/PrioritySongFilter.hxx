// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "ISongFilter.hxx"

#include <chrono>

class PrioritySongFilter final : public ISongFilter {
	const uint8_t value;

public:
	explicit PrioritySongFilter(uint8_t _value) noexcept
		:value(_value) {}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<PrioritySongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};
