// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "LocateUri.hxx"
#include "client/IClient.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"
#include "storage/Registry.hxx"
#include "util/StringCompare.hxx"
#include "util/UriExtract.hxx"
#include "util/UriUtil.hxx"

#ifdef ENABLE_DATABASE
#include "storage/StorageInterface.hxx"
#endif

#include <stdexcept>

using std::string_view_literals::operator""sv;

static LocatedUri
LocateFileUri(const std::string_view uri, const IClient *client
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
			return {LocatedUri::Type::RELATIVE, suffix};
	}
#endif

	if (client != nullptr)
		client->AllowFile(path);

	return {LocatedUri::Type::PATH, uri, std::move(path)};
}

static LocatedUri
LocateAbsoluteUri(UriPluginKind kind, const std::string_view uri
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
			return {LocatedUri::Type::RELATIVE, suffix};
	}

	if (kind == UriPluginKind::STORAGE &&
	    GetStoragePluginByUri(uri) == nullptr)
		throw std::invalid_argument("Unsupported URI scheme");
#endif

	return {LocatedUri::Type::ABSOLUTE, uri};
}

LocatedUri
LocateUri(UriPluginKind kind,
	  const std::string_view uri, const IClient *client
#ifdef ENABLE_DATABASE
	  , const Storage *storage
#endif
	  )
{
	/* skip the obsolete "file://" prefix */
	if (const auto path_utf8 = StringAfterPrefixIgnoreCase(uri, "file://"sv);
	    path_utf8.data() != nullptr) {
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
	else {
		if (!uri_safe_local(uri))
			throw std::invalid_argument{"Bad relative path"};

		return LocatedUri(LocatedUri::Type::RELATIVE, uri);
	}
}
