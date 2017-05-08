/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "ProxyDatabasePlugin.hxx"
#include "db/Interface.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/DatabaseListener.hxx"
#include "db/Selection.hxx"
#include "db/DatabaseError.hxx"
#include "db/PlaylistInfo.hxx"
#include "db/LightDirectory.hxx"
#include "db/LightSong.hxx"
#include "db/Stats.hxx"
#include "SongFilter.hxx"
#include "Compiler.h"
#include "config/Block.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/Tag.hxx"
#include "util/ScopeExit.hxx"
#include "util/RuntimeError.hxx"
#include "protocol/Ack.hxx"
#include "event/SocketMonitor.hxx"
#include "event/IdleMonitor.hxx"
#include "Log.hxx"

#include <mpd/client.h>
#include <mpd/async.h>

#include <cassert>
#include <string>
#include <list>

class LibmpdclientError final : public std::runtime_error {
	enum mpd_error code;

public:
	LibmpdclientError(enum mpd_error _code, const char *_msg)
		:std::runtime_error(_msg), code(_code) {}

	enum mpd_error GetCode() const {
		return code;
	}
};

class ProxySong : public LightSong {
	Tag tag2;

public:
	explicit ProxySong(const mpd_song *song);
};

class AllocatedProxySong : public ProxySong {
	mpd_song *const song;

public:
	explicit AllocatedProxySong(mpd_song *_song)
		:ProxySong(_song), song(_song) {}

	~AllocatedProxySong() {
		mpd_song_free(song);
	}
};

class ProxyDatabase final : public Database, SocketMonitor, IdleMonitor {
	DatabaseListener &listener;

	const std::string host;
	const unsigned port;
	const bool keepalive;

	struct mpd_connection *connection;

	/* this is mutable because GetStats() must be "const" */
	mutable time_t update_stamp;

	/**
	 * The libmpdclient idle mask that was removed from the other
	 * MPD.  This will be handled by the next OnIdle() call.
	 */
	unsigned idle_received;

	/**
	 * Is the #connection currently "idle"?  That is, did we send
	 * the "idle" command to it?
	 */
	bool is_idle;

public:
	ProxyDatabase(EventLoop &_loop, DatabaseListener &_listener,
		      const ConfigBlock &block);

	static Database *Create(EventLoop &loop, DatabaseListener &listener,
				const ConfigBlock &block);

	void Open() override;
	void Close() override;
	const LightSong *GetSong(const char *uri_utf8) const override;
	void ReturnSong(const LightSong *song) const override;

	void Visit(const DatabaseSelection &selection,
		   VisitDirectory visit_directory,
		   VisitSong visit_song,
		   VisitPlaylist visit_playlist) const override;

	void VisitUniqueTags(const DatabaseSelection &selection,
			     TagType tag_type, tag_mask_t group_mask,
			     VisitTag visit_tag) const override;

	DatabaseStats GetStats(const DatabaseSelection &selection) const override;

	unsigned Update(const char *uri_utf8, bool discard) override;

	time_t GetUpdateStamp() const override {
		return update_stamp;
	}

private:
	void Connect();
	void CheckConnection();
	void EnsureConnected();

	void Disconnect();

	/* virtual methods from SocketMonitor */
	bool OnSocketReady(unsigned flags) override;

	/* virtual methods from IdleMonitor */
	void OnIdle() override;
};

static constexpr struct {
	TagType d;
	enum mpd_tag_type s;
} tag_table[] = {
	{ TAG_ARTIST, MPD_TAG_ARTIST },
	{ TAG_ALBUM, MPD_TAG_ALBUM },
	{ TAG_ALBUM_ARTIST, MPD_TAG_ALBUM_ARTIST },
	{ TAG_TITLE, MPD_TAG_TITLE },
	{ TAG_TRACK, MPD_TAG_TRACK },
	{ TAG_NAME, MPD_TAG_NAME },
	{ TAG_GENRE, MPD_TAG_GENRE },
	{ TAG_DATE, MPD_TAG_DATE },
	{ TAG_COMPOSER, MPD_TAG_COMPOSER },
	{ TAG_PERFORMER, MPD_TAG_PERFORMER },
	{ TAG_COMMENT, MPD_TAG_COMMENT },
	{ TAG_DISC, MPD_TAG_DISC },
	{ TAG_MUSICBRAINZ_ARTISTID, MPD_TAG_MUSICBRAINZ_ARTISTID },
	{ TAG_MUSICBRAINZ_ALBUMID, MPD_TAG_MUSICBRAINZ_ALBUMID },
	{ TAG_MUSICBRAINZ_ALBUMARTISTID,
	  MPD_TAG_MUSICBRAINZ_ALBUMARTISTID },
	{ TAG_MUSICBRAINZ_TRACKID, MPD_TAG_MUSICBRAINZ_TRACKID },
#if LIBMPDCLIENT_CHECK_VERSION(2,10,0)
	{ TAG_MUSICBRAINZ_RELEASETRACKID,
	  MPD_TAG_MUSICBRAINZ_RELEASETRACKID },
#endif
	{ TAG_NUM_OF_ITEM_TYPES, MPD_TAG_COUNT }
};

