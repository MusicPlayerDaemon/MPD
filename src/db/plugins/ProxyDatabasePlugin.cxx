// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ProxyDatabasePlugin.hxx"
#include "db/Interface.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/DatabaseListener.hxx"
#include "db/Selection.hxx"
#include "db/VHelper.hxx"
#include "db/DatabaseError.hxx"
#include "db/PlaylistInfo.hxx"
#include "db/LightDirectory.hxx"
#include "song/LightSong.hxx"
#include "db/Stats.hxx"
#include "song/Filter.hxx"
#include "song/UriSongFilter.hxx"
#include "song/BaseSongFilter.hxx"
#include "song/TagSongFilter.hxx"
#include "config/Block.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "tag/ParseName.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/RecursiveMap.hxx"
#include "util/ScopeExit.hxx"
#include "protocol/Ack.hxx"
#include "event/SocketEvent.hxx"
#include "event/IdleEvent.hxx"
#include "Log.hxx"

#include <mpd/client.h>
#include <mpd/async.h>

#include <cassert>
#include <list>
#include <string>
#include <utility>

class LibmpdclientError final : public std::runtime_error {
	enum mpd_error code;

public:
	LibmpdclientError(enum mpd_error _code, const char *_msg)
		:std::runtime_error(_msg), code(_code) {}

	[[nodiscard]] enum mpd_error GetCode() const {
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

	AllocatedProxySong(const AllocatedProxySong &) = delete;
	AllocatedProxySong &operator=(const AllocatedProxySong &) = delete;
};

class ProxyDatabase final : public Database {
	SocketEvent socket_event;
	IdleEvent idle_event;

	DatabaseListener &listener;

	const std::string host;
	const std::string password;
	const unsigned port;
	const bool keepalive;

	struct mpd_connection *connection;

	/* this is mutable because GetStats() must be "const" */
	mutable std::chrono::system_clock::time_point update_stamp;

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

	static DatabasePtr Create(EventLoop &main_event_loop,
				  EventLoop &io_event_loop,
				  DatabaseListener &listener,
				  const ConfigBlock &block);

	void Open() override;
	void Close() noexcept override;
	const LightSong *GetSong(std::string_view uri_utf8) const override;
	void ReturnSong(const LightSong *song) const noexcept override;

	void Visit(const DatabaseSelection &selection,
		   VisitDirectory visit_directory,
		   VisitSong visit_song,
		   VisitPlaylist visit_playlist) const override;

	RecursiveMap<std::string> CollectUniqueTags(const DatabaseSelection &selection,
						    std::span<const TagType> tag_types) const override;

	DatabaseStats GetStats(const DatabaseSelection &selection) const override;

	unsigned Update(const char *uri_utf8, bool discard) override;

	std::chrono::system_clock::time_point GetUpdateStamp() const noexcept override {
		return update_stamp;
	}

private:
	void Connect();
	void CheckConnection();
	void EnsureConnected();

	void Disconnect() noexcept;

