/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
