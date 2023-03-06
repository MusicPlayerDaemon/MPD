// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MIX_RAMP_INFO_HXX
#define MPD_MIX_RAMP_INFO_HXX

#include <string>

class MixRampInfo {
	std::string start, end;

public:
	MixRampInfo() = default;

	void Clear() noexcept {
		start.clear();
		end.clear();
	}

	[[gnu::pure]]
	bool IsDefined() const noexcept {
		return !start.empty() || !end.empty();
	}

	[[gnu::pure]]
	const char *GetStart() const noexcept {
		return start.empty() ? nullptr : start.c_str();
	}

	[[gnu::pure]]
	const char *GetEnd() const noexcept {
		return end.empty() ? nullptr : end.c_str();
	}

	void SetStart(const char *new_value) noexcept {
		if (new_value == nullptr)
			start.clear();
		else
			start = new_value;
	}

	void SetStart(std::string &&new_value) noexcept {
		start = std::move(new_value);
	}

	void SetEnd(const char *new_value) noexcept {
		if (new_value == nullptr)
			end.clear();
		else
			end = new_value;
	}

	void SetEnd(std::string &&new_value) noexcept {
		end = std::move(new_value);
	}
};

#endif
