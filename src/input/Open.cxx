// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InputStream.hxx"
#include "Registry.hxx"
#include "InputPlugin.hxx"
#include "LocalOpen.hxx"
#include "RewindInputStream.hxx"
#include "WaitReady.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"

#include <stdexcept>

InputStreamPtr
InputStream::Open(std::string_view url, Mutex &mutex)
{
	if (PathTraitsUTF8::IsAbsolute(url)) {
		const auto path = AllocatedPath::FromUTF8Throw(url);
		return OpenLocalInputStream(path, mutex);
	}

	for (const auto &plugin : GetEnabledInputPlugins()) {
		if (!plugin.SupportsUri(url))
			continue;

		if (auto is = plugin.open(url, mutex))
			return input_rewind_open(std::move(is));
	}

	throw std::runtime_error("Unrecognized URI");
}

InputStreamPtr
InputStream::OpenReady(std::string_view uri, Mutex &mutex)
{
	auto is = Open(uri, mutex);
	LockWaitReady(*is);
	return is;
}