static void
Copy(TagBuilder &tag, TagType d_tag,
     const struct mpd_song *song, enum mpd_tag_type s_tag)
{

	for (unsigned i = 0;; ++i) {
		const char *value = mpd_song_get_tag(song, s_tag, i);
		if (value == nullptr)
			break;

		tag.AddItem(d_tag, value);
	}
}

ProxySong::ProxySong(const mpd_song *song)
{
	directory = nullptr;
	uri = mpd_song_get_uri(song);
	real_uri = nullptr;
	tag = &tag2;
	mtime = mpd_song_get_last_modified(song);

#if LIBMPDCLIENT_CHECK_VERSION(2,3,0)
	start_time = SongTime::FromS(mpd_song_get_start(song));
	end_time = SongTime::FromS(mpd_song_get_end(song));
#else
	start_time = end_time = SongTime::zero();
#endif

	TagBuilder tag_builder;

	const unsigned duration = mpd_song_get_duration(song);
	if (duration > 0)
		tag_builder.SetDuration(SignedSongTime::FromS(duration));

	for (const auto *i = &tag_table[0]; i->d != TAG_NUM_OF_ITEM_TYPES; ++i)
		Copy(tag_builder, i->d, song, i->s);

	tag_builder.Commit(tag2);
}

gcc_const
static enum mpd_tag_type
Convert(TagType tag_type) noexcept
{
	for (auto i = &tag_table[0]; i->d != TAG_NUM_OF_ITEM_TYPES; ++i)
		if (i->d == tag_type)
			return i->s;

	return MPD_TAG_COUNT;
}

static void
ThrowError(struct mpd_connection *connection)
{
	const auto code = mpd_connection_get_error(connection);

	AtScopeExit(connection) {
		mpd_connection_clear_error(connection);
	};

	if (code == MPD_ERROR_SERVER) {
		/* libmpdclient's "enum mpd_server_error" is the same
		   as our "enum ack" */
		const auto server_error =
			mpd_connection_get_server_error(connection);
		throw ProtocolError((enum ack)server_error,
				    mpd_connection_get_error_message(connection));
	} else {
		throw LibmpdclientError(code,
					mpd_connection_get_error_message(connection));
	}
}

static void
CheckError(struct mpd_connection *connection)
{
	const auto code = mpd_connection_get_error(connection);
	if (code != MPD_ERROR_SUCCESS)
		ThrowError(connection);
}

static bool
SendConstraints(mpd_connection *connection, const SongFilter::Item &item)
{
	switch (item.GetTag()) {
		mpd_tag_type tag;

#if LIBMPDCLIENT_CHECK_VERSION(2,9,0)
	case LOCATE_TAG_BASE_TYPE:
		if (mpd_connection_cmp_server_version(connection, 0, 18, 0) < 0)
			/* requires MPD 0.18 */
			return true;

		return mpd_search_add_base_constraint(connection,
						      MPD_OPERATOR_DEFAULT,
						      item.GetValue());
#endif

	case LOCATE_TAG_FILE_TYPE:
		return mpd_search_add_uri_constraint(connection,
						     MPD_OPERATOR_DEFAULT,
						     item.GetValue());

	case LOCATE_TAG_ANY_TYPE:
		return mpd_search_add_any_tag_constraint(connection,
							 MPD_OPERATOR_DEFAULT,
							 item.GetValue());

	default:
		tag = Convert(TagType(item.GetTag()));
		if (tag == MPD_TAG_COUNT)
			return true;

		return mpd_search_add_tag_constraint(connection,
						     MPD_OPERATOR_DEFAULT,
						     tag,
						     item.GetValue());
	}
}

static bool
SendConstraints(mpd_connection *connection, const SongFilter &filter)
{
	for (const auto &i : filter.GetItems())
		if (!SendConstraints(connection, i))
			return false;

	return true;
}