	void OnSocketReady(unsigned flags) noexcept;
	void OnIdle() noexcept;
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
	{ TAG_ORIGINAL_DATE, MPD_TAG_ORIGINAL_DATE },
	{ TAG_COMPOSER, MPD_TAG_COMPOSER },
	{ TAG_PERFORMER, MPD_TAG_PERFORMER },
	{ TAG_COMMENT, MPD_TAG_COMMENT },
	{ TAG_DISC, MPD_TAG_DISC },
	{ TAG_MUSICBRAINZ_ARTISTID, MPD_TAG_MUSICBRAINZ_ARTISTID },
	{ TAG_MUSICBRAINZ_ALBUMID, MPD_TAG_MUSICBRAINZ_ALBUMID },
	{ TAG_MUSICBRAINZ_ALBUMARTISTID,
	  MPD_TAG_MUSICBRAINZ_ALBUMARTISTID },
	{ TAG_MUSICBRAINZ_TRACKID, MPD_TAG_MUSICBRAINZ_TRACKID },
	{ TAG_MUSICBRAINZ_RELEASETRACKID,
	  MPD_TAG_MUSICBRAINZ_RELEASETRACKID },
	{ TAG_ARTIST_SORT, MPD_TAG_ARTIST_SORT },
	{ TAG_ALBUM_ARTIST_SORT, MPD_TAG_ALBUM_ARTIST_SORT },
	{ TAG_ALBUM_SORT, MPD_TAG_ALBUM_SORT },
#if LIBMPDCLIENT_CHECK_VERSION(2,17,0)
	{ TAG_WORK, MPD_TAG_WORK },
	{ TAG_CONDUCTOR, MPD_TAG_CONDUCTOR },
	{ TAG_LABEL, MPD_TAG_LABEL },
	{ TAG_GROUPING, MPD_TAG_GROUPING },
	{ TAG_MUSICBRAINZ_WORKID, MPD_TAG_MUSICBRAINZ_WORKID },
#endif
#if LIBMPDCLIENT_CHECK_VERSION(2,20,0)
	{ TAG_COMPOSERSORT, MPD_TAG_COMPOSER_SORT },
	{ TAG_ENSEMBLE, MPD_TAG_ENSEMBLE },
	{ TAG_MOVEMENT, MPD_TAG_MOVEMENT },
	{ TAG_MOVEMENTNUMBER, MPD_TAG_MOVEMENTNUMBER },
	{ TAG_LOCATION, MPD_TAG_LOCATION },
#endif
#if LIBMPDCLIENT_CHECK_VERSION(2,21,0)
	{ TAG_MOOD, MPD_TAG_MOOD },
	{ TAG_MUSICBRAINZ_RELEASEGROUPID,
	  MPD_TAG_MUSICBRAINZ_RELEASEGROUPID },
#endif
	{ TAG_NUM_OF_ITEM_TYPES, MPD_TAG_COUNT }
};

static void
Copy(TagBuilder &tag, TagType d_tag,
     const struct mpd_song *song, enum mpd_tag_type s_tag) noexcept
{

	for (unsigned i = 0;; ++i) {
		const char *value = mpd_song_get_tag(song, s_tag, i);
		if (value == nullptr)
			break;

		tag.AddItem(d_tag, value);
	}
}

ProxySong::ProxySong(const mpd_song *song)
	:LightSong(mpd_song_get_uri(song), tag2)
{
	const auto _mtime = mpd_song_get_last_modified(song);
	if (_mtime > 0)
		mtime = std::chrono::system_clock::from_time_t(_mtime);

#if LIBMPDCLIENT_CHECK_VERSION(2,21,0)
	if (const auto _added = mpd_song_get_added(song); _added > 0)
		added = std::chrono::system_clock::from_time_t(_added);
#endif

	start_time = SongTime::FromS(mpd_song_get_start(song));
	end_time = SongTime::FromS(mpd_song_get_end(song));

	const auto *af = mpd_song_get_audio_format(song);
	if (af != nullptr) {
		if (audio_valid_sample_rate(af->sample_rate))
			audio_format.sample_rate = af->sample_rate;


		switch (af->bits) {
		case MPD_SAMPLE_FORMAT_FLOAT:
			audio_format.format = SampleFormat::FLOAT;
			break;

		case MPD_SAMPLE_FORMAT_DSD:
			audio_format.format = SampleFormat::DSD;
			break;

		case 8:
			audio_format.format = SampleFormat::S8;
			break;

		case 16:
			audio_format.format = SampleFormat::S16;
			break;

		case 24:
			audio_format.format = SampleFormat::S24_P32;
			break;

		case 32:
			audio_format.format = SampleFormat::S32;
			break;
		}

		if (audio_valid_channel_count(af->channels))
			audio_format.channels = af->channels;
	}

	TagBuilder tag_builder;

	const unsigned duration = mpd_song_get_duration(song);
	if (duration > 0)
		tag_builder.SetDuration(SignedSongTime::FromS(duration));

	for (const auto *i = &tag_table[0]; i->d != TAG_NUM_OF_ITEM_TYPES; ++i)
		Copy(tag_builder, i->d, song, i->s);

	tag_builder.Commit(tag2);
}

[[gnu::const]]
static enum mpd_tag_type
Convert(TagType tag_type) noexcept
{
	for (auto i = &tag_table[0]; i->d != TAG_NUM_OF_ITEM_TYPES; ++i)
		if (i->d == tag_type)
			return i->s;

	return MPD_TAG_COUNT;
}

[[noreturn]]
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
SendConstraints(mpd_connection *connection, const SongFilter &filter)
{
	return mpd_search_add_expression(connection,
					 filter.ToExpression().c_str());
}

static bool
SendConstraints(mpd_connection *connection, const DatabaseSelection &selection,
		const RangeArg &window)
{
	if (!selection.uri.empty() &&
	    !mpd_search_add_base_constraint(connection,
					    MPD_OPERATOR_DEFAULT,
					    selection.uri.c_str()))
		return false;

	if (selection.filter != nullptr &&
	    !SendConstraints(connection, *selection.filter))
		return false;

	if (selection.sort != TAG_NUM_OF_ITEM_TYPES) {
		if (selection.sort == SORT_TAG_LAST_MODIFIED) {
			if (!mpd_search_add_sort_name(connection, "Last-Modified",
						      selection.descending))
				return false;
#if LIBMPDCLIENT_CHECK_VERSION(2,21,0)
		} else if (selection.sort == SORT_TAG_ADDED) {
			if (!mpd_search_add_sort_name(connection, "Added",
						      selection.descending))
				return false;
#endif
		} else {
			const auto sort = Convert(selection.sort);
			/* if this is an unsupported tag, the sort
			   will be done later by class
			   DatabaseVisitorHelper */
			if (sort != MPD_TAG_COUNT &&
			    !mpd_search_add_sort_tag(connection, sort,
						     selection.descending))
				return false;
		}
	}

	if (window != RangeArg::All() &&
	    !mpd_search_add_window(connection, window.start, window.end))
		return false;

	return true;
}

static bool
SendGroup(mpd_connection *connection, TagType group)
{
	assert(group != TAG_NUM_OF_ITEM_TYPES);

	const auto tag = Convert(group);
	if (tag == MPD_TAG_COUNT)
		throw std::runtime_error("Unsupported tag");

	return mpd_search_add_group_tag(connection, tag);
}

static bool
SendGroup(mpd_connection *connection, std::span<const TagType> group)
{
	while (!group.empty()) {
		if (!SendGroup(connection, group.back()))
		    return false;

		group = group.first(group.size() - 1);
	}

	return true;
}

ProxyDatabase::ProxyDatabase(EventLoop &_loop, DatabaseListener &_listener,
			     const ConfigBlock &block)
	:Database(proxy_db_plugin),
	 socket_event(_loop, BIND_THIS_METHOD(OnSocketReady)),
	 idle_event(_loop, BIND_THIS_METHOD(OnIdle)),
	 listener(_listener),
	 host(block.GetBlockValue("host", "")),
	 password(block.GetBlockValue("password", "")),
	 port(block.GetBlockValue("port", 0U)),
	 keepalive(block.GetBlockValue("keepalive", false))
{
}

DatabasePtr
ProxyDatabase::Create(EventLoop &loop, EventLoop &,
		      DatabaseListener &listener,
		      const ConfigBlock &block)
{
	return std::make_unique<ProxyDatabase>(loop, listener, block);
}

void
ProxyDatabase::Open()
{
	update_stamp = std::chrono::system_clock::time_point::min();

	try {
		Connect();
	} catch (...) {
		/* this error is non-fatal, because this plugin will
		   attempt to reconnect again automatically */
		LogError(std::current_exception());
	}
}

void
ProxyDatabase::Close() noexcept
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

