/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DatabasePlugin.hxx"
#include "DatabaseSelection.hxx"
#include "DatabaseError.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "SongFilter.hxx"
#include "Compiler.h"
#include "ConfigData.hxx"
#include "tag/TagBuilder.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "protocol/Ack.hxx"

#include <mpd/client.h>

#include <cassert>
#include <string>
#include <list>

class ProxyDatabase : public Database {
	std::string host;
	unsigned port;

	struct mpd_connection *connection;
	Directory *root;

	/* this is mutable because GetStats() must be "const" */
	mutable time_t update_stamp;

public:
	static Database *Create(const config_param &param,
				Error &error);

	virtual bool Open(Error &error) override;
	virtual void Close() override;
	virtual Song *GetSong(const char *uri_utf8,
				     Error &error) const override;
	virtual void ReturnSong(Song *song) const;

	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   Error &error) const override;

	virtual bool VisitUniqueTags(const DatabaseSelection &selection,
				     TagType tag_type,
				     VisitString visit_string,
				     Error &error) const override;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      Error &error) const override;

	virtual time_t GetUpdateStamp() const override {
		return update_stamp;
	}

private:
	bool Configure(const config_param &param, Error &error);

	bool Connect(Error &error);
	bool CheckConnection(Error &error);
	bool EnsureConnected(Error &error);
};

static constexpr Domain libmpdclient_domain("libmpdclient");

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
	{ TAG_NUM_OF_ITEM_TYPES, MPD_TAG_COUNT }
};

gcc_const
static enum mpd_tag_type
Convert(TagType tag_type)
{
	for (auto i = &tag_table[0]; i->d != TAG_NUM_OF_ITEM_TYPES; ++i)
		if (i->d == tag_type)
			return i->s;

	return MPD_TAG_COUNT;
}

static bool
CheckError(struct mpd_connection *connection, Error &error)
{
	const auto code = mpd_connection_get_error(connection);
	if (code == MPD_ERROR_SUCCESS)
		return true;

	if (code == MPD_ERROR_SERVER) {
		/* libmpdclient's "enum mpd_server_error" is the same
		   as our "enum ack" */
		const auto server_error =
			mpd_connection_get_server_error(connection);
		error.Set(ack_domain, (int)server_error,
			  mpd_connection_get_error_message(connection));
	} else {
		error.Set(libmpdclient_domain, (int)code,
			  mpd_connection_get_error_message(connection));
	}

	mpd_connection_clear_error(connection);
	return false;
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
						      item.GetValue().c_str());
#endif

	case LOCATE_TAG_FILE_TYPE:
		return mpd_search_add_uri_constraint(connection,
						     MPD_OPERATOR_DEFAULT,
						     item.GetValue().c_str());

	case LOCATE_TAG_ANY_TYPE:
		return mpd_search_add_any_tag_constraint(connection,
							 MPD_OPERATOR_DEFAULT,
							 item.GetValue().c_str());

	default:
		tag = Convert(TagType(item.GetTag()));
		if (tag == MPD_TAG_COUNT)
			return true;

		return mpd_search_add_tag_constraint(connection,
						     MPD_OPERATOR_DEFAULT,
						     tag,
						     item.GetValue().c_str());
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

Database *
ProxyDatabase::Create(const config_param &param, Error &error)
{
	ProxyDatabase *db = new ProxyDatabase();
	if (!db->Configure(param, error)) {
		delete db;
		db = nullptr;
	}

	return db;
}

bool
ProxyDatabase::Configure(const config_param &param, gcc_unused Error &error)
{
	host = param.GetBlockValue("host", "");
	port = param.GetBlockValue("port", 0u);

	return true;
}

bool
ProxyDatabase::Open(Error &error)
{
	if (!Connect(error))
		return false;

	root = Directory::NewRoot();
	update_stamp = 0;

	return true;
}

void
ProxyDatabase::Close()
{
	root->Free();

	if (connection != nullptr)
		mpd_connection_free(connection);
}

bool
ProxyDatabase::Connect(Error &error)
{
	const char *_host = host.empty() ? nullptr : host.c_str();
	connection = mpd_connection_new(_host, port, 0);
	if (connection == nullptr) {
		error.Set(libmpdclient_domain, (int)MPD_ERROR_OOM,
			  "Out of memory");
		return false;
	}

	if (!CheckError(connection, error)) {
		if (connection != nullptr) {
			mpd_connection_free(connection);
			connection = nullptr;
		}

		return false;
	}

	return true;
}

