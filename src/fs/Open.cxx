// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Open.hxx"
#include "Path.hxx"
#include "system/Error.hxx"
#include "io/FileHandle.hxx"

#ifdef _WIN32
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/SystemError.hxx"
#include <fileapi.h>
#else
#include "io/Open.hxx"
#endif

using std::string_view_literals::operator""sv;

UniqueFileHandle
OpenReadOnly(Path path)
{
#ifdef _WIN32
	const FileHandle handle{
		CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
			   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			   nullptr)
	};

	if (!handle.IsDefined())
		throw FmtLastError("Failed to open {:?}"sv, path);

	return UniqueFileHandle{AdoptTag{}, handle};
#else
	return OpenReadOnly(path.c_str());
#endif
}

