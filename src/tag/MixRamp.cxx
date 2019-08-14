/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "MixRamp.hxx"
#include "VorbisComment.hxx"
#include "MixRampInfo.hxx"
#include "util/ASCII.hxx"
#include "util/StringView.hxx"

#include <assert.h>

template<typename T>
static bool
ParseMixRampTagTemplate(MixRampInfo &info, const T t) noexcept
{
	const auto start = t["mixramp_start"];
	if (!start.IsNull()) {
		info.SetStart(std::string(start.data, start.size));
		return true;
	}

	const auto end = t["mixramp_end"];
	if (!start.IsNull()) {
		info.SetEnd(std::string(end.data, end.size));
		return true;
	}

	return false;
}

bool
ParseMixRampTag(MixRampInfo &info,
		const char *name, const char *value) noexcept
{
	assert(name != nullptr);
	assert(value != nullptr);

	struct NameValue {
		const char *name;
		const char *value;

		gcc_pure
		StringView operator[](const char *n) const noexcept {
			return StringEqualsCaseASCII(name, n)
				? value
				: nullptr;
		}
	};

	return ParseMixRampTagTemplate(info, NameValue{name, value});
}

bool
ParseMixRampVorbis(MixRampInfo &info, StringView entry) noexcept
{
	struct VorbisCommentEntry {
		StringView entry;

		gcc_pure
		StringView operator[](StringView n) const noexcept {
			return GetVorbisCommentValue(entry, n);
		}
	};

	return ParseMixRampTagTemplate(info, VorbisCommentEntry{entry});
}