bool
ProxyDatabase::CheckConnection(Error &error)
{
	assert(connection != nullptr);

	if (!mpd_connection_clear_error(connection)) {
		mpd_connection_free(connection);
		return Connect(error);
	}

	return true;
}

bool
ProxyDatabase::EnsureConnected(Error &error)
{
	return connection != nullptr
		? CheckConnection(error)
		: Connect(error);
}

static Song *
Convert(const struct mpd_song *song);

Song *
ProxyDatabase::GetSong(const char *uri, Error &error) const
{
	// TODO: eliminate the const_cast
	if (!const_cast<ProxyDatabase *>(this)->EnsureConnected(error))
		return nullptr;

	if (!mpd_send_list_meta(connection, uri)) {
		CheckError(connection, error);
		return nullptr;
	}

	struct mpd_song *song = mpd_recv_song(connection);
	Song *song2 = song != nullptr
		? Convert(song)
		: nullptr;
	if (song != nullptr)
		mpd_song_free(song);
	if (!mpd_response_finish(connection)) {
		if (song2 != nullptr)
			song2->Free();

		CheckError(connection, error);
		return nullptr;
	}

	if (song2 == nullptr)
		error.Format(db_domain, DB_NOT_FOUND, "No such song: %s", uri);

	return song2;
}

void
ProxyDatabase::ReturnSong(Song *song) const
{
	assert(song != nullptr);
	assert(song->IsInDatabase());
	assert(song->IsDetached());

	song->Free();
}

static bool
Visit(struct mpd_connection *connection, const char *uri,
      bool recursive, const SongFilter *filter,
      VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist, Error &error);

static bool
Visit(struct mpd_connection *connection,
      bool recursive, const SongFilter *filter,
      const struct mpd_directory *directory,
      VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist, Error &error)
{
	const char *path = mpd_directory_get_path(directory);

	if (visit_directory) {
		Directory *d = Directory::NewGeneric(path, &detached_root);
		bool success = visit_directory(*d, error);
		d->Free();
		if (!success)
			return false;
	}

	if (recursive &&
	    !Visit(connection, path, recursive, filter,
		   visit_directory, visit_song, visit_playlist, error))
		return false;

	return true;
}

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

static Song *
Convert(const struct mpd_song *song)
{
	Song *s = Song::NewDetached(mpd_song_get_uri(song));

	s->mtime = mpd_song_get_last_modified(song);
	s->start_ms = mpd_song_get_start(song) * 1000;
	s->end_ms = mpd_song_get_end(song) * 1000;

	TagBuilder tag;
	tag.SetTime(mpd_song_get_duration(song));

	for (const auto *i = &tag_table[0]; i->d != TAG_NUM_OF_ITEM_TYPES; ++i)
		Copy(tag, i->d, song, i->s);

	s->tag = tag.CommitNew();

	return s;
}

gcc_pure
static bool
Match(const SongFilter *filter, const Song &song)
{
	return filter == nullptr || filter->Match(song);
}

static bool
Visit(const SongFilter *filter,
      const struct mpd_song *song,
      VisitSong visit_song, Error &error)
{
	if (!visit_song)
		return true;

	Song *s = Convert(song);
	bool success = !Match(filter, *s) || visit_song(*s, error);
	s->Free();

	return success;
}

