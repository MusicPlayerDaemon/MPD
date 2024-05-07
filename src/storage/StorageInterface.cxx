// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StorageInterface.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"

AllocatedPath
Storage::MapFS([[maybe_unused]] std::string_view uri_utf8) const noexcept
{
	return nullptr;
}

AllocatedPath
Storage::MapChildFS(std::string_view uri_utf8,
		    std::string_view child_utf8) const noexcept
{
	const auto uri2 = PathTraitsUTF8::Build(uri_utf8, child_utf8);
	return MapFS(uri2);
}
