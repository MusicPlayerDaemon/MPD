// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InputStream.hxx"
#include "Registry.hxx"
#include "InputPlugin.hxx"
#include "LocalOpen.hxx"
#include "CondHandler.hxx"
#include "RewindInputStream.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"

#include <stdexcept>

InputStreamPtr
InputStream::Open(const char *url, Mutex &mutex)
{
	if (PathTraitsUTF8::IsAbsolute(url)) {
		const auto path = AllocatedPath::FromUTF8Throw(url);
		return OpenLocalInputStream(path, mutex);
	}

	input_plugins_for_each_enabled(plugin) {
		if (!plugin->SupportsUri(url))
			continue;

		auto is = plugin->open(url, mutex);
		if (is != nullptr)
			return input_rewind_open(std::move(is));
	}

	throw std::runtime_error("Unrecognized URI");
}

InputStreamPtr
InputStream::OpenReady(const char *uri, Mutex &mutex)
{
	CondInputStreamHandler handler;

	auto is = Open(uri, mutex);
	is->SetHandler(&handler);

	{
		std::unique_lock<Mutex> lock(mutex);

		handler.cond.wait(lock, [&is]{
			is->Update();
			return is->IsReady();
		});

		is->Check();
	}

	is->SetHandler(nullptr);
	return is;
}