static bool
Visit(const struct mpd_playlist *playlist,
      VisitPlaylist visit_playlist, Error &error)
{
	if (!visit_playlist)
		return true;

	PlaylistInfo p(mpd_playlist_get_path(playlist),
		       mpd_playlist_get_last_modified(playlist));

	return visit_playlist(p, detached_root, error);
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

static bool
Visit(struct mpd_connection *connection, const char *uri,
      bool recursive, const SongFilter *filter,
      VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist, Error &error)
{
	if (!mpd_send_list_meta(connection, uri))
		return CheckError(connection, error);

	std::list<ProxyEntity> entities(ReceiveEntities(connection));
	if (!CheckError(connection, error))
		return false;

	for (const auto &entity : entities) {
		switch (mpd_entity_get_type(entity)) {
		case MPD_ENTITY_TYPE_UNKNOWN:
			break;

		case MPD_ENTITY_TYPE_DIRECTORY:
			if (!Visit(connection, recursive, filter,
				   mpd_entity_get_directory(entity),
				   visit_directory, visit_song, visit_playlist,
				   error))
				return false;
			break;

		case MPD_ENTITY_TYPE_SONG:
			if (!Visit(filter,
				   mpd_entity_get_song(entity), visit_song,
				   error))
				return false;
			break;

		case MPD_ENTITY_TYPE_PLAYLIST:
			if (!Visit(mpd_entity_get_playlist(entity),
				   visit_playlist, error))
				return false;
			break;
		}
	}

	return CheckError(connection, error);
}

static bool
SearchSongs(struct mpd_connection *connection,
	    const DatabaseSelection &selection,
	    VisitSong visit_song,
	    Error &error)
{
	assert(selection.recursive);
	assert(visit_song);

	const bool exact = selection.filter == nullptr ||
		!selection.filter->HasFoldCase();

	if (!mpd_search_db_songs(connection, exact) ||
	    !SendConstraints(connection, selection) ||
	    !mpd_search_commit(connection))
		return CheckError(connection, error);

	bool result = true;
	struct mpd_song *song;
	while (result && (song = mpd_recv_song(connection)) != nullptr) {
		Song *song2 = Convert(song);
		mpd_song_free(song);

		result = !Match(selection.filter, *song2) ||
			visit_song(*song2, error);
		song2->Free();
	}

	mpd_response_finish(connection);
	return result && CheckError(connection, error);
}

bool
ProxyDatabase::Visit(const DatabaseSelection &selection,
		     VisitDirectory visit_directory,
		     VisitSong visit_song,
		     VisitPlaylist visit_playlist,
		     Error &error) const
{
	// TODO: eliminate the const_cast
	if (!const_cast<ProxyDatabase *>(this)->EnsureConnected(error))
		return nullptr;

	if (!visit_directory && !visit_playlist && selection.recursive)
		/* this optimized code path can only be used under
		   certain conditions */
		return ::SearchSongs(connection, selection, visit_song, error);

	/* fall back to recursive walk (slow!) */
	return ::Visit(connection, selection.uri.c_str(),
		       selection.recursive, selection.filter,
		       visit_directory, visit_song, visit_playlist,
		       error);
}

bool
ProxyDatabase::VisitUniqueTags(const DatabaseSelection &selection,
			       TagType tag_type,
			       VisitString visit_string,
			       Error &error) const
{
	// TODO: eliminate the const_cast
	if (!const_cast<ProxyDatabase *>(this)->EnsureConnected(error))
		return nullptr;

	enum mpd_tag_type tag_type2 = Convert(tag_type);
	if (tag_type2 == MPD_TAG_COUNT) {
		error.Set(libmpdclient_domain, "Unsupported tag");
		return false;
	}

	if (!mpd_search_db_tags(connection, tag_type2))
		return CheckError(connection, error);

	if (!SendConstraints(connection, selection))
		return CheckError(connection, error);

	if (!mpd_search_commit(connection))
		return CheckError(connection, error);

	bool result = true;

	struct mpd_pair *pair;
	while (result &&
	       (pair = mpd_recv_pair_tag(connection, tag_type2)) != nullptr) {
		result = visit_string(pair->value, error);
		mpd_return_pair(connection, pair);
	}

	return mpd_response_finish(connection) &&
		CheckError(connection, error) &&
		result;
}

bool
ProxyDatabase::GetStats(const DatabaseSelection &selection,
			DatabaseStats &stats, Error &error) const
{
	// TODO: match
	(void)selection;

	// TODO: eliminate the const_cast
	if (!const_cast<ProxyDatabase *>(this)->EnsureConnected(error))
		return nullptr;

	struct mpd_stats *stats2 =
		mpd_run_stats(connection);
	if (stats2 == nullptr)
		return CheckError(connection, error);

	update_stamp = (time_t)mpd_stats_get_db_update_time(stats2);

	stats.song_count = mpd_stats_get_number_of_songs(stats2);
	stats.total_duration = mpd_stats_get_db_play_time(stats2);
	stats.artist_count = mpd_stats_get_number_of_artists(stats2);
	stats.album_count = mpd_stats_get_number_of_albums(stats2);
	mpd_stats_free(stats2);

	return true;
}

const DatabasePlugin proxy_db_plugin = {
	"proxy",
	ProxyDatabase::Create,
};
