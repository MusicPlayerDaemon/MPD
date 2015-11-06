/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "config.h"
#include "LocateUri.hxx"
#include "client/Client.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"
#include "util/UriUtil.hxx"
#include "util/StringCompare.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#ifdef ENABLE_DATABASE
#include "storage/StorageInterface.hxx"
#endif

const Domain locate_uri_domain("locate_uri");

static LocatedUri
LocateFileUri(const char *uri, const Client *client,
#ifdef ENABLE_DATABASE
	      const Storage *storage,
#endif
	      Error &error)
{
	auto path = AllocatedPath::FromUTF8(uri, error);
	if (path.IsNull())
		return LocatedUri::Unknown();

#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const char *suffix = storage->MapToRelativeUTF8(uri);
		if (suffix != nullptr)
			/* this path was relative to the music
			   directory */
			return LocatedUri(LocatedUri::Type::RELATIVE, suffix);
	}
#endif

	if (client != nullptr && !client->AllowFile(path, error))
		return LocatedUri::Unknown();

	return LocatedUri(LocatedUri::Type::PATH, uri, std::move(path));
}

static LocatedUri
LocateAbsoluteUri(const char *uri,
#ifdef ENABLE_DATABASE
		  const Storage *storage,
#endif
		  Error &error)
{
	if (!uri_supported_scheme(uri)) {
		error.Set(locate_uri_domain, "Unsupported URI scheme");
		return LocatedUri::Unknown();
	}

#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const char *suffix = storage->MapToRelativeUTF8(uri);
		if (suffix != nullptr)
			return LocatedUri(LocatedUri::Type::RELATIVE, suffix);
	}
#endif

	return LocatedUri(LocatedUri::Type::ABSOLUTE, uri);
}

LocatedUri
LocateUri(const char *uri, const Client *client,
#ifdef ENABLE_DATABASE
	  const Storage *storage,
#endif
	  Error &error)
{
	/* skip the obsolete "file://" prefix */
	const char *path_utf8 = StringAfterPrefix(uri, "file://");
	if (path_utf8 != nullptr) {
		if (!PathTraitsUTF8::IsAbsolute(path_utf8)) {
			error.Set(locate_uri_domain, "Malformed file:// URI");
			return LocatedUri::Unknown();
		}

		return LocateFileUri(path_utf8, client,
#ifdef ENABLE_DATABASE
				     storage,
#endif
				     error);
	} else if (PathTraitsUTF8::IsAbsolute(uri))
		return LocateFileUri(uri, client,
#ifdef ENABLE_DATABASE
				     storage,
#endif
				     error);
	else if (uri_has_scheme(uri))
		return LocateAbsoluteUri(uri,
#ifdef ENABLE_DATABASE
					 storage,
#endif
					 error);
	else
		return LocatedUri(LocatedUri::Type::RELATIVE, uri);
}
