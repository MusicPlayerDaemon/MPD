// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistPlugin.hxx"
#include "util/StringUtil.hxx"

bool
PlaylistPlugin::SupportsScheme(std::string_view scheme) const noexcept
{
	return schemes != nullptr &&
		StringArrayContainsCase(schemes, scheme);
}

bool
PlaylistPlugin::SupportsSuffix(std::string_view suffix) const noexcept
{
	return suffixes != nullptr &&
		StringArrayContainsCase(suffixes, suffix);
}

bool
PlaylistPlugin::SupportsMimeType(std::string_view mime_type) const noexcept
{
	return mime_types != nullptr &&
		StringArrayContainsCase(mime_types, mime_type);
}