static bool
SendConstraints(mpd_connection *connection, const DatabaseSelection &selection)
{
#if LIBMPDCLIENT_CHECK_VERSION(2,9,0)
	if (!selection.uri.empty() &&
	    mpd_connection_cmp_server_version(connection, 0, 18, 0) >= 0) {
		/* requires MPD 0.18 */
		if (!mpd_search_add_base_constraint(connection,
						    MPD_OPERATOR_DEFAULT,
						    selection.uri.c_str()))
			return false;
	}
#endif

	if (selection.filter != nullptr &&
	    !SendConstraints(connection, *selection.filter))
		return false;

	return true;
}

ProxyDatabase::ProxyDatabase(EventLoop &_loop, DatabaseListener &_listener,
			     const ConfigBlock &block)
	:Database(proxy_db_plugin),
	 SocketMonitor(_loop), IdleMonitor(_loop),
	 listener(_listener),
	 host(block.GetBlockValue("host", "")),
	 port(block.GetBlockValue("port", 0u)),
	 keepalive(block.GetBlockValue("keepalive", false))
{
}

Database *
ProxyDatabase::Create(EventLoop &loop, DatabaseListener &listener,
		      const ConfigBlock &block)
{
	return new ProxyDatabase(loop, listener, block);
}

void
ProxyDatabase::Open()
{
	update_stamp = 0;

	try {
		Connect();
	} catch (const std::runtime_error &error) {
		/* this error is non-fatal, because this plugin will
		   attempt to reconnect again automatically */
		LogError(error);
	}
}

void
ProxyDatabase::Close()
{
	if (connection != nullptr)
		Disconnect();
}

void
ProxyDatabase::Connect()
{
	const char *_host = host.empty() ? nullptr : host.c_str();
	connection = mpd_connection_new(_host, port, 0);
	if (connection == nullptr)
		throw LibmpdclientError(MPD_ERROR_OOM, "Out of memory");

	try {
		CheckError(connection);
	} catch (...) {
		mpd_connection_free(connection);
		connection = nullptr;

		std::throw_with_nested(host.empty()
				       ? std::runtime_error("Failed to connect to remote MPD")
				       : FormatRuntimeError("Failed to connect to remote MPD '%s'",
							    host.c_str()));
	}

#if LIBMPDCLIENT_CHECK_VERSION(2, 10, 0)
	mpd_connection_set_keepalive(connection, keepalive);
#else
	// suppress -Wunused-private-field
	(void)keepalive;
#endif

	idle_received = unsigned(-1);
	is_idle = false;

	SocketMonitor::Open(mpd_async_get_fd(mpd_connection_get_async(connection)));
	IdleMonitor::Schedule();
}

void
ProxyDatabase::CheckConnection()
{
	assert(connection != nullptr);

	if (!mpd_connection_clear_error(connection)) {
		Disconnect();
		Connect();
		return;
	}

	if (is_idle) {
		unsigned idle = mpd_run_noidle(connection);
		if (idle == 0) {
			try {
				CheckError(connection);
			} catch (...) {
				Disconnect();
				throw;
			}
		}

		idle_received |= idle;
		is_idle = false;
		IdleMonitor::Schedule();
	}
}

void
ProxyDatabase::EnsureConnected()
{
	if (connection != nullptr)
		CheckConnection();
	else
		Connect();
}

void
ProxyDatabase::Disconnect()
{
	assert(connection != nullptr);

	IdleMonitor::Cancel();
	SocketMonitor::Steal();

	mpd_connection_free(connection);
	connection = nullptr;
}

bool
ProxyDatabase::OnSocketReady(gcc_unused unsigned flags)
{
	assert(connection != nullptr);

	if (!is_idle) {
		// TODO: can this happen?
		IdleMonitor::Schedule();
		return false;
	}

	unsigned idle = (unsigned)mpd_recv_idle(connection, false);
	if (idle == 0) {
		try {
			CheckError(connection);
		} catch (const std::runtime_error &error) {
			LogError(error);
			Disconnect();
			return false;
		}
	}

	/* let OnIdle() handle this */
	idle_received |= idle;
	is_idle = false;
	IdleMonitor::Schedule();
	return false;
}

void
ProxyDatabase::OnIdle()
{
	assert(connection != nullptr);

	/* handle previous idle events */

	if (idle_received & MPD_IDLE_DATABASE)
		listener.OnDatabaseModified();

	idle_received = 0;

	/* send a new idle command to the other MPD */

	if (is_idle)
		// TODO: can this happen?
		return;

	if (!mpd_send_idle_mask(connection, MPD_IDLE_DATABASE)) {
		try {
			ThrowError(connection);
		} catch (const std::runtime_error &error) {
			LogError(error);
		}

		SocketMonitor::Steal();
		mpd_connection_free(connection);
		connection = nullptr;
		return;
	}

	is_idle = true;
	SocketMonitor::ScheduleRead();
}

