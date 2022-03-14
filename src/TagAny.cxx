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

#include "TagAny.hxx"
#include "TagStream.hxx"
#include "TagFile.hxx"
#include "tag/Generic.hxx"
#include "song/LightSong.hxx"
#include "db/Interface.hxx"
#include "storage/StorageInterface.hxx"
#include "client/Client.hxx"
#include "protocol/Ack.hxx"
#include "fs/AllocatedPath.hxx"
#include "input/InputStream.hxx"
#include "util/Compiler.h"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"
#include "util/UriExtract.hxx"
#include "LocateUri.hxx"

static void
TagScanStream(const char *uri, TagHandler &handler)
{
	Mutex mutex;

	auto is = InputStream::OpenReady(uri, mutex);
	if (!tag_stream_scan(*is, handler))
		throw ProtocolError(ACK_ERROR_NO_EXIST, "Failed to load file");

	ScanGenericTags(*is, handler);
}

static void
TagScanFile(const Path path_fs, TagHandler &handler)
{
	if (!ScanFileTagsNoGeneric(path_fs, handler))
		throw ProtocolError(ACK_ERROR_NO_EXIST, "Failed to load file");

	ScanGenericTags(path_fs, handler);
}

#ifdef ENABLE_DATABASE

/**
 * Collapse "../" prefixes in a URI relative to the specified base
 * URI.
 */
static std::string
ResolveUri(std::string_view base, const char *relative)
{
	while (true) {
		const char *rest = StringAfterPrefix(relative, "../");
		if (rest == nullptr)
			break;

		if (base == ".")
			throw ProtocolError(ACK_ERROR_NO_EXIST, "Bad real URI");

		base = PathTraitsUTF8::GetParent(base);
		relative = rest;
	}

	return PathTraitsUTF8::Build(base, relative);
}

/**
 * Look up the specified song in the database and return its
 * (resolved) "real" URI.
 */
static std::string
GetRealSongUri(Client &client, std::string_view uri)
{
	const auto &db = client.GetDatabaseOrThrow();

	const auto *song = db.GetSong(uri);
	if (song == nullptr)
		throw ProtocolError(ACK_ERROR_NO_EXIST, "No such song");

	AtScopeExit(&db, song) { db.ReturnSong(song); };

	if (song->real_uri == nullptr)
		return {};

	return ResolveUri(PathTraitsUTF8::GetParent(uri), song->real_uri);
}

#endif

static void
TagScanDatabase(Client &client, const char *uri, TagHandler &handler)
{
#ifdef ENABLE_DATABASE
	const auto real_uri = GetRealSongUri(client, uri);

	if (!real_uri.empty()) {
		uri = real_uri.c_str();

		// TODO: support absolute paths?
		if (uri_has_scheme(uri))
			return TagScanStream(uri, handler);
	}

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
