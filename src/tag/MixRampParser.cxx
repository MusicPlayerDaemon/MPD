// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MixRampParser.hxx"
#include "VorbisComment.hxx"
#include "MixRampInfo.hxx"
#include "util/ASCII.hxx"

#include <cassert>

template<typename T>
static bool
ParseMixRampTagTemplate(MixRampInfo &info, const T t) noexcept
{
	if (const std::string_view value = t["mixramp_start"];
	    !value.empty()) {
		info.SetStart(std::string{value});
		return true;
	}

	if (const std::string_view value = t["mixramp_end"];
	    !value.empty()) {
		info.SetEnd(std::string{value});
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

		[[gnu::pure]]
		std::string_view operator[](const char *n) const noexcept {
			return StringEqualsCaseASCII(name, n)
				? value
				: std::string_view{};
		}
	};

	return ParseMixRampTagTemplate(info, NameValue{name, value});
}

bool
ParseMixRampVorbis(MixRampInfo &info, std::string_view entry) noexcept
{
	struct VorbisCommentEntry {
		std::string_view entry;

		[[gnu::pure]]
		std::string_view operator[](std::string_view n) const noexcept {
			return GetVorbisCommentValue(entry, n);
		}
	};

	return ParseMixRampTagTemplate(info, VorbisCommentEntry{entry});
}
