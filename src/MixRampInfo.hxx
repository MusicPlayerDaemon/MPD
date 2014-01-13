/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_MIX_RAMP_INFO_HXX
#define MPD_MIX_RAMP_INFO_HXX

#include "check.h"
#include "Compiler.h"

#include <string>

class MixRampInfo {
	std::string start, end;

public:
	MixRampInfo() = default;

	void Clear() {
		start.clear();
		end.clear();
	}

	gcc_pure
	bool IsDefined() const {
		return !start.empty() || !end.empty();
	}

	gcc_pure
	const char *GetStart() const {
		return start.empty() ? nullptr : start.c_str();
	}

	gcc_pure
	const char *GetEnd() const {
		return end.empty() ? nullptr : end.c_str();
	}

	void SetStart(const char *new_value) {
		if (new_value == nullptr)
			start.clear();
		else
			start = new_value;
	}

	void SetStart(std::string &&new_value) {
		start = std::move(new_value);
	}

	void SetEnd(const char *new_value) {
		if (new_value == nullptr)
			end.clear();
		else
			end = new_value;
	}

	void SetEnd(std::string &&new_value) {
		end = std::move(new_value);
	}
};

#endif
