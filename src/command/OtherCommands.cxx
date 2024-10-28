// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "OtherCommands.hxx"
#include "Request.hxx"
#include "FileCommands.hxx"
#include "StorageCommands.hxx"
#include "db/Uri.hxx"
#include "storage/StorageInterface.hxx"
#include "LocateUri.hxx"
#include "song/DetachedSong.hxx"
#include "SongPrint.hxx"
#include "TagPrint.hxx"
#include "TagStream.hxx"
#include "tag/Handler.hxx"
#include "TimePrint.hxx"
#include "decoder/DecoderPrint.hxx"
#include "ls.hxx"
#include "time/ChronoUtil.hxx"
#include "util/UriUtil.hxx"
#include "util/StringAPI.hxx"
#include "fs/AllocatedPath.hxx"
#include "Stats.hxx"
#include "PlaylistFile.hxx"
#include "db/PlaylistVector.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "protocol/IdleFlags.hxx"
#include "Log.hxx"
#include "Mapper.hxx"

#ifdef ENABLE_DATABASE
#include "DatabaseCommands.hxx"
#include "db/Interface.hxx"
#include "db/update/Service.hxx"
#endif

#include <fmt/format.h>

#include <cassert>

static void
print_spl_list(Response &r, const PlaylistVector &list)
{
	for (const auto &i : list) {
		r.Fmt(FMT_STRING("playlist: {}\n"), i.name);

		if (!IsNegative(i.mtime))
			time_print(r, "Last-Modified", i.mtime);
	}
}

CommandResult
handle_urlhandlers(Client &client, [[maybe_unused]] Request args, Response &r)
{
	if (client.IsLocal())
		r.Write("handler: file://\n");
	print_supported_uri_schemes(r);
	return CommandResult::OK;
}

CommandResult
handle_decoders([[maybe_unused]] Client &client, [[maybe_unused]] Request args,
		Response &r)
{
	decoder_list_print(r);
	return CommandResult::OK;
}

CommandResult
handle_kill([[maybe_unused]] Client &client, [[maybe_unused]] Request request,
	    [[maybe_unused]] Response &r)
{
	return CommandResult::KILL;
}

CommandResult
handle_listfiles(Client &client, Request args, Response &r)
{
	/* default is root directory */
	const auto uri = args.GetOptional(0, "");

	const auto located_uri = LocateUri(UriPluginKind::STORAGE, uri, &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );

	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
#ifdef ENABLE_DATABASE
		/* use storage plugin to list remote directory */
		return handle_listfiles_storage(client, r,
						located_uri.canonical_uri);
#else
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
#endif

	case LocatedUri::Type::RELATIVE:
#ifdef ENABLE_DATABASE
		if (client.GetInstance().storage != nullptr)
			/* if we have a storage instance, obtain a list of
			   files from it */
			return handle_listfiles_storage(r,
							*client.GetInstance().storage,
							uri);

		/* fall back to entries from database if we have no storage */
		return handle_listfiles_db(client, r, uri);
#else
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
#endif

	case LocatedUri::Type::PATH:
		/* list local directory */
		return handle_listfiles_local(r, located_uri.path);
	}

	gcc_unreachable();
}

class PrintTagHandler final : public NullTagHandler {
	Response &response;

public:
	explicit PrintTagHandler(Response &_response) noexcept
		:NullTagHandler(WANT_TAG), response(_response) {}

	void OnTag(TagType type, std::string_view value) noexcept override {
		if (response.GetClient().tag_mask.Test(type))
			tag_print(response, type, value);
	}
};