const LightSong *
ProxyDatabase::GetSong(const char *uri) const
{
	// TODO: eliminate the const_cast
	const_cast<ProxyDatabase *>(this)->EnsureConnected();

	if (!mpd_send_list_meta(connection, uri))
		ThrowError(connection);

	struct mpd_song *song = mpd_recv_song(connection);
	if (!mpd_response_finish(connection)) {
		if (song != nullptr)
			mpd_song_free(song);
		ThrowError(connection);
	}

	if (song == nullptr)
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	return new AllocatedProxySong(song);
}

void
ProxyDatabase::ReturnSong(const LightSong *_song) const
{
	assert(_song != nullptr);

	AllocatedProxySong *song = (AllocatedProxySong *)
		const_cast<LightSong *>(_song);
	delete song;
}

static void
Visit(struct mpd_connection *connection, const char *uri,
      bool recursive, const SongFilter *filter,
      VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist);

static void
Visit(struct mpd_connection *connection,
      bool recursive, const SongFilter *filter,
      const struct mpd_directory *directory,
      VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist)
{
	const char *path = mpd_directory_get_path(directory);
#if LIBMPDCLIENT_CHECK_VERSION(2,9,0)
	time_t mtime = mpd_directory_get_last_modified(directory);
#else
	time_t mtime = 0;
#endif

	if (visit_directory)
		visit_directory(LightDirectory(path, mtime));

	if (recursive)
		Visit(connection, path, recursive, filter,
		      visit_directory, visit_song, visit_playlist);
}

gcc_pure
static bool
Match(const SongFilter *filter, const LightSong &song) noexcept
{
	return filter == nullptr || filter->Match(song);
}

static void
Visit(const SongFilter *filter,
      const mpd_song *_song,
      VisitSong visit_song)
{
	if (!visit_song)
		return;

	const ProxySong song(_song);
	if (Match(filter, song))
		visit_song(song);
}

static void
Visit(const struct mpd_playlist *playlist,
      VisitPlaylist visit_playlist)
{
	if (!visit_playlist)
		return;

	PlaylistInfo p(mpd_playlist_get_path(playlist),
		       mpd_playlist_get_last_modified(playlist));

	visit_playlist(p, LightDirectory::Root());
}

class ProxyEntity {
	struct mpd_entity *entity;

public:
	explicit ProxyEntity(struct mpd_entity *_entity)
		:entity(_entity) {}

	ProxyEntity(const ProxyEntity &other) = delete;

	ProxyEntity(ProxyEntity &&other)
		:entity(other.entity) {
		other.entity = nullptr;
	}

	~ProxyEntity() {
		if (entity != nullptr)
			mpd_entity_free(entity);
	}

	ProxyEntity &operator=(const ProxyEntity &other) = delete;

	operator const struct mpd_entity *() const {
		return entity;
	}
};

static std::list<ProxyEntity>
ReceiveEntities(struct mpd_connection *connection)
{
	std::list<ProxyEntity> entities;
	struct mpd_entity *entity;
	while ((entity = mpd_recv_entity(connection)) != nullptr)
		entities.push_back(ProxyEntity(entity));

	mpd_response_finish(connection);
	return entities;
}

static void
Visit(struct mpd_connection *connection, const char *uri,
      bool recursive, const SongFilter *filter,
      VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist)
{
	if (!mpd_send_list_meta(connection, uri))
		ThrowError(connection);

	std::list<ProxyEntity> entities(ReceiveEntities(connection));
	CheckError(connection);

	for (const auto &entity : entities) {
		switch (mpd_entity_get_type(entity)) {
		case MPD_ENTITY_TYPE_UNKNOWN:
			break;

		case MPD_ENTITY_TYPE_DIRECTORY:
			Visit(connection, recursive, filter,
			      mpd_entity_get_directory(entity),
			      visit_directory, visit_song, visit_playlist);
			break;

		case MPD_ENTITY_TYPE_SONG:
			Visit(filter, mpd_entity_get_song(entity), visit_song);
			break;

		case MPD_ENTITY_TYPE_PLAYLIST:
			Visit(mpd_entity_get_playlist(entity),
			      visit_playlist);
			break;
		}
	}
}

