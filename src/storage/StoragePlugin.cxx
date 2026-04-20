// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StoragePlugin.hxx"
#include "util/StringCompare.hxx"

bool
StoragePlugin::SupportsUri(std::string_view uri) const noexcept
{
	if (prefixes == nullptr)
		return false;

	for (auto i = prefixes; *i != nullptr; ++i)
		if (StringStartsWithIgnoreCase(uri, *i))
			return true;

	return false;
}