static CommandResult
handle_lsinfo_absolute(Response &r, const char *uri)
{
	PrintTagHandler h(r);
	if (!tag_stream_scan(uri, h)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such file");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

static CommandResult
handle_lsinfo_relative(Client &client, Response &r, const char *uri)
{
#ifdef ENABLE_DATABASE
	if (CommandResult result = handle_lsinfo2(client, uri, r);
	    result != CommandResult::OK)
		return result;
#else
	(void)client;
#endif

	if (!client.ProtocolFeatureEnabled(PF_HIDE_PLAYLISTS_IN_ROOT) && isRootDirectory(uri)) {
		try {
			print_spl_list(r, ListPlaylistFiles());
		} catch (...) {
			LogError(std::current_exception());
		}
	} else {
#ifndef ENABLE_DATABASE
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
#endif
	}

	return CommandResult::OK;
}

static CommandResult
handle_lsinfo_path(Client &, Response &r,
		   const char *path_utf8, Path path_fs)
{
	DetachedSong song(path_utf8);
	if (!song.LoadFile(path_fs)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such file");
		return CommandResult::ERROR;
	}

	song_print_info(r, song);
	return CommandResult::OK;
}

CommandResult
handle_lsinfo(Client &client, Request args, Response &r)
{
	/* default is root directory */
	auto uri = args.GetOptional(0, "");
	if (StringIsEqual(uri, "/"))
		/* this URI is malformed, but some clients are buggy
		   and use "lsinfo /" to list files in the music root
		   directory, which was never intended to work, but
		   once did; in order to retain backwards
		   compatibility, work around this here */
		uri = "";

	const auto located_uri = LocateUri(UriPluginKind::INPUT, uri, &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );

	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		return handle_lsinfo_absolute(r, located_uri.canonical_uri);

	case LocatedUri::Type::RELATIVE:
		return handle_lsinfo_relative(client, r,
					      located_uri.canonical_uri);

	case LocatedUri::Type::PATH:
		/* print information about an arbitrary local file */
		return handle_lsinfo_path(client, r, located_uri.canonical_uri,
					  located_uri.path);
	}

	gcc_unreachable();
}

#ifdef ENABLE_DATABASE

static CommandResult
handle_update(Response &r, UpdateService &update,
	      const char *uri_utf8, bool discard)
{
	unsigned ret = update.Enqueue(uri_utf8, discard);
	r.Fmt(FMT_STRING("updating_db: {}\n"), ret);
	return CommandResult::OK;
}

static CommandResult
handle_update(Response &r, Database &db,
	      const char *uri_utf8, bool discard)
{
	unsigned id = db.Update(uri_utf8, discard);
	if (id > 0) {
		r.Fmt(FMT_STRING("updating_db: {}\n"), id);
		return CommandResult::OK;
	} else {
		/* Database::Update() has returned 0 without setting
		   the Error: the method is not implemented */
		r.Error(ACK_ERROR_NO_EXIST, "Not implemented");
		return CommandResult::ERROR;
	}
}

#endif

static CommandResult
handle_update(Client &client, Request args, Response &r, bool discard)
{
#ifdef ENABLE_DATABASE
	const char *path = "";

	assert(args.size() <= 1);
	if (!args.empty()) {
		path = args.front();

		if (*path == 0 || StringIsEqual(path, "/"))
			/* backwards compatibility with MPD 0.15 */
			path = "";
		else if (!uri_safe_local(path)) {
			r.Error(ACK_ERROR_ARG, "Malformed path");
			return CommandResult::ERROR;
		}
	}

	if (auto *update = client.GetInstance().update)
		return handle_update(r, *update, path, discard);

	if (auto *db = client.GetInstance().GetDatabase())
		return handle_update(r, *db, path, discard);
#else
	(void)client;
	(void)args;
	(void)discard;
#endif

	r.Error(ACK_ERROR_NO_EXIST, "No database");
	return CommandResult::ERROR;
}

CommandResult
handle_update(Client &client, Request args, [[maybe_unused]] Response &r)
{
	return handle_update(client, args, r, false);
}

CommandResult
handle_rescan(Client &client, Request args, Response &r)
{
	return handle_update(client, args, r, true);
}

CommandResult
handle_getvol(Client &client, Request, Response &r)
{
	auto &partition = client.GetPartition();

	const auto volume = partition.mixer_memento.GetVolume(partition.outputs);
	if (volume >= 0)
		r.Fmt(FMT_STRING("volume: {}\n"), volume);

	return CommandResult::OK;
}

CommandResult
handle_setvol(Client &client, Request args, Response &)
{
	unsigned level = args.ParseUnsigned(0, 100);

	auto &partition = client.GetPartition();
	partition.mixer_memento.SetVolume(partition.outputs, level);
	partition.EmitIdle(IDLE_MIXER);
	return CommandResult::OK;
}

CommandResult
handle_volume(Client &client, Request args, Response &r)
{
	int relative = args.ParseInt(0, -100, 100);

	auto &partition = client.GetPartition();
	auto &outputs = partition.outputs;
	auto &mixer_memento = partition.mixer_memento;

	const int old_volume = mixer_memento.GetVolume(outputs);
	if (old_volume < 0) {
		r.Error(ACK_ERROR_SYSTEM, "No mixer");
		return CommandResult::ERROR;
	}

	int new_volume = old_volume + relative;
	if (new_volume < 0)
		new_volume = 0;
	else if (new_volume > 100)
		new_volume = 100;

	if (new_volume != old_volume) {
		mixer_memento.SetVolume(outputs, new_volume);
		partition.EmitIdle(IDLE_MIXER);
	}

	return CommandResult::OK;
}

CommandResult
handle_stats(Client &client, [[maybe_unused]] Request args, Response &r)
{
	stats_print(r, client.GetPartition());
	return CommandResult::OK;
}

CommandResult
handle_config(Client &client, [[maybe_unused]] Request args, Response &r)
{
	if (!client.IsLocal()) {
		r.Error(ACK_ERROR_PERMISSION,
			"Command only permitted to local clients");
		return CommandResult::ERROR;
	}

#ifdef ENABLE_DATABASE
	if (const Storage *storage = client.GetStorage()) {
		const auto path = storage->MapUTF8("");
		r.Fmt(FMT_STRING("music_directory: {}\n"), path);
	}
#endif

	if (const auto spl_path = map_spl_path(); !spl_path.IsNull())
		r.Fmt(FMT_STRING("playlist_directory: {}\n"), spl_path.ToUTF8());

#ifdef HAVE_PCRE
	r.Write("pcre: 1\n");
#endif

	return CommandResult::OK;
}

CommandResult
handle_idle(Client &client, Request args, Response &r)
{
	unsigned flags = 0;
	for (const char *i : args) {
		unsigned event = idle_parse_name(i);
		if (event == 0) {
			r.FmtError(ACK_ERROR_ARG,
				   FMT_STRING("Unrecognized idle event: {}"),
				   i);
			return CommandResult::ERROR;
		}

		flags |= event;
	}

	/* No argument means that the client wants to receive everything */
	if (flags == 0)
		flags = ~0;

	/* enable "idle" mode on this client */
	client.IdleWait(flags);

	return CommandResult::IDLE;
}