		if (mpd_connection_cmp_server_version(connection, 0, 21, 0) < 0) {
			const unsigned *version =
				mpd_connection_get_server_version(connection);
			throw FmtRuntimeError("Connect to MPD {}.{}.{}, but this "
					      "plugin requires at least version 0.21",
					      version[0], version[1], version[2]);
		}

		if (!password.empty() &&
		    !mpd_run_password(connection, password.c_str()))
			ThrowError(connection);
	} catch (...) {
		mpd_connection_free(connection);
		connection = nullptr;

		std::throw_with_nested(host.empty()
				       ? std::runtime_error("Failed to connect to remote MPD")
				       : FmtRuntimeError("Failed to connect to remote MPD '{}'",
							 host));
	}

	mpd_connection_set_keepalive(connection, keepalive);

	idle_received = ~0U;
	is_idle = false;

	socket_event.Open(SocketDescriptor(mpd_async_get_fd(mpd_connection_get_async(connection))));
	idle_event.Schedule();
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
		idle_event.Schedule();
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
ProxyDatabase::Disconnect() noexcept
{
	assert(connection != nullptr);

	idle_event.Cancel();
	socket_event.ReleaseSocket();

	mpd_connection_free(connection);
	connection = nullptr;
}

void
ProxyDatabase::OnSocketReady([[maybe_unused]] unsigned flags) noexcept
{
	assert(connection != nullptr);

	if (!is_idle) {
		// TODO: can this happen?
		idle_event.Schedule();
		socket_event.Cancel();
		return;
	}

	auto idle = (unsigned)mpd_recv_idle(connection, false);
	if (idle == 0) {
		try {
			CheckError(connection);
		} catch (...) {
			LogError(std::current_exception());
			Disconnect();
			return;
		}
	}

	/* let OnIdle() handle this */
	idle_received |= idle;
	is_idle = false;
	idle_event.Schedule();
	socket_event.Cancel();
}

