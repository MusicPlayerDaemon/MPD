/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "TagAny.hxx"
#include "TagStream.hxx"
#include "TagFile.hxx"
#include "tag/Generic.hxx"
#include "storage/StorageInterface.hxx"
#include "client/Client.hxx"
#include "protocol/Ack.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Compiler.h"
#include "util/UriExtract.hxx"
#include "LocateUri.hxx"

static void
TagScanStream(const char *uri, TagHandler &handler)
{
	if (!tag_stream_scan(uri, handler))
		throw ProtocolError(ACK_ERROR_NO_EXIST, "Failed to load file");
}

static void
TagScanFile(const Path path_fs, TagHandler &handler)
{
	if (!ScanFileTagsNoGeneric(path_fs, handler))
		throw ProtocolError(ACK_ERROR_NO_EXIST, "Failed to load file");

	ScanGenericTags(path_fs, handler);
}

static void
TagScanDatabase(Client &client, const char *uri, TagHandler &handler)
{
#ifdef ENABLE_DATABASE
	const Storage *storage = client.GetStorage();
	if (storage == nullptr) {
#else
		(void)client;
		(void)uri;
		(void)handler;
#endif
		throw ProtocolError(ACK_ERROR_NO_EXIST, "No database");
#ifdef ENABLE_DATABASE
	}

	{
		const auto path_fs = storage->MapFS(uri);
		if (!path_fs.IsNull())
			return TagScanFile(path_fs, handler);
	}

	{
		const auto absolute_uri = storage->MapUTF8(uri);
		if (uri_has_scheme(absolute_uri.c_str()))
			return TagScanStream(absolute_uri.c_str(), handler);
	}

	throw ProtocolError(ACK_ERROR_NO_EXIST, "No such file");
#endif
}

void
TagScanAny(Client &client, const char *uri, TagHandler &handler)
{
	const auto located_uri = LocateUri(UriPluginKind::INPUT, uri, &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );
	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		return TagScanStream(located_uri.canonical_uri, handler);

	case LocatedUri::Type::RELATIVE:
		return TagScanDatabase(client, located_uri.canonical_uri,
				       handler);

	case LocatedUri::Type::PATH:
		return TagScanFile(located_uri.path, handler);
	}

	gcc_unreachable();
}
