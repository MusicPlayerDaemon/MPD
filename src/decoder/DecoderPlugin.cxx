// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderPlugin.hxx"
#include "util/StringCompare.hxx"
#include "util/StringUtil.hxx"

#include <algorithm>
#include <cassert>

bool
DecoderPlugin::SupportsUri(const char *uri) const noexcept
{
	if (protocols != nullptr) {
		const auto p = protocols();
		return std::any_of(p.begin(), p.end(), [uri](const auto &schema)
			{ return StringStartsWithIgnoreCase(uri, schema.c_str()); } );
	}

	return false;
}

[[gnu::pure]]
static bool
SetContains(const auto &set, const auto &key) noexcept
{
#ifdef ANDROID
	/* the libc++ version in Android NDK r25c doesn't implement
	   std::set::contains() */ 
	return set.find(key) != set.end();
#else
	return set.contains(key);
#endif
}

bool
DecoderPlugin::SupportsSuffix(std::string_view suffix) const noexcept
{
	return (suffixes != nullptr &&
		StringArrayContainsCase(suffixes, suffix)) ||
		(suffixes_function != nullptr &&
		 SetContains(suffixes_function(), suffix));
}

bool
DecoderPlugin::SupportsMimeType(std::string_view mime_type) const noexcept
{
	return mime_types != nullptr &&
		StringArrayContainsCase(mime_types, mime_type);
}