void
ProxyDatabase::OnIdle() noexcept
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
		} catch (...) {
			LogError(std::current_exception());
		}

		socket_event.ReleaseSocket();
		mpd_connection_free(connection);
		connection = nullptr;
		return;
	}

	is_idle = true;
	socket_event.ScheduleRead();
}

const LightSong *
ProxyDatabase::GetSong(std::string_view uri) const
{
	// TODO: eliminate the const_cast
	const_cast<ProxyDatabase *>(this)->EnsureConnected();

	if (!mpd_send_list_meta(connection, std::string(uri).c_str()))
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
ProxyDatabase::ReturnSong(const LightSong *_song) const noexcept
{
	assert(_song != nullptr);

	auto *song = (AllocatedProxySong *)
		const_cast<LightSong *>(_song);
	delete song;
}

static void
Visit(struct mpd_connection *connection, const char *uri,
      bool recursive, const SongFilter *filter,
      const VisitDirectory& visit_directory, const VisitSong& visit_song,
      const VisitPlaylist& visit_playlist);

static void
Visit(struct mpd_connection *connection,
      bool recursive, const SongFilter *filter,
      const struct mpd_directory *directory,
      const VisitDirectory& visit_directory, const VisitSong& visit_song,
      const VisitPlaylist& visit_playlist)
{
	const char *path = mpd_directory_get_path(directory);

	std::chrono::system_clock::time_point mtime =
		std::chrono::system_clock::time_point::min();
	time_t _mtime = mpd_directory_get_last_modified(directory);
	if (_mtime > 0)
		mtime = std::chrono::system_clock::from_time_t(_mtime);

	if (visit_directory)
		visit_directory(LightDirectory(path, mtime));

	if (recursive)
		Visit(connection, path, recursive, filter,
		      visit_directory, visit_song, visit_playlist);
}

[[gnu::pure]]
static bool
Match(const SongFilter *filter, const LightSong &song) noexcept
{
	return filter == nullptr || filter->Match(song);
}

static void
Visit(const SongFilter *filter,
      const mpd_song *_song,
      const VisitSong& visit_song)
{
	if (!visit_song)
		return;

	const ProxySong song(_song);
	if (Match(filter, song))
		visit_song(song);
}

static void
Visit(const struct mpd_playlist *playlist,
      const VisitPlaylist& visit_playlist)
{
	if (!visit_playlist)
		return;

	time_t mtime = mpd_playlist_get_last_modified(playlist);

	PlaylistInfo p(mpd_playlist_get_path(playlist),
		       mtime > 0
		       ? std::chrono::system_clock::from_time_t(mtime)
		       : std::chrono::system_clock::time_point::min());

	visit_playlist(p, LightDirectory::Root());
}

class ProxyEntity {
	struct mpd_entity *entity;

public:
	explicit ProxyEntity(struct mpd_entity *_entity) noexcept
		:entity(_entity) {}

	ProxyEntity(const ProxyEntity &other) = delete;

	ProxyEntity(ProxyEntity &&other) noexcept
		:entity(other.entity) {
		other.entity = nullptr;
	}

	~ProxyEntity() noexcept {
		if (entity != nullptr)
			mpd_entity_free(entity);
	}

	ProxyEntity &operator=(const ProxyEntity &other) = delete;

	operator const struct mpd_entity *() const noexcept {
		return entity;
	}
};

static std::list<ProxyEntity>
ReceiveEntities(struct mpd_connection *connection) noexcept
{
	std::list<ProxyEntity> entities;
	struct mpd_entity *entity;
	while ((entity = mpd_recv_entity(connection)) != nullptr)
		entities.emplace_back(entity);

	mpd_response_finish(connection);
	return entities;
}

static void
Visit(struct mpd_connection *connection, const char *uri,
      bool recursive, const SongFilter *filter,
      const VisitDirectory& visit_directory, const VisitSong& visit_song,
      const VisitPlaylist& visit_playlist)
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
	    const VisitSong& visit_song)