static void
SearchSongs(struct mpd_connection *connection,
	    const DatabaseSelection &selection,
	    VisitSong visit_song)
{
	assert(selection.recursive);
	assert(visit_song);

	const bool exact = selection.filter == nullptr ||
		!selection.filter->HasFoldCase();

	if (!mpd_search_db_songs(connection, exact) ||
	    !SendConstraints(connection, selection) ||
	    !mpd_search_commit(connection))
		ThrowError(connection);

	while (auto *song = mpd_recv_song(connection)) {
		AllocatedProxySong song2(song);

		if (Match(selection.filter, song2)) {
			try {
				visit_song(song2);
			} catch (...) {
				mpd_response_finish(connection);
				throw;
			}
		}
	}

	if (!mpd_response_finish(connection))
		ThrowError(connection);
}

/**
 * Check whether we can use the "base" constraint.  Requires
 * libmpdclient 2.9 and MPD 0.18.
 */
gcc_pure
static bool
ServerSupportsSearchBase(const struct mpd_connection *connection) noexcept
{
#if LIBMPDCLIENT_CHECK_VERSION(2,9,0)
	return mpd_connection_cmp_server_version(connection, 0, 18, 0) >= 0;
#else
	(void)connection;

	return false;
#endif
}

void
ProxyDatabase::Visit(const DatabaseSelection &selection,
		     VisitDirectory visit_directory,
		     VisitSong visit_song,
		     VisitPlaylist visit_playlist) const
{
	// TODO: eliminate the const_cast
	const_cast<ProxyDatabase *>(this)->EnsureConnected();

	if (!visit_directory && !visit_playlist && selection.recursive &&
	    (ServerSupportsSearchBase(connection)
	     ? !selection.IsEmpty()
	     : selection.HasOtherThanBase())) {
		/* this optimized code path can only be used under
		   certain conditions */
		::SearchSongs(connection, selection, visit_song);
		return;
	}

	/* fall back to recursive walk (slow!) */
	::Visit(connection, selection.uri.c_str(),
		selection.recursive, selection.filter,
		visit_directory, visit_song, visit_playlist);
}

void
ProxyDatabase::VisitUniqueTags(const DatabaseSelection &selection,
			       TagType tag_type,
			       gcc_unused tag_mask_t group_mask,
			       VisitTag visit_tag) const
{
	// TODO: eliminate the const_cast
	const_cast<ProxyDatabase *>(this)->EnsureConnected();

	enum mpd_tag_type tag_type2 = Convert(tag_type);
	if (tag_type2 == MPD_TAG_COUNT)
		throw std::runtime_error("Unsupported tag");

	if (!mpd_search_db_tags(connection, tag_type2) ||
	    !SendConstraints(connection, selection))
		ThrowError(connection);

	// TODO: use group_mask

	if (!mpd_search_commit(connection))
		ThrowError(connection);

	while (auto *pair = mpd_recv_pair_tag(connection, tag_type2)) {
		AtScopeExit(this, pair) {
			mpd_return_pair(connection, pair);
		};

		TagBuilder tag;
		tag.AddItem(tag_type, pair->value);

		if (tag.IsEmpty())
			/* if no tag item has been added, then the
			   given value was not acceptable
			   (e.g. empty); forcefully insert an empty
			   tag in this case, as the caller expects the
			   given tag type to be present */
			tag.AddEmptyItem(tag_type);

		try {
			visit_tag(tag.Commit());
		} catch (...) {
			mpd_response_finish(connection);
			throw;
		}
	}

	if (!mpd_response_finish(connection))
		ThrowError(connection);
}

DatabaseStats
ProxyDatabase::GetStats(const DatabaseSelection &selection) const
{
	// TODO: match
	(void)selection;

	// TODO: eliminate the const_cast
	const_cast<ProxyDatabase *>(this)->EnsureConnected();

	struct mpd_stats *stats2 =
		mpd_run_stats(connection);
	if (stats2 == nullptr)
		ThrowError(connection);

	update_stamp = (time_t)mpd_stats_get_db_update_time(stats2);

	DatabaseStats stats;
	stats.song_count = mpd_stats_get_number_of_songs(stats2);
	stats.total_duration = std::chrono::seconds(mpd_stats_get_db_play_time(stats2));
	stats.artist_count = mpd_stats_get_number_of_artists(stats2);
	stats.album_count = mpd_stats_get_number_of_albums(stats2);
	mpd_stats_free(stats2);
	return stats;
}

unsigned
ProxyDatabase::Update(const char *uri_utf8, bool discard)
{
	EnsureConnected();

	unsigned id = discard
		? mpd_run_rescan(connection, uri_utf8)
		: mpd_run_update(connection, uri_utf8);
	if (id == 0)
		CheckError(connection);

	return id;
}

const DatabasePlugin proxy_db_plugin = {
	"proxy",
	DatabasePlugin::FLAG_REQUIRE_STORAGE,
	ProxyDatabase::Create,
};
