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

#include "config.h"
#include "LocateUri.hxx"
#include "client/Client.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"
#include "storage/Registry.hxx"
#include "util/ASCII.hxx"
#include "util/UriExtract.hxx"

#ifdef ENABLE_DATABASE
#include "storage/StorageInterface.hxx"
#endif

#include <stdexcept>

static LocatedUri
LocateFileUri(const char *uri, const Client *client
#ifdef ENABLE_DATABASE
	      , const Storage *storage
#endif
	      )
{
	auto path = AllocatedPath::FromUTF8Throw(uri);

#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const auto suffix = storage->MapToRelativeUTF8(uri);
		if (suffix.data() != nullptr)
			/* this path was relative to the music
			   directory */
			// TODO: don't use suffix.data() (ok for now because we know it's null-terminated)
			return {LocatedUri::Type::RELATIVE, suffix.data()};
	}
#endif

	if (client != nullptr)
		client->AllowFile(path);

	return {LocatedUri::Type::PATH, uri, std::move(path)};
}

static LocatedUri
LocateAbsoluteUri(UriPluginKind kind, const char *uri
#ifdef ENABLE_DATABASE
		  , const Storage *storage
#endif
		  )
{
	switch (kind) {
	case UriPluginKind::INPUT:
		if (!uri_supported_scheme(uri))
			throw std::invalid_argument("Unsupported URI scheme");
		break;

	case UriPluginKind::STORAGE:
		/* plugin support will be checked after the
		   Storage::MapToRelativeUTF8() call */
		break;

	case UriPluginKind::PLAYLIST:
		/* for now, no validation for playlist URIs; this is
		   more complicated because there are three ways to
		   identify which plugin to use: URI scheme, filename
		   suffix and MIME type */
		break;
	}

#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const auto suffix = storage->MapToRelativeUTF8(uri);
		if (suffix.data() != nullptr)
			// TODO: don't use suffix.data() (ok for now because we know it's null-terminated)
			return {LocatedUri::Type::RELATIVE, suffix.data()};
	}

	if (kind == UriPluginKind::STORAGE &&
	    GetStoragePluginByUri(uri) == nullptr)
		throw std::invalid_argument("Unsupported URI scheme");
#endif

	return {LocatedUri::Type::ABSOLUTE, uri};
}

LocatedUri
LocateUri(UriPluginKind kind,
	  const char *uri, const Client *client
#ifdef ENABLE_DATABASE
	  , const Storage *storage
#endif
	  )
{
	/* skip the obsolete "file://" prefix */
	const char *path_utf8 = StringAfterPrefixCaseASCII(uri, "file://");
	if (path_utf8 != nullptr) {
		if (!PathTraitsUTF8::IsAbsolute(path_utf8))
			throw std::invalid_argument("Malformed file:// URI");

		return LocateFileUri(path_utf8, client
#ifdef ENABLE_DATABASE
				     , storage
#endif
				     );
	} else if (PathTraitsUTF8::IsAbsolute(uri))
		return LocateFileUri(uri, client
#ifdef ENABLE_DATABASE
				     , storage
#endif
				     );
	else if (uri_has_scheme(uri))
		return LocateAbsoluteUri(kind, uri
#ifdef ENABLE_DATABASE
					 , storage
#endif
					 );
	else
		return LocatedUri(LocatedUri::Type::RELATIVE, uri);
}