try {
	assert(selection.recursive);
	assert(visit_song);

	const bool exact = selection.filter == nullptr ||
		!selection.filter->HasFoldCase();

	/* request only this number of songs at a time to avoid
	   blowing the server's max_output_buffer_size limit */
	constexpr unsigned LIMIT = 4096;

	auto remaining_window = selection.window;

	while (remaining_window.start < remaining_window.end) {
		auto window = remaining_window;
		if (window.end - window.start > LIMIT)
			window.end = window.start + LIMIT;

		if (!mpd_search_db_songs(connection, exact) ||
		    !SendConstraints(connection, selection, window) ||
		    !mpd_search_commit(connection))
			ThrowError(connection);

		while (auto *song = mpd_recv_song(connection)) {
			++window.start;

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

		if (window.start != window.end)
			/* the other MPD has given us less than we
			   requested - this means there's no more
			   data */
			break;

		remaining_window.start = window.end;
	}
} catch (...) {
	if (connection != nullptr)
		mpd_search_cancel(connection);

	throw;
}

[[gnu::pure]]
static bool
IsSortSupported(TagType tag_type) noexcept
{
	if (tag_type == TagType(SORT_TAG_LAST_MODIFIED)) {
		return true;
	}

#if LIBMPDCLIENT_CHECK_VERSION(2,21,0)
	if (tag_type == TagType(SORT_TAG_ADDED)) {
		return true;
	}
#endif

	return Convert(tag_type) != MPD_TAG_COUNT;
}

[[gnu::pure]]
static DatabaseSelection
CheckSelection(DatabaseSelection selection) noexcept
{
	selection.uri.clear();
	selection.filter = nullptr;

	if (selection.sort != TAG_NUM_OF_ITEM_TYPES &&
	    IsSortSupported(selection.sort))
		/* we can forward the "sort" parameter to the other
		   MPD */
		selection.sort = TAG_NUM_OF_ITEM_TYPES;

	if (selection.window != RangeArg::All())
		/* we can forward the "window" parameter to the other
		   MPD */
		selection.window = RangeArg::All();

	return selection;
}

void
ProxyDatabase::Visit(const DatabaseSelection &selection,
		     VisitDirectory visit_directory,
		     VisitSong visit_song,
		     VisitPlaylist visit_playlist) const
{
	// TODO: eliminate the const_cast
	const_cast<ProxyDatabase *>(this)->EnsureConnected();

	DatabaseVisitorHelper helper(CheckSelection(selection),
				     visit_song);

	if (!visit_directory && !visit_playlist && selection.recursive &&
	    selection.IsFiltered()) {
		/* this optimized code path can only be used under
		   certain conditions */
		::SearchSongs(connection, selection, visit_song);
		helper.Commit();
		return;
	}

	/* fall back to recursive walk (slow!) */
	::Visit(connection, selection.uri.c_str(),
		selection.recursive, selection.filter,
		visit_directory, visit_song, visit_playlist);

	helper.Commit();
}

RecursiveMap<std::string>
ProxyDatabase::CollectUniqueTags(const DatabaseSelection &selection,
				 std::span<const TagType> tag_types) const
try {
	// TODO: eliminate the const_cast
	const_cast<ProxyDatabase *>(this)->EnsureConnected();

	enum mpd_tag_type tag_type2 = Convert(tag_types.back());
	if (tag_type2 == MPD_TAG_COUNT)
		throw std::runtime_error("Unsupported tag");

	const auto group = tag_types.first(tag_types.size() - 1);

	if (!mpd_search_db_tags(connection, tag_type2) ||
	    !SendConstraints(connection, selection, selection.window) ||
	    !SendGroup(connection, group))
		ThrowError(connection);

	if (!mpd_search_commit(connection))
		ThrowError(connection);

	RecursiveMap<std::string> result;
	std::vector<RecursiveMap<std::string> *> position;
	position.emplace_back(&result);

	while (auto *pair = mpd_recv_pair(connection)) {
		AtScopeExit(this, pair) {
			mpd_return_pair(connection, pair);
		};

		const auto current_type = tag_name_parse_i(pair->name);
		if (current_type == TAG_NUM_OF_ITEM_TYPES)
			continue;

		auto it = std::find(tag_types.begin(), tag_types.end(),
				    current_type);
		if (it == tag_types.end())
			continue;

		size_t i = std::distance(tag_types.begin(), it);
		if (i > position.size())
			continue;

		if (i + 1 < position.size())
			position.resize(i + 1);

		auto &parent = *position[i];
		position.emplace_back(&parent[pair->value]);
	}

	if (!mpd_response_finish(connection))
		ThrowError(connection);

	return result;
} catch (...) {
	if (connection != nullptr)
		mpd_search_cancel(connection);

	throw;
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

	update_stamp = std::chrono::system_clock::from_time_t(mpd_stats_get_db_update_time(stats2));

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
